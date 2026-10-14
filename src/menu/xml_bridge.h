#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

struct lua_State;

namespace fs15menu {

using ResourceResolver = std::function<std::filesystem::path(const std::string&)>;

// Register the read-only XML calls used by the recovered GUI and localization
// scripts. The resolver maps virtual names (for example dataS/gui/MainScreen.xml)
// to local files. The caller initializes COM and owns the global delete() call.
// Closing the Lua state also frees XML documents loaded by its registered bridge.
void register_xml(lua_State* state, ResourceResolver resolver);
bool delete_xml(int handle);
int xml_document_count();

// Validate metadata through the same DTD-prohibited, bounded DOM reader used by
// the Lua XML bridge. Throws on malformed input or an unexpected root name.
void validate_xml_memory(std::string_view xml, std::string_view expected_root = {});

}  // namespace fs15menu
