/*
 * Behavioral reconstruction of the FS15 launcher, NOT of the game engine.
 * Evidence: original FarmingSimulator2015.exe, SHA-256 recorded in analysis/pe/.
 * 0x00401860: XML startup.cmdline, x64/x86 prefix, forward arguments.
 * 0x004045e0: GetNativeSystemInfo/GetSystemInfo, architecture == AMD64 (9).
 * 0x004046e0: quote executable, CreateProcess, wait up to 5000 ms.
 * This source uses new C++17/Unicode/XML plumbing and does not claim binary parity.
 */
#include <windows.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

static void check(HRESULT result, const char* operation) {
    if (FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ")");
}

static std::wstring trim(const std::wstring& text) {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    return text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
}

static std::wstring read_cmdline(const std::string& xml) {
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()), static_cast<UINT>(xml.size())));
    if (!stream) throw std::runtime_error("Cannot create XML stream");
    ComPtr<IXmlReader> reader;
    check(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr), "CreateXmlReader");
    check(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit), "SetProperty");
    check(reader->SetInput(stream.Get()), "SetInput");
    XmlNodeType type = XmlNodeType_None;
    bool root_seen = false, in_cmdline = false, found = false;
    std::wstring command;
    HRESULT result;
    while ((result = reader->Read(&type)) == S_OK) {
        UINT depth = 0;
        check(reader->GetDepth(&depth), "GetDepth");
        const WCHAR* text = nullptr;
        UINT length = 0;
        if (type == XmlNodeType_Element) {
            check(reader->GetLocalName(&text, &length), "GetLocalName");
            const std::wstring name(text, length);
            if (depth == 0) {
                if (name != L"startup") throw std::runtime_error("Expected <startup> root");
                root_seen = true;
            } else if (depth == 1 && name == L"cmdline") {
                if (found) throw std::runtime_error("Duplicate <cmdline>");
                in_cmdline = true;
                found = true;
                if (reader->IsEmptyElement()) in_cmdline = false;
            } else if (in_cmdline) {
                throw std::runtime_error("Nested XML is not a command line");
            }
        } else if (type == XmlNodeType_EndElement && depth == 1) {
            in_cmdline = false;
        } else if (in_cmdline && (type == XmlNodeType_Text || type == XmlNodeType_CDATA || type == XmlNodeType_Whitespace)) {
            check(reader->GetValue(&text, &length), "GetValue");
            command.append(text, length);
        }
    }
    check(result, "Read XML");
    command = trim(command);
    if (!root_seen || !found || command.empty()) throw std::runtime_error("Missing startup.cmdline");
    return command;
}

static bool native_amd64() {
    SYSTEM_INFO info{};
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    using GetInfo = void(WINAPI*)(LPSYSTEM_INFO);
    const auto get_native = reinterpret_cast<GetInfo>(GetProcAddress(kernel, "GetNativeSystemInfo"));
    if (get_native) get_native(&info); else GetSystemInfo(&info);
    return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
}

// Encode one argument according to Windows argv parsing, including final backslashes.
static std::wstring quote(const std::wstring& text) {
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : text) {
        if (ch == L'\\') { ++backslashes; continue; }
        if (ch == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result += ch;
        } else {
            result.append(backslashes, L'\\');
            result += ch;
        }
        backslashes = 0;
    }
    result.append(backslashes * 2, L'\\');
    return result + L"\"";
}

struct Plan { fs::path executable; std::wstring arguments; };
static Plan make_plan(const fs::path& root, const std::wstring& startup, bool use_x64) {
    // The original launcher splits its configured command at the first space.
    const auto split = startup.find(L' ');
    const auto name = startup.substr(0, split);
    return {root / (use_x64 ? L"x64" : L"x86") / name,
            split == std::wstring::npos ? L"" : startup.substr(split + 1)};
}

static int self_test() {
    const std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?><startup><cmdline>FarmingSimulator2015Game.exe -name FarmingSimulator2015</cmdline></startup>";
    const auto command = read_cmdline(xml);
    const auto x64 = make_plan(L"C:\\Games\\FS15", command, true);
    const auto x86 = make_plan(L"C:\\Games\\FS15", command, false);
    auto require = [](bool passed, const char* name) { if (!passed) throw std::runtime_error(name); };
    require(x64.executable == fs::path(L"C:\\Games\\FS15\\x64\\FarmingSimulator2015Game.exe"), "x64 path");
    require(x86.executable == fs::path(L"C:\\Games\\FS15\\x86\\FarmingSimulator2015Game.exe"), "x86 path");
    require(x64.arguments == L"-name FarmingSimulator2015", "configured arguments");
    require(make_plan(L"C:\\FS15", L"game.exe", true).arguments.empty(), "no configured arguments");
    require(read_cmdline("<startup><cmdline>game.exe a&amp;b</cmdline></startup>") == L"game.exe a&b", "XML entities");
    require(quote(L"C:\\path with spaces\\") == L"\"C:\\path with spaces\\\\\"", "trailing slash quoting");
    require(quote(L"a\"b") == L"\"a\\\"b\"", "embedded quote");
    bool rejected = false;
    try { read_cmdline("<startup><cmdline>unterminated"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "malformed XML rejection");
    rejected = false;
    try { read_cmdline("<startup/>"); } catch (const std::exception&) { rejected = true; }
    require(rejected, "missing command rejection");
    std::wcout << L"9 launcher checks passed. Native AMD64: " << (native_amd64() ? L"yes" : L"no") << L"\n";
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) { std::cerr << "COM initialization failed\n"; return 1; }
    int exit_code = 0;
    try {
        fs::path root = LR"(C:\Program Files (x86)\Steam\steamapps\common\Farming Simulator 15)";
        const DWORD required = GetEnvironmentVariableW(L"FS15_GAME_DIR", nullptr, 0);
        if (required) {
            std::wstring configured(required, L'\0');
            const DWORD length = GetEnvironmentVariableW(L"FS15_GAME_DIR", configured.data(), required);
            if (length && length < required) { configured.resize(length); root = configured; }
        }
        bool dry_run = false, use_x64 = native_amd64();
        std::vector<std::wstring> extra;
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--self-test") { exit_code = self_test(); CoUninitialize(); return exit_code; }
            if (arg == L"--dry-run") dry_run = true;
            else if (arg == L"--game-dir" && i + 1 < argc) root = argv[++i];
            else if (arg == L"--arch" && i + 1 < argc) {
                const std::wstring arch = argv[++i];
                if (arch != L"x64" && arch != L"x86") throw std::runtime_error("--arch must be x64 or x86");
                use_x64 = arch == L"x64";
            } else if (arg == L"--") {
                while (++i < argc) extra.emplace_back(argv[i]);
                break;
            } else if (arg == L"--help") {
                std::wcout << L"FS15LauncherRebuilt [--game-dir PATH] [--arch x64|x86] [--dry-run] [-- game arguments]\nReconstructed launcher only; uses the original installed game.\n";
                CoUninitialize(); return 0;
            } else throw std::runtime_error("Unknown/incomplete option; use --help");
        }
        root = fs::absolute(root);
        std::ifstream file(root / L"FarmingSimulator2015.xml", std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open FarmingSimulator2015.xml in game directory");
        const std::string xml((std::istreambuf_iterator<char>(file)), {});
        const auto plan = make_plan(root, read_cmdline(xml), use_x64);
        if (!fs::is_regular_file(plan.executable)) throw std::runtime_error("Configured game executable does not exist");
        std::wstring cmdline = quote(plan.executable.wstring());
        if (!plan.arguments.empty()) cmdline += L" " + plan.arguments;
        for (const auto& arg : extra) cmdline += L" " + quote(arg);
        std::wcout << L"Reconstructed launcher; original game executable.\nWorking directory: " << root.wstring() << L"\nCommand: " << cmdline << L"\n";
        if (!dry_run) {
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(plan.executable.c_str(), cmdline.data(), nullptr, nullptr, FALSE,
                                NORMAL_PRIORITY_CLASS, nullptr, root.c_str(), &startup, &process)) {
                throw std::runtime_error("CreateProcessW failed: " + std::to_string(GetLastError()));
            }
            CloseHandle(process.hThread);
            WaitForSingleObject(process.hProcess, 5000);
            DWORD child_code = 0;
            GetExitCodeProcess(process.hProcess, &child_code);
            std::wcout << L"PID: " << process.dwProcessId << L"; status after 5s: " << child_code << L"\n";
            CloseHandle(process.hProcess);
            if (child_code != STILL_ACTIVE) exit_code = static_cast<int>(child_code);
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        exit_code = 1;
    }
    CoUninitialize();
    return exit_code;
}
