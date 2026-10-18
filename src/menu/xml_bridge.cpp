// Independently authored read-only XML adapter for the reconstructed menu host.
// This does not load a game executable, resolve its imports, or write game files.
#include "xml_bridge.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <wrl/client.h>
#else
#include <expat.h>
#include <exception>
#include <fstream>
#endif

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs15menu {
namespace {

#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif

struct Node {
    std::string name;
    std::string text;
    std::unordered_map<std::string, std::string> attributes;
    std::vector<std::unique_ptr<Node>> children;
};

// The menu runs Lua and host callbacks on its UI thread. Handles are globally
// distinct so the host can implement delete(handle) without a Lua state argument.
std::unordered_map<int, std::unique_ptr<Node>> documents;
int next_handle = 0x100000;
constexpr size_t max_xml_bytes = 8 * 1024 * 1024;

struct Context {
    ResourceResolver resolve;
    std::unordered_set<int> owned;

    ~Context() {
        for (int handle : owned) documents.erase(handle);
    }
};

#ifdef _WIN32
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    }
}

std::string utf8(const WCHAR* value, UINT length) {
    if (!length) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                                         static_cast<int>(length), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("XML contains invalid Unicode");
    std::string result(static_cast<size_t>(size), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value,
                            static_cast<int>(length), result.data(), size, nullptr, nullptr)) {
        throw std::runtime_error("Cannot convert XML text to UTF-8");
    }
    return result;
}

std::string node_name(IXmlReader* reader) {
    const WCHAR* value = nullptr;
    UINT length = 0;
    check(reader->GetQualifiedName(&value, &length), "Read XML name");
    return utf8(value, length);
}

std::string node_value(IXmlReader* reader) {
    const WCHAR* value = nullptr;
    UINT length = 0;
    check(reader->GetValue(&value, &length), "Read XML value");
    return utf8(value, length);
}

std::unique_ptr<Node> parse_xml_stream(IStream* stream) {
    STATSTG details{};
    check(stream->Stat(&details, STATFLAG_NONAME), "Read XML stream size");
    if (details.cbSize.QuadPart > max_xml_bytes) throw std::runtime_error("XML exceeds the 8 MiB limit");
    ComPtr<IXmlReader> reader;
    check(CreateXmlReader(__uuidof(IXmlReader),
                          reinterpret_cast<void**>(reader.GetAddressOf()), nullptr), "Create XML reader");
    // Prohibit every DTD, including internal declarations. No entity resolver is
    // installed; external paths and network entities cannot be requested.
    check(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit), "Prohibit XML DTD");
    check(reader->SetProperty(XmlReaderProperty_MaxElementDepth, 512), "Set XML depth limit");
    check(reader->SetInput(stream), "Set XML input");

    std::unique_ptr<Node> root;
    std::vector<Node*> stack;
    size_t count = 0;
    XmlNodeType type = XmlNodeType_None;
    HRESULT result;
    while ((result = reader->Read(&type)) == S_OK) {
        if (type == XmlNodeType_Element) {
            if (++count > 200000) throw std::runtime_error("XML has too many elements");
            auto node = std::make_unique<Node>();
            node->name = node_name(reader.Get());
            const bool empty = reader->IsEmptyElement() != FALSE;
            const HRESULT first = reader->MoveToFirstAttribute();
            check(first, "Read XML attributes");
            if (first == S_OK) {
                HRESULT attribute;
                do {
                    auto name = node_name(reader.Get());
                    if (!node->attributes.emplace(std::move(name), node_value(reader.Get())).second) {
                        throw std::runtime_error("Duplicate XML attribute");
                    }
                    attribute = reader->MoveToNextAttribute();
                } while (attribute == S_OK);
                check(attribute, "Read next XML attribute");
                check(reader->MoveToElement(), "Return to XML element");
            }
            Node* current = node.get();
            if (stack.empty()) {
                if (root) throw std::runtime_error("XML has multiple root elements");
                root = std::move(node);
            } else {
                stack.back()->children.push_back(std::move(node));
            }
            if (!empty) stack.push_back(current);
        } else if (type == XmlNodeType_EndElement) {
            if (stack.empty()) throw std::runtime_error("Unmatched XML closing element");
            stack.pop_back();
        } else if (!stack.empty() &&
                   (type == XmlNodeType_Text || type == XmlNodeType_CDATA || type == XmlNodeType_Whitespace)) {
            stack.back()->text += node_value(reader.Get());
        }
    }
    if (FAILED(result)) {
        UINT line = 0, column = 0;
        reader->GetLineNumber(&line);
        reader->GetLinePosition(&column);
        throw std::runtime_error("Invalid XML at line " + std::to_string(line) +
                                 ", column " + std::to_string(column) + " (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(result)) + ")");
    }
    if (!root || !stack.empty()) throw std::runtime_error("XML has no complete root element");
    return root;
}

std::unique_ptr<Node> parse_xml(const std::filesystem::path& path) {
    ComPtr<IStream> stream;
    const HRESULT opened = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                                                  FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, stream.GetAddressOf());
    if (FAILED(opened)) {
        throw std::runtime_error("Cannot open XML file " + path.u8string() + " (HRESULT " +
                                 std::to_string(static_cast<unsigned long>(opened)) + ")");
    }
    return parse_xml_stream(stream.Get());
}

std::unique_ptr<Node> parse_xml_memory(std::string_view xml) {
    if (xml.empty()) throw std::runtime_error("XML memory input is empty");
    if (xml.size() > max_xml_bytes) throw std::runtime_error("XML exceeds the 8 MiB limit");
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()), static_cast<UINT>(xml.size())));
    if (!stream) throw std::runtime_error("Cannot create XML memory stream");
    return parse_xml_stream(stream.Get());
}
#else
// Expat is a third-party XML decoder, not the recovered GE6 tokenizer. The
// platform-neutral DOM/property/ownership layer below remains shared with the
// Windows XmlLite adapter. Never unwind a C++ exception through Expat callbacks.
struct Parser {
    XML_Parser parser=nullptr;
    std::unique_ptr<Node> root;
    std::vector<Node*> stack;
    size_t count=0;
    std::exception_ptr error;
    Parser(){parser=XML_ParserCreate(nullptr);if(!parser)throw std::bad_alloc();}
    ~Parser(){XML_ParserFree(parser);}
    template<class F> void callback(F body) noexcept {
        if(error)return;
        try{body();}catch(...){error=std::current_exception();XML_StopParser(parser,XML_FALSE);}
    }
    static void start(void* user,const XML_Char* name,const XML_Char** attributes){
        auto& p=*static_cast<Parser*>(user);p.callback([&]{
            if(++p.count>200000)throw std::runtime_error("XML has too many elements");
            if(p.stack.size()>=512)throw std::runtime_error("XML exceeds the depth limit");
            auto node=std::make_unique<Node>();node->name=name;
            for(size_t i=0;attributes[i];i+=2)if(!node->attributes.emplace(attributes[i],attributes[i+1]).second)throw std::runtime_error("Duplicate XML attribute");
            Node* current=node.get();
            if(p.stack.empty()){if(p.root)throw std::runtime_error("XML has multiple root elements");p.root=std::move(node);}
            else p.stack.back()->children.push_back(std::move(node));
            p.stack.push_back(current);
        });
    }
    static void end(void* user,const XML_Char*){auto& p=*static_cast<Parser*>(user);p.callback([&]{if(p.stack.empty())throw std::runtime_error("Unmatched XML closing element");p.stack.pop_back();});}
    static void text(void* user,const XML_Char* text,int size){auto& p=*static_cast<Parser*>(user);p.callback([&]{if(!p.stack.empty())p.stack.back()->text.append(text,static_cast<size_t>(size));});}
    static void dtd(void* user,const XML_Char*,const XML_Char*,const XML_Char*,int){auto& p=*static_cast<Parser*>(user);p.callback([]{throw std::runtime_error("XML DTD is prohibited");});}
    static int external(XML_Parser,const XML_Char*,const XML_Char*,const XML_Char*,const XML_Char*){return XML_STATUS_ERROR;}
};
std::unique_ptr<Node> parse_xml_memory(std::string_view xml){
    if(xml.empty())throw std::runtime_error("XML memory input is empty");
    if(xml.size()>max_xml_bytes)throw std::runtime_error("XML exceeds the 8 MiB limit");
    Parser p;XML_SetUserData(p.parser,&p);XML_SetElementHandler(p.parser,Parser::start,Parser::end);XML_SetCharacterDataHandler(p.parser,Parser::text);
    XML_SetStartDoctypeDeclHandler(p.parser,Parser::dtd);XML_SetExternalEntityRefHandler(p.parser,Parser::external);XML_SetParamEntityParsing(p.parser,XML_PARAM_ENTITY_PARSING_NEVER);
    const auto result=XML_Parse(p.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE);
    if(p.error)std::rethrow_exception(p.error);
    if(result!=XML_STATUS_OK)throw std::runtime_error("Invalid XML at line "+std::to_string(XML_GetCurrentLineNumber(p.parser))+": "+XML_ErrorString(XML_GetErrorCode(p.parser)));
    if(!p.root||!p.stack.empty())throw std::runtime_error("XML has no complete root element");
    return std::move(p.root);
}
std::unique_ptr<Node> parse_xml(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open XML file "+path.u8string());
    const auto end=in.tellg();if(end<0||static_cast<uint64_t>(end)>max_xml_bytes)throw std::runtime_error("XML exceeds the 8 MiB limit");
    std::string data(static_cast<size_t>(end),'\0');in.seekg(0);if(!data.empty()&&!in.read(data.data(),static_cast<std::streamsize>(data.size())))throw std::runtime_error("Cannot read XML file");
    return parse_xml_memory(data);
}
#endif

struct Segment {
    std::string_view name;
    size_t index = 0;
};

std::optional<Segment> parse_segment(std::string_view value) {
    if (value.empty()) return {};
    const size_t open = value.find('(');
    if (open == std::string_view::npos) {
        if (value.find(')') != std::string_view::npos) return {};
        return Segment{value, 0};
    }
    if (!open || value.back() != ')') return {};
    const auto index_text = value.substr(open + 1, value.size() - open - 2);
    if (index_text.empty()) return {};
    size_t index = 0;
    const auto parsed = std::from_chars(index_text.data(), index_text.data() + index_text.size(), index);
    if (parsed.ec != std::errc() || parsed.ptr != index_text.data() + index_text.size()) return {};
    return Segment{value.substr(0, open), index};
}

struct Property {
    const Node* node = nullptr;
    const std::string* value = nullptr;
    bool exists = false;
};

Property lookup(int handle, std::string_view key) {
    const auto found = documents.find(handle);
    if (found == documents.end()) return {};
    const size_t hash = key.find('#');
    const auto path = key.substr(0, hash);
    const auto attribute = hash == std::string_view::npos ? std::string_view{} : key.substr(hash + 1);
    if (path.empty() || (hash != std::string_view::npos &&
                         (attribute.empty() || attribute.find('#') != std::string_view::npos))) return {};
    const Node* current = nullptr;
    size_t offset = 0;
    while (offset < path.size()) {
        const size_t dot = path.find('.', offset);
        const size_t end = dot == std::string_view::npos ? path.size() : dot;
        const auto part = parse_segment(path.substr(offset, end - offset));
        if (!part) return {};
        if (!current) {
            current = found->second.get();
            if (part->index || current->name != part->name) return {};
        } else {
            const Node* next = nullptr;
            size_t matching = 0;
            for (const auto& child : current->children) {
                if (child->name == part->name && matching++ == part->index) {
                    next = child.get();
                    break;
                }
            }
            if (!next) return {};
            current = next;
        }
        if (dot == std::string_view::npos) break;
        offset = dot + 1;
        if (offset == path.size()) return {};
    }
    if (hash == std::string_view::npos) return {current, &current->text, true};
    const auto found_attribute = current->attributes.find(std::string(attribute));
    if (found_attribute == current->attributes.end()) return {};
    return {current, &found_attribute->second, true};
}

std::string_view trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Exceptions unwind C++ objects before calling lua_error(), which can longjmp.
// luaL_check* argument checks occur before a callback creates C++ local objects.
template<int (*Body)(lua_State*)>
int protected_call(lua_State* state) {
    try {
        return Body(state);
    } catch (const std::exception& error) {
        lua_pushstring(state, error.what());
    } catch (...) {
        lua_pushliteral(state, "Unexpected XML bridge error");
    }
    return lua_error(state);
}

int store_document(lua_State* state, std::unique_ptr<Node> root) {
    auto* context = static_cast<Context*>(lua_touserdata(state, lua_upvalueindex(1)));
    if (next_handle == std::numeric_limits<int>::max()) throw std::runtime_error("XML handle range exhausted");
    const int handle = next_handle++;
    context->owned.insert(handle);
    documents.emplace(handle, std::move(root));
    lua_pushinteger(state, handle);
    return 1;
}

int load_document(lua_State* state) {
    luaL_checkstring(state, 1);  // Diagnostic document name; path controls loading.
    const char* path = luaL_checkstring(state, 2);
    auto* context = static_cast<Context*>(lua_touserdata(state, lua_upvalueindex(1)));
    return store_document(state, parse_xml(context->resolve(path)));
}

int load_memory_document(lua_State* state) {
    luaL_checkstring(state, 1);
    size_t size = 0;
    const char* xml = luaL_checklstring(state, 2, &size);
    return store_document(state, parse_xml_memory({xml, size}));
}

int read_string(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    const char* key = luaL_checkstring(state, 2);
    const auto property = lookup(handle, key);
    if (property.exists) lua_pushlstring(state, property.value->data(), property.value->size());
    else lua_pushnil(state);
    return 1;
}

int read_integer(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    const char* key = luaL_checkstring(state, 2);
    const auto property = lookup(handle, key);
    if (property.exists) {
        auto value = trim(*property.value);
        if (!value.empty() && value.front() == '+') value.remove_prefix(1);
        if (value.empty()) { lua_pushnil(state); return 1; }
        int32_t number = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
        if (!value.empty() && result.ec == std::errc() && result.ptr == value.data() + value.size()) {
            lua_pushinteger(state, number);
            return 1;
        }
    }
    lua_pushnil(state);
    return 1;
}

int read_float(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    const char* key = luaL_checkstring(state, 2);
    const auto property = lookup(handle, key);
    if (property.exists) {
        auto value = trim(*property.value);
        if (!value.empty() && value.front() == '+') value.remove_prefix(1);
        if (value.empty()) { lua_pushnil(state); return 1; }
        double number = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), number);
        if (!value.empty() && result.ec == std::errc() && result.ptr == value.data() + value.size() && std::isfinite(number)) {
            lua_pushnumber(state, number);
            return 1;
        }
    }
    lua_pushnil(state);
    return 1;
}

int read_bool(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    const char* key = luaL_checkstring(state, 2);
    const auto property = lookup(handle, key);
    if (property.exists) {
        auto value = trim(*property.value);
        auto equals = [value](std::string_view wanted) {
            if (value.size() != wanted.size()) return false;
            for (size_t i = 0; i < value.size(); ++i) {
                const char c = value[i] >= 'A' && value[i] <= 'Z' ? value[i] + ('a' - 'A') : value[i];
                if (c != wanted[i]) return false;
            }
            return true;
        };
        if (equals("true") || equals("1")) { lua_pushboolean(state, 1); return 1; }
        if (equals("false") || equals("0")) { lua_pushboolean(state, 0); return 1; }
    }
    lua_pushnil(state);
    return 1;
}

int has_property(lua_State* state) {
    const int handle = static_cast<int>(luaL_checkinteger(state, 1));
    const char* key = luaL_checkstring(state, 2);
    const auto property = lookup(handle, key);
    lua_pushboolean(state, property.exists);
    return 1;
}

int destroy_context(lua_State* state) {
    static_cast<Context*>(lua_touserdata(state, 1))->~Context();
    return 0;
}

}  // namespace

void register_xml(lua_State* state, ResourceResolver resolver) {
    if (!state || !resolver) throw std::invalid_argument("XML bridge requires a Lua state and resource resolver");
    auto* context = new (lua_newuserdata(state, sizeof(Context))) Context{std::move(resolver), {}};
    (void)context;
    if (luaL_newmetatable(state, "fs15menu.XMLContext")) {
        lua_pushcfunction(state, destroy_context);
        lua_setfield(state, -2, "__gc");
    }
    lua_setmetatable(state, -2);
    const int context_index = lua_gettop(state);
    const luaL_Reg functions[] = {
        {"loadXMLFile", protected_call<load_document>},
        {"loadXMLFileFromMemory", protected_call<load_memory_document>},
        {"getXMLString", protected_call<read_string>},
        {"getXMLInt", protected_call<read_integer>},
        {"getXMLFloat", protected_call<read_float>},
        {"getXMLBool", protected_call<read_bool>},
        {"hasXMLProperty", protected_call<has_property>},
        {nullptr, nullptr}
    };
    for (const luaL_Reg* function = functions; function->name; ++function) {
        lua_pushvalue(state, context_index);
        lua_pushcclosure(state, function->func, 1);
        lua_setglobal(state, function->name);
    }
    lua_pop(state, 1);
}

bool delete_xml(int handle) {
    return documents.erase(handle) != 0;
}

int xml_document_count() {
    return static_cast<int>(documents.size());
}

void validate_xml_memory(std::string_view xml, std::string_view expected_root) {
    const auto root = parse_xml_memory(xml);
    if (!expected_root.empty() && root->name != expected_root) {
        throw std::runtime_error("XML has an unexpected root element");
    }
}

}  // namespace fs15menu
