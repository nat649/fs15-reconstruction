#pragma once

#include <filesystem>

struct lua_State;

namespace fs15menu {

// Read-only snapshots of savegame1..savegame20/careerSavegame.xml. An empty
// directory deliberately yields twenty empty slots. This service never writes,
// deletes, creates, or executes files from the selected profile.
void register_savegames(lua_State* state, const std::filesystem::path& directory);

}  // namespace fs15menu
