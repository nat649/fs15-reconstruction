// Independently authored metadata adapter for the recovered CareerScreen.
#include "savegames.h"
#include "xml_bridge.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <array>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace fs15menu {
namespace {

constexpr int slot_count = 20;
constexpr size_t max_metadata_bytes = 8 * 1024 * 1024;

// These stable host identifiers are shared with the Lua scripts by name. Their
// numeric correspondence with the original native engine has not been verified.
enum SaveError {
    error_ok = 0,
    error_data_corrupt = 1,
    error_scan_in_progress = 2,
    error_scan_failed = 3,
    error_device_unavailable = 4,
};

struct Slot {
    std::string metadata;
    std::string display_name;
};

struct Context {
    std::filesystem::path directory;
    std::string profile_path;
    std::array<Slot, slot_count> slots;
    int snapshot_error = error_ok;
    bool scanned = false;

    explicit Context(std::filesystem::path selected) : directory(std::move(selected)) {
        if (!directory.empty()) {
            profile_path = directory.generic_u8string();
            if (profile_path.back() != '/') profile_path += '/';
        }
        for (int index = 0; index < slot_count; ++index) {
            slots[index].display_name = "Savegame " + std::to_string(index + 1);
        }
    }

    void scan() {
        snapshot_error = error_ok;
        for (auto& slot : slots) slot.metadata.clear();
        scanned = true;
        if (directory.empty()) return;

        std::error_code ec;
        if (!std::filesystem::is_directory(directory, ec) || ec) {
            snapshot_error = error_scan_failed;
            return;
        }
        for (int index = 0; index < slot_count; ++index) {
            const auto filename = directory / ("savegame" + std::to_string(index + 1)) / "careerSavegame.xml";
            ec.clear();
            const bool present = std::filesystem::exists(filename, ec);
            if (!present && !ec) continue;
            try {
                if (ec || !std::filesystem::is_regular_file(filename)) {
                    throw std::runtime_error("Cannot read save metadata");
                }
                const auto length = std::filesystem::file_size(filename);
                if (!length || length > max_metadata_bytes) {
                    throw std::runtime_error("Save metadata exceeds the size limit or is empty");
                }
                std::ifstream file(filename, std::ios::binary);
                if (!file) throw std::runtime_error("Cannot open save metadata for reading");
                std::string metadata(static_cast<size_t>(length), '\0');
                file.read(metadata.data(), static_cast<std::streamsize>(metadata.size()));
                if (!file || file.peek() != std::char_traits<char>::eof()) {
                    throw std::runtime_error("Save metadata changed while reading");
                }
                validate_xml_memory(metadata, "careerSavegame");
                slots[index].metadata = std::move(metadata);
            } catch (const std::exception&) {
                // The recovered CareerScreen recognizes this sentinel. Raw user
                // metadata and profile filenames are never written to a log.
                slots[index].metadata = "corrupt";
                snapshot_error = error_data_corrupt;
            }
        }
    }
};

Context* context(lua_State* state) {
    return static_cast<Context*>(lua_touserdata(state, lua_upvalueindex(1)));
}

template<int (*Body)(lua_State*)>
int protected_call(lua_State* state) {
    try {
        return Body(state);
    } catch (const std::exception& error) {
        lua_pushstring(state, error.what());
    } catch (...) {
        lua_pushliteral(state, "Unexpected save metadata error");
    }
    return lua_error(state);
}

int slot_info(lua_State* state) {
    const lua_Integer checked_index = luaL_checkinteger(state, 1);
    if (checked_index < 1 || checked_index > slot_count) throw std::out_of_range("Savegame index must be between 1 and 20");
    const int index = static_cast<int>(checked_index);
    auto* selected = context(state);
    if (!selected->scanned) selected->scan();
    const auto& slot = selected->slots[static_cast<size_t>(index - 1)];
    lua_pushlstring(state, slot.metadata.data(), slot.metadata.size());
    lua_pushlstring(state, slot.display_name.data(), slot.display_name.size());
    return 2;
}

int invoke_callback(lua_State* state) {
    lua_pushvalue(state, 2);
    lua_gettable(state, 1);
    if (!lua_isfunction(state, -1)) return luaL_error(state, "Save metadata callback is not a target method");
    lua_pushvalue(state, 1);
    lua_pushvalue(state, 3);
    lua_call(state, 2, 0);
    return 0;
}

int update_list(lua_State* state) {
    const char* callback = luaL_checkstring(state, 1);
    luaL_checktype(state, 2, LUA_TTABLE);
    auto* selected = context(state);
    selected->scan();
    // Both the method lookup (including __index for recovered classes) and the
    // callback execute inside a protected Lua call. No C++ objects span it.
    lua_pushcfunction(state, invoke_callback);
    lua_pushvalue(state, 2);
    lua_pushstring(state, callback);
    lua_pushinteger(state, selected->snapshot_error);
    if (lua_pcall(state, 3, 0, 0) != 0) {
        const char* message = lua_tostring(state, -1);
        const std::string detail = message ? message : "non-string Lua error";
        lua_pop(state, 1);
        throw std::runtime_error("Save metadata callback failed: " + detail);
    }
    return 0;
}

int num_slots(lua_State* state) {
    lua_pushinteger(state, slot_count);
    return 1;
}

int profile_path(lua_State* state) {
    const auto& path = context(state)->profile_path;
    lua_pushlstring(state, path.data(), path.size());
    return 1;
}

int no_operation(lua_State*) {
    // Storage devices are fixed to the caller-selected directory. The menu host
    // has no physics scene, so neither reset nor flush has work to perform.
    return 0;
}

int no_write_space(lua_State* state) {
    lua_pushboolean(state, 0);
    return 1;
}

int reject_mutation(lua_State* state) {
    const char* name = lua_tostring(state, lua_upvalueindex(2));
    throw std::runtime_error(std::string(name ? name : "Save operation") +
                             " is unavailable: this menu reads save metadata only");
}

int snapshot_count(lua_State* state) {
    auto* selected = context(state);
    if (!selected->scanned) selected->scan();
    int populated = 0;
    int corrupt = 0;
    for (const auto& slot : selected->slots) {
        if (slot.metadata == "corrupt") ++corrupt;
        else if (!slot.metadata.empty()) ++populated;
    }
    lua_pushinteger(state, populated);
    lua_pushinteger(state, corrupt);
    return 2;
}

int destroy_context(lua_State* state) {
    static_cast<Context*>(lua_touserdata(state, 1))->~Context();
    return 0;
}

}  // namespace

void register_savegames(lua_State* state, const std::filesystem::path& directory) {
    if (!state) throw std::invalid_argument("Save metadata service requires a Lua state");
    new (lua_newuserdata(state, sizeof(Context))) Context(directory);
    if (luaL_newmetatable(state, "fs15menu.SaveContext")) {
        lua_pushcfunction(state, destroy_context);
        lua_setfield(state, -2, "__gc");
    }
    lua_setmetatable(state, -2);
    const int context_index = lua_gettop(state);
    const luaL_Reg functions[] = {
        {"saveGetInfoById", protected_call<slot_info>},
        {"saveUpdateList", protected_call<update_list>},
        {"saveGetNumOfSaveGames", protected_call<num_slots>},
        {"saveResetStorageDeviceSelection", no_operation},
        {"flushPhysicsCaches", no_operation},
        {"saveGetHasSpaceForSaveGame", no_write_space},
        {"getUserProfileAppPath", protected_call<profile_path>},
        {"fs15_save_snapshot_count", protected_call<snapshot_count>},
        {"saveDeleteSavegame", protected_call<reject_mutation>},
        {"saveWriteSavegameStart", protected_call<reject_mutation>},
        {"saveWriteSavegameFinish", protected_call<reject_mutation>},
        {nullptr, nullptr}
    };
    for (const auto* function = functions; function->name; ++function) {
        lua_pushvalue(state, context_index);
        lua_pushstring(state, function->name);
        lua_pushcclosure(state, function->func, 2);
        lua_setglobal(state, function->name);
    }
    lua_pop(state, 1);

    lua_newtable(state);
    const std::pair<const char*, int> constants[] = {
        {"ERROR_OK", error_ok},
        {"ERROR_DATA_CORRUPT", error_data_corrupt},
        {"ERROR_SCAN_IN_PROGRESS", error_scan_in_progress},
        {"ERROR_SCAN_FAILED", error_scan_failed},
        {"ERROR_DEVICE_UNAVAILABLE", error_device_unavailable}
    };
    for (const auto& constant : constants) {
        lua_pushinteger(state, constant.second);
        lua_setfield(state, -2, constant.first);
    }
    lua_setglobal(state, "Savegame");
}

}  // namespace fs15menu
