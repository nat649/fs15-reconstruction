#pragma once
#include <windows.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

struct lua_State;

namespace fs15menu {
using ResourceResolver = std::function<std::filesystem::path(const std::string&)>;

// Independently authored drawing backend for the recovered game's Lua GUI.
class Renderer {
public:
    Renderer(ResourceResolver resolver, int width, int height);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void begin_frame();
    void present(HDC dc);
    void save_png(const std::filesystem::path& path);
    bool delete_overlay(int id);
    int overlay_count() const;
    int missing_asset_count() const;
    void register_lua(lua_State* state);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
