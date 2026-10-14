// Authored fixtures exercise only the read-only host services, without game data.
#include "savegames.h"
#include "xml_bridge.h"

#include <windows.h>
#include <objbase.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using State = std::unique_ptr<lua_State, decltype(&lua_close)>;

void run(lua_State* state, const char* source) {
    if (luaL_dostring(state, source) != 0) {
        const char* detail = lua_tostring(state, -1);
        throw std::runtime_error(detail ? detail : "Lua test failed");
    }
}

int delete_document(lua_State* state) {
    fs15menu::delete_xml(static_cast<int>(luaL_checkinteger(state, 1)));
    return 0;
}

State create_state(const std::filesystem::path& directory) {
    State state(luaL_newstate(), lua_close);
    if (!state) throw std::runtime_error("Cannot allocate Lua fixture state");
    luaL_openlibs(state.get());
    fs15menu::register_xml(state.get(), [](const std::string& path) { return std::filesystem::u8path(path); });
    fs15menu::register_savegames(state.get(), directory);
    lua_pushcfunction(state.get(), delete_document);
    lua_setglobal(state.get(), "delete");
    return state;
}

void write(const std::filesystem::path& file, const std::string& contents) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!stream) throw std::runtime_error("Cannot create authored metadata fixture");
}

std::string read(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Fixtures {
    std::filesystem::path parent;
    std::filesystem::path root;

    static bool ordinary_directory(const std::filesystem::path& path) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
               (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    explicit Fixtures(const std::filesystem::path& build_directory) {
        parent = std::filesystem::absolute(build_directory).lexically_normal();
        if (!parent.is_absolute()) throw std::runtime_error("Fixture parent must resolve to an absolute path");
        std::filesystem::create_directories(parent);
        if (!ordinary_directory(parent)) throw std::runtime_error("Fixture parent cannot be a reparse point");
        root = parent / ("menu-metadata-fixture-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        if (!std::filesystem::create_directory(root)) throw std::runtime_error("Fixture directory already exists");
        if (root.parent_path() != parent || !ordinary_directory(root)) {
            throw std::runtime_error("Fixture directory is outside its verified parent");
        }
    }

    ~Fixtures() {
        // Never recursively remove an unchecked or caller-supplied path.
        const auto resolved = std::filesystem::absolute(root).lexically_normal();
        if (resolved.parent_path() == parent && ordinary_directory(parent) && ordinary_directory(resolved) &&
            resolved.filename().u8string().find("menu-metadata-fixture-") == 0) {
            std::error_code ignored;
            std::filesystem::remove_all(resolved, ignored);
        }
    }
};

void empty_slots() {
    auto state = create_state({});
    run(state.get(), R"lua(
        assert(saveGetNumOfSaveGames() == 20)
        assert(getUserProfileAppPath() == '')
        for i = 1, 20 do
            local metadata, name = saveGetInfoById(i)
            assert(metadata == '' and name == 'Savegame ' .. i)
        end
        assert(not pcall(saveGetInfoById, 0))
        assert(not pcall(saveGetInfoById, 21))
        assert(not pcall(saveGetInfoById, 4294967297))
        assert(not saveGetHasSpaceForSaveGame(1, 100))
        saveResetStorageDeviceSelection()
        flushPhysicsCaches()
        local seen = 0
        local methods = { completed = function(self, code)
            assert(code == Savegame.ERROR_OK)
            seen = seen + 1
        end }
        local target = setmetatable({}, {__index = methods})
        saveUpdateList('completed', target)
        assert(seen == 1)
        assert(not pcall(saveUpdateList, 'missing', target))
        local bad = {completed = function() error('authored callback failure') end}
        local ok, message = pcall(saveUpdateList, 'completed', bad)
        assert(not ok and message:find('authored callback failure', 1, true))
        local badLookup = setmetatable({}, {__index = function() error('authored lookup failure') end})
        ok, message = pcall(saveUpdateList, 'completed', badLookup)
        assert(not ok and message:find('authored lookup failure', 1, true))
        for _, name in ipairs({'saveDeleteSavegame', 'saveWriteSavegameStart', 'saveWriteSavegameFinish'}) do
            ok, message = pcall(_G[name], 1)
            assert(not ok and message:find('reads save metadata only', 1, true))
        end
        assert(fs15_save_snapshot_count() == 0)
    )lua");
}

void memory_xml() {
    auto state = create_state({});
    run(state.get(), R"lua(
        local xml = loadXMLFileFromMemory('authored', '<careerSavegame revision="2" valid="true" money="1200"><mod title="A &amp; B"/><mod title="second"/></careerSavegame>')
        assert(getXMLInt(xml, 'careerSavegame#revision') == 2)
        assert(getXMLFloat(xml, 'careerSavegame#money') == 1200)
        assert(getXMLBool(xml, 'careerSavegame#valid') == true)
        assert(getXMLString(xml, 'careerSavegame.mod(0)#title') == 'A & B')
        assert(getXMLString(xml, 'careerSavegame.mod(1)#title') == 'second')
        assert(not hasXMLProperty(xml, 'careerSavegame.mod(2)'))
        assert(not pcall(loadXMLFileFromMemory, 'empty', ''))
        assert(not pcall(loadXMLFileFromMemory, 'broken', '<careerSavegame>'))
        assert(not pcall(loadXMLFileFromMemory, 'roots', '<a/><b/>'))
        assert(not pcall(loadXMLFileFromMemory, 'dtd', '<!DOCTYPE x [<!ENTITY v "blocked">]><x>&v;</x>'))
        assert(not pcall(loadXMLFileFromMemory, 'external', '<!DOCTYPE x SYSTEM "file:///authored-does-not-exist"><x/>'))
        assert(not pcall(loadXMLFileFromMemory, 'large', string.rep('x', 8 * 1024 * 1024 + 1)))
        delete(xml)
        assert(not hasXMLProperty(xml, 'careerSavegame'))
        -- Leave one live handle to verify that closing the Lua state frees it.
        assert(loadXMLFileFromMemory('owned', '<owned/>'))
    )lua");
    if (fs15menu::xml_document_count() != 1) throw std::runtime_error("XML explicit deletion failed");
    state.reset();
    if (fs15menu::xml_document_count() != 0) throw std::runtime_error("Lua state did not release XML documents");
}

void metadata_files(const std::filesystem::path& root) {
    const std::string valid = "<careerSavegame revision=\"2\" valid=\"true\" mapId=\"Map01\" money=\"1200\"/>";
    const auto first = root / "savegame1/careerSavegame.xml";
    write(first, valid);
    write(root / "savegame3/careerSavegame.xml", "<careerSavegame>");
    write(root / "savegame4/careerSavegame.xml", "<!DOCTYPE careerSavegame><careerSavegame/>");
    write(root / "savegame5/careerSavegame.xml", "<unexpectedRoot/>");
    write(root / "savegame6/careerSavegame.xml", "");
    write(root / "savegame7/careerSavegame.xml", std::string(8 * 1024 * 1024 + 1, 'x'));
    auto state = create_state(root);
    run(state.get(), R"lua(
        assert(getUserProfileAppPath():sub(-1) == '/')
        local target = {complete = function(self, code) self.code = code end}
        saveUpdateList('complete', target)
        assert(target.code == Savegame.ERROR_DATA_CORRUPT)
        local populated, corrupt = fs15_save_snapshot_count()
        assert(populated == 1 and corrupt == 5)
        assert(saveGetInfoById(2) == '')
        for i = 3, 7 do assert(saveGetInfoById(i) == 'corrupt') end
        local metadata = saveGetInfoById(1)
        local xml = loadXMLFileFromMemory('savefixture', metadata)
        assert(getXMLInt(xml, 'careerSavegame#money') == 1200)
        assert(not pcall(saveDeleteSavegame, 1, 'complete', target))
        assert(saveGetInfoById(1) == metadata)
    )lua");
    if (read(first) != valid) throw std::runtime_error("Metadata service modified an authored fixture");
    write(first, "<careerSavegame money=\"2400\"/>");
    run(state.get(), R"lua(
        local target = {complete = function() end}
        saveUpdateList('complete', target)
        local xml = loadXMLFileFromMemory('refreshed', saveGetInfoById(1))
        assert(getXMLInt(xml, 'careerSavegame#money') == 2400)
    )lua");
    state.reset();
    if (fs15menu::xml_document_count() != 0) throw std::runtime_error("Save XML leaked after closing state");

    auto missing = create_state(root / "authored-missing-directory");
    run(missing.get(), R"lua(
        local target = {complete = function(self, code) self.code = code end}
        saveUpdateList('complete', target)
        assert(target.code == Savegame.ERROR_SCAN_FAILED)
        for i = 1, 20 do assert(saveGetInfoById(i) == '') end
    )lua");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized)) return 2;
    int result = 0;
    try {
        if (argc != 2) throw std::runtime_error("Expected build fixture directory");
        Fixtures fixtures(argv[1]);
        empty_slots();
        memory_xml();
        metadata_files(fixtures.root);
        std::cout << "Read-only save metadata and secured memory XML fixtures passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    CoUninitialize();
    return result;
}
