// Independently built GUI host for recovered FS15 menu Lua. No original engine
// code is loaded. This milestone intentionally has no world/physics runtime.
#include <windows.h>
#include <objbase.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "xml_bridge.h"
#include "render.h"
#include "savegames.h"

namespace fs = std::filesystem;
static constexpr int screen_width = 1280, screen_height = 720;
static fs::path resource_root;
static fs::path save_directory, capture_directory;
static std::ofstream runtime_log;
static lua_State* vm = nullptr;
static std::unique_ptr<fs15menu::Renderer> renderer;
static bool exit_requested = false, failed = false, smoke_mode = false;
static HWND menu_window = nullptr;
static std::vector<std::string> loaded_scripts;
static std::set<std::string> loaded_assets;
static std::string fatal_error;
static size_t frame_count = 0, unavailable_count = 0;
static void draw(HDC dc = nullptr);

static void log_line(const std::string& message) {
    runtime_log << message << '\n';
    runtime_log.flush();
    OutputDebugStringA((message + "\n").c_str());
}

static std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) return L"Invalid UTF-8 message";
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}

static fs::path relative_resource(const std::string& name) {
    std::string normalized = name;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized.empty() || normalized.find(':') != std::string::npos || normalized.front() == '/')
        throw std::runtime_error("Invalid resource path: " + name);
    fs::path relative = fs::u8path(normalized);
    for (const auto& part : relative) {
        if (part == L".." || part == L".") throw std::runtime_error("Invalid resource traversal: " + name);
    }
    return relative;
}

static fs::path resolve_asset(const std::string& name) {
    const auto relative = relative_resource(name);
    // Engine filenames use .png while the local archive contains .dds textures.
    for (const auto& base : {resource_root / "assets", resource_root / "recovered/gar"}) {
        auto candidate = base / relative;
        if (fs::is_regular_file(candidate)) {
            const auto asset = candidate.lexically_relative(base).generic_u8string();
            if (loaded_assets.insert(asset).second) log_line("ASSET " + asset);
            return candidate;
        }
        if (candidate.extension() == L".png") {
            candidate.replace_extension(L".dds");
            if (fs::is_regular_file(candidate)) {
                const auto asset = candidate.lexically_relative(base).generic_u8string();
                if (loaded_assets.insert(asset).second) log_line("ASSET " + asset);
                return candidate;
            }
        }
    }
    throw std::runtime_error("Missing recovered asset: " + name + ". Run GAR extraction and package-menu.ps1 first.");
}

static fs::path resolve_script(const std::string& name) {
    const auto relative = relative_resource(name);
    if (relative.extension() != L".lua") throw std::runtime_error("Menu source must be recovered .lua: " + name);
    for (const auto& base : {resource_root / "lua", resource_root / "recovered/lua"}) {
        const auto candidate = base / relative;
        if (fs::is_regular_file(candidate)) return candidate;
    }
    throw std::runtime_error("Missing recovered Lua source: " + name + ". Run script recovery first.");
}

static int traceback(lua_State* state) {
    luaL_traceback(state, state, lua_tostring(state, 1), 1);
    return 1;
}

static void call_checked(int arguments, int results = 0) {
    const int function_index = lua_gettop(vm) - arguments;
    lua_pushcfunction(vm, traceback);
    lua_insert(vm, function_index);
    const int status = lua_pcall(vm, arguments, results, function_index);
    lua_remove(vm, function_index);
    if (status) {
        const char* message = lua_tostring(vm, -1);
        std::string error = message ? message : "Unknown Lua error";
        lua_pop(vm, 1);
        throw std::runtime_error(error);
    }
}

static void execute_file(const fs::path& path, const std::string& chunk_name) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + chunk_name);
    const std::string body((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (body.empty() || static_cast<unsigned char>(body.front()) == 0x1b)
        throw std::runtime_error("Refusing compiled/original bytecode as recovered source: " + chunk_name);
    if (luaL_loadbufferx(vm, body.data(), body.size(), ("@" + chunk_name).c_str(), "t")) {
        std::string error = lua_tostring(vm, -1);
        lua_pop(vm, 1);
        throw std::runtime_error(error);
    }
    call_checked(0);
}

static int source(lua_State* state) {
    const char* name = luaL_checkstring(state, 1);
    bool error = false;
    {
        try {
            execute_file(resolve_script(name), name);
            loaded_scripts.emplace_back(name);
            log_line("SOURCE " + std::string(name));
        } catch (const std::exception& exception) {
            lua_pushstring(state, exception.what());
            error = true;
        }
    }
    return error ? lua_error(state) : 0;
}

static int adapter_source(lua_State* state) {
    const char* name = luaL_checkstring(state, 1);
    bool error = false;
    {
        try {
            if (std::string(name) != "career.lua" && std::string(name) != "newgame.lua")
                throw std::runtime_error("Unknown authored menu module");
            const auto local = resource_root / name;
            execute_file(fs::is_regular_file(local) ? local : resource_root / "src/menu" / name,
                "fs15-menu/" + std::string(name));
            log_line("ADAPTER " + std::string(name));
        } catch (const std::exception& exception) { lua_pushstring(state, exception.what()); error = true; }
    }
    return error ? lua_error(state) : 0;
}

static int capture_screen(lua_State* state) {
    const char* name = luaL_checkstring(state, 1);
    bool error = false;
    {
        try {
            const std::string label = name;
            if (label.empty() || label.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
                throw std::runtime_error("Invalid menu capture label");
            draw();
            if (!capture_directory.empty()) {
                fs::create_directories(capture_directory);
                renderer->save_png(capture_directory / (label + ".png"));
                log_line("CAPTURE " + label);
            }
        } catch (const std::exception& exception) { lua_pushstring(state, exception.what()); error = true; }
    }
    return error ? lua_error(state) : 0;
}

static int print(lua_State* state) {
    std::string message;
    for (int i = 1; i <= lua_gettop(state); ++i) {
        if (i > 1) message += '\t';
        const char* item = lua_tostring(state, i);
        message += item ? item : lua_typename(state, lua_type(state, i));
    }
    log_line("LUA " + message);
    return 0;
}

static int file_exists(lua_State* state) {
    bool exists = false;
    try { resolve_asset(luaL_checkstring(state, 1)); exists = true; }
    catch (const std::exception&) { }
    lua_pushboolean(state, exists);
    return 1;
}

static int delete_resource(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    if (!fs15menu::delete_xml(handle) && renderer) renderer->delete_overlay(handle);
    return 0;
}

static int exit_menu(lua_State*) {
    exit_requested = true;
    log_line("EXIT requested by recovered menu callback");
    return 0;
}

static int menu_unavailable(lua_State* state) {
    const char* name = luaL_checkstring(state, 1);
    ++unavailable_count;
    log_line("UNAVAILABLE screen=" + std::string(name));
    if (!smoke_mode) {
        const auto message = wide("The recovered interface requested " + std::string(name) + ".\n\nThis milestone rebuilds the game frontend. Game worlds and multiplayer still need an engine implementation.");
        MessageBoxW(menu_window, message.c_str(), L"FS15 reconstruction progress", MB_OK | MB_ICONINFORMATION);
    }
    return 0;
}

static int get_screen_info(lua_State* state) {
    lua_pushinteger(state, screen_width);
    lua_pushinteger(state, screen_height);
    lua_pushinteger(state, 60);
    return 3;
}
static std::wstring quote_argument(const std::wstring& value) {
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t ch:value){if(ch==L'\\'){++slashes;continue;}
        out.append(slashes*(ch==L'\"'?2:1)+(ch==L'\"'?1:0),L'\\');out+=ch;slashes=0;}
    out.append(slashes*2,L'\\');return out+L"\"";
}
static int launch_world(lua_State* state) {
    const char* map=luaL_checkstring(state,1);
    const auto difficulty=luaL_checkinteger(state,2),slot=luaL_checkinteger(state,3);
    bool error=false;
    {
        try {
            const std::string id=map;
            if((id!="Map01"&&id!="Map02")||difficulty<1||difficulty>3||slot<1||slot>20)
                throw std::runtime_error("Invalid terrain launch request");
            const std::string stem=id=="Map01"?"map01":"map02",dem=id=="Map01"?"map01_dem.png":"americanMap_dem.png";
            resolve_asset("data/maps/"+stem+".i3d");resolve_asset("data/maps/"+stem+"/"+dem);resolve_asset("data/maps/"+stem+"/pda_map_H.dds");
            auto executable=resource_root/"FS15EngineRebuilt.exe";
            if(!fs::is_regular_file(executable))executable=resource_root/"build/Release/FS15EngineRebuilt.exe";
            if(!fs::is_regular_file(executable))throw std::runtime_error("Build FS15EngineRebuilt before opening a terrain");
            if(smoke_mode) {
                // GUI tests check this plan; native tests rasterize both maps.
                log_line("WORLD_LAUNCH_PLAN map="+id+" mode=original-map-scene");
            }else {
                auto command=quote_argument(executable.wstring())+L" --root "+quote_argument(resource_root.wstring())+L" --map "+wide(id)+L" --play --difficulty "+std::to_wstring(difficulty);
                STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
                if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,resource_root.c_str(),&startup,&process))
                    throw std::runtime_error("Cannot launch terrain runtime: "+std::to_string(GetLastError()));
                CloseHandle(process.hThread);CloseHandle(process.hProcess);
                log_line("WORLD_PROCESS_STARTED map="+id+" mode=original-map-scene");
            }
            lua_pushboolean(state,1);
        }catch(const std::exception& exception){lua_pushstring(state,exception.what());error=true;}
    }
    return error?lua_error(state):1;
}
static int get_screen_mode(lua_State* state) { lua_pushinteger(state, 0); return 1; }
static int num_gamepads(lua_State* state) { lua_pushinteger(state, 0); return 1; }
static int upper_utf8(lua_State* state) {
    const char* input = luaL_checkstring(state, 1);
    bool error = false;
    {
        try {
            const auto text = wide(input);
            if (text.empty()) { lua_pushliteral(state, ""); return 1; }
            const int length = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
            if (!length) throw std::runtime_error("Cannot uppercase UTF-8 menu text");
            std::wstring uppercase(length, L'\0');
            LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, text.data(), static_cast<int>(text.size()), uppercase.data(), length, nullptr, nullptr, 0);
            const int size = WideCharToMultiByte(CP_UTF8, 0, uppercase.data(), length, nullptr, 0, nullptr, nullptr);
            std::string result(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, uppercase.data(), length, result.data(), size, nullptr, nullptr);
            lua_pushlstring(state, result.data(), result.size());
        } catch (const std::exception& exception) { lua_pushstring(state, exception.what()); error = true; }
    }
    return error ? lua_error(state) : 1;
}

static void global_number(const char* name, double value) { lua_pushnumber(vm, value); lua_setglobal(vm, name); }
static void global_string(const char* name, const std::string& value) { lua_pushlstring(vm, value.data(), value.size()); lua_setglobal(vm, name); }

static void boot(const std::string& language) {
    vm = luaL_newstate();
    if (!vm) throw std::runtime_error("Cannot create LuaJIT VM");
    luaL_openlibs(vm);
    lua_register(vm, "source", source);
    lua_register(vm, "menuSource", adapter_source);
    lua_register(vm, "menuCapture", capture_screen);
    lua_register(vm, "print", print);
    lua_register(vm, "fileExists", file_exists);
    lua_register(vm, "delete", delete_resource);
    lua_register(vm, "doExit", exit_menu);
    lua_register(vm, "menuUnavailable", menu_unavailable);
    lua_register(vm, "menuLaunchWorld", launch_world);
    lua_register(vm, "getScreenModeInfo", get_screen_info);
    lua_register(vm, "getScreenMode", get_screen_mode);
    lua_register(vm, "getNumOfGamepads", num_gamepads);
    lua_register(vm, "utf8ToUpper", upper_utf8);
    global_number("g_screenWidth", screen_width);
    global_number("g_screenHeight", screen_height);
    global_number("g_screenAspectRatio", double(screen_width) / screen_height);
    global_string("g_languageShort", language);
    global_string("g_languageSuffix", "_" + language);
    fs15menu::register_xml(vm, resolve_asset);
    fs15menu::register_savegames(vm, save_directory);
    renderer = std::make_unique<fs15menu::Renderer>(resolve_asset, screen_width, screen_height);
    renderer->register_lua(vm);
    const auto packaged = resource_root / "bootstrap.lua";
    execute_file(fs::is_regular_file(packaged) ? packaged : resource_root / "src/menu/bootstrap.lua", "fs15-menu/bootstrap.lua");
    lua_getglobal(vm, "fs15_menu_selfcheck");
    call_checked(0);
    log_line("BOOT_OK recovered Lua MainScreen opened; original game engine not loaded");
}

static void update() {
    lua_getglobal(vm, "fs15_menu_update");
    lua_pushnumber(vm, 16);
    call_checked(1);
    if (exit_requested && menu_window) PostMessageW(menu_window, WM_CLOSE, 0, 0);
}

static void draw(HDC dc) {
    renderer->begin_frame();
    lua_getglobal(vm, "fs15_menu_draw");
    call_checked(0);
    if (dc) renderer->present(dc);
    ++frame_count;
}

static void mouse(int x, int y, bool down, bool up, int button = 1) {
    lua_getglobal(vm, "fs15_menu_mouse");
    lua_pushnumber(vm, double(x) / screen_width);
    lua_pushnumber(vm, 1.0 - double(y) / screen_height);
    lua_pushboolean(vm, down);
    lua_pushboolean(vm, up);
    lua_pushinteger(vm, button);
    call_checked(5);
}

static void report_failure(const std::exception& exception) {
    if (failed) return;
    failed = true;
    fatal_error = exception.what();
    log_line("FATAL " + fatal_error);
    if (!smoke_mode) MessageBoxW(menu_window, wide(fatal_error).c_str(), L"FS15 menu startup error", MB_OK | MB_ICONERROR);
}

static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    try {
        switch (message) {
        case WM_CREATE:
            SetTimer(window, 1, 16, nullptr);
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER:
            update();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            const auto dc = BeginPaint(window, &paint);
            try { draw(dc); } catch (...) { EndPaint(window, &paint); throw; }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_MOUSEMOVE: mouse(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), false, false); return 0;
        case WM_LBUTTONDOWN: SetCapture(window); mouse(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), true, false); return 0;
        case WM_LBUTTONUP: ReleaseCapture(); mouse(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), false, true); return 0;
        case WM_MOUSEWHEEL: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(window, &point);
            const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
            const int wheel_button = delta > 0 ? 4 : 5;
            const int steps = std::max(1, std::abs(delta) / WHEEL_DELTA);
            for (int i = 0; i < steps; ++i) {
                mouse(point.x, point.y, true, false, wheel_button);
                mouse(point.x, point.y, false, true, wheel_button);
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_KEYUP:
            lua_getglobal(vm, "fs15_menu_key");
            lua_pushinteger(vm, static_cast<int>(wparam));
            lua_pushboolean(vm, message == WM_KEYDOWN);
            call_checked(2);
            return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception& exception) {
        report_failure(exception);
        PostMessageW(window, WM_CLOSE, 0, 0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static fs::path executable_directory() {
    wchar_t buffer[32768];
    const auto size = GetModuleFileNameW(nullptr, buffer, 32768);
    if (!size || size >= 32768) throw std::runtime_error("Cannot locate menu executable");
    return fs::path(std::wstring(buffer, size)).parent_path();
}

static fs::path default_save_directory() {
    PWSTR documents = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DONT_VERIFY, nullptr, &documents))) return {};
    const auto directory = fs::path(documents) / "My Games/FarmingSimulator2015";
    CoTaskMemFree(documents);
    return fs::is_directory(directory) ? directory : fs::path{};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    fs::path screenshot;
    int result = 0;
    try {
        resource_root = executable_directory();
        if (!fs::exists(resource_root / "bootstrap.lua") && !fs::exists(resource_root / "src/menu/bootstrap.lua")) {
            auto parent = resource_root.parent_path().parent_path();
            if (fs::exists(parent / "src/menu/bootstrap.lua")) resource_root = parent;
        }
        std::string language = "en";
        std::wstring start_screen = L"MainScreen";
        bool test_inputs = false, save_directory_explicit = false;
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!arguments) throw std::runtime_error("Cannot parse menu arguments");
        for (int i = 1; i < count; ++i) {
            const std::wstring argument = arguments[i];
            if (argument == L"--root" && i + 1 < count) resource_root = fs::absolute(arguments[++i]);
            else if (argument == L"--smoke") smoke_mode = true;
            else if (argument == L"--test-inputs") test_inputs = true;
            else if (argument == L"--save-dir" && i + 1 < count) { save_directory = fs::absolute(arguments[++i]); save_directory_explicit = true; }
            else if (argument == L"--empty-saves") { save_directory.clear(); save_directory_explicit = true; }
            else if (argument == L"--capture-dir" && i + 1 < count) capture_directory = fs::absolute(arguments[++i]);
            else if (argument == L"--screen" && i + 1 < count) start_screen = arguments[++i];
            else if (argument == L"--screenshot" && i + 1 < count) screenshot = fs::absolute(arguments[++i]);
            else throw std::runtime_error("Usage: FS15MenuRebuilt.exe [--root PATH] [--smoke] [--test-inputs] [--screen MainScreen|CareerScreen|DifficultyScreen|MapSelectionScreen|CreditsScreen] [--save-dir PATH|--empty-saves] [--capture-dir PATH] [--screenshot PATH]");
        }
        LocalFree(arguments);
        if (!save_directory_explicit && !smoke_mode) save_directory = default_save_directory();
        fs::create_directories(resource_root / "logs");
        runtime_log.open(resource_root / "logs/menu-runtime.log", std::ios::trunc);
        if (!runtime_log) throw std::runtime_error("Cannot write menu-runtime.log");
        log_line("FS15 independently compiled menu host; recovered source execution; 1280x720");
        log_line(std::string("SAVE_SERVICE read_only=true profile=") + (save_directory.empty() ? "empty-slots" : "local-profile"));
        boot(language);
        auto show_start_screen = [&]() {
            if (start_screen != L"MainScreen") {
                const std::vector<std::wstring> allowed{L"CareerScreen", L"DifficultyScreen", L"MapSelectionScreen", L"CreditsScreen"};
                if (std::find(allowed.begin(), allowed.end(), start_screen) == allowed.end()) throw std::runtime_error("Unknown frontend screen");
                lua_getglobal(vm, "fs15_menu_show");
                std::string name;
                for (wchar_t character : start_screen) name.push_back(static_cast<char>(character));
                lua_pushlstring(vm, name.data(), name.size());
                call_checked(1);
            }
        };
        if (smoke_mode) {
            if (!test_inputs) show_start_screen();
            for (int i = 0; i < 120; ++i) { update(); draw(); }
            if (test_inputs) {
                lua_getglobal(vm, "fs15_menu_test_inputs");
                call_checked(0);
                if (!exit_requested) throw std::runtime_error("Recovered Quit callback did not request exit");
                if (!unavailable_count) throw std::runtime_error("Unavailable engine boundary was not exercised");
                log_line("INPUT_CHECK_OK actual recovered button handlers exercised");
            }
            if (test_inputs) { show_start_screen(); draw(); }
            if (renderer->missing_asset_count()) throw std::runtime_error("Missing assets in rendered menu");
            if (!screenshot.empty()) renderer->save_png(screenshot);
            log_line("SMOKE_OK frames=" + std::to_string(frame_count) + " scripts=" + std::to_string(loaded_scripts.size()) + " overlays=" + std::to_string(renderer->overlay_count()));
        } else {
            show_start_screen();
            WNDCLASSW window_class{};
            window_class.lpfnWndProc = window_proc;
            window_class.hInstance = instance;
            window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            window_class.lpszClassName = L"FS15ReconstructedMenuHost";
            if (!RegisterClassW(&window_class)) throw std::runtime_error("Cannot register menu window");
            const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
            RECT rectangle{0, 0, screen_width, screen_height};
            AdjustWindowRect(&rectangle, style, FALSE);
            menu_window = CreateWindowW(window_class.lpszClassName, L"Farming Simulator 15 - Reconstructed Frontend", style,
                CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
                nullptr, nullptr, instance, nullptr);
            if (!menu_window) throw std::runtime_error("Cannot create menu window");
            ShowWindow(menu_window, show);
            UpdateWindow(menu_window);
            log_line("WINDOW_OPEN main menu");
            if (!screenshot.empty()) { draw(); renderer->save_png(screenshot); }
            MSG message{};
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (failed) result = 1;
        }
    } catch (const std::exception& exception) {
        report_failure(exception);
        result = 1;
    }
    log_line("SHUTDOWN exit=" + std::to_string(result) + " frames=" + std::to_string(frame_count));
    // Lua finalizers must run while the overlay service is still alive.
    if (vm) { lua_close(vm); vm = nullptr; }
    renderer.reset();
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}
