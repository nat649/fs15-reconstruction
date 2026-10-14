-- Authored adapter for running the recovered FS15 GUI source on the rebuilt host.
-- This is deliberately a menu runtime: it never loads main.lua, a mission, mods,
-- multiplayer networking, the original executable, or original Lua bytecode.

local bit = require("bit")
bitAND = bit.band
bitOR = bit.bor
RenderText = { ALIGN_LEFT = 0, ALIGN_CENTER = 1, ALIGN_RIGHT = 2 }

GS_PLATFORM_TYPE_PC = 0
GS_PLATFORM_TYPE_PS3 = 1
GS_PLATFORM_TYPE_X360 = 2
GS_PLATFORM_TYPE_WIIU = 3
GS_PLATFORM_TYPE_PS4 = 4
GS_PLATFORM_TYPE_XBOXONE = 5
GS_PLATFORM_TYPE = GS_PLATFORM_TYPE_PC
GS_IS_CONSOLE_VERSION = false
GS_IS_CURRENT_GEN = false
GS_IS_OLD_GEN = false
GS_MULTIPLAYER = true
GS_IS_STEAM_VERSION = true -- Match the local Steam edition's menu arrangement.
GS_IS_MAC_APP_STORE_VERSION = false

g_screenWidth = assert(g_screenWidth, "Host must set g_screenWidth")
g_screenHeight = assert(g_screenHeight, "Host must set g_screenHeight")
g_screenAspectRatio = g_screenWidth / g_screenHeight
g_languageShort = g_languageShort or "en"
g_languageSuffix = "_" .. g_languageShort
g_referenceScreenWidth = 1920
g_referenceScreenHeight = 1080
g_aspectScaleX = math.min(1, (16 / 9) / g_screenAspectRatio)
g_aspectScaleY = math.min(1, g_screenAspectRatio / (16 / 9))
g_baseUIFilename = "dataS2/menu/hud/gui_elements.png"
if g_screenWidth <= 1280 and g_screenHeight <= 720 then
    g_baseUIFilename = "dataS2/menu/hud/gui_elements_512.png"
end
g_analogStickHTolerance = 0.85
g_analogStickVTolerance = 0.85
g_forceDefaultPCButtons = true
g_plattformPrefix = ""
g_uniqueDlcNamePrefix = "pdlc_"
g_modNameToDirectory = {}
g_isDemo = false
g_isGamescomVersion = false
g_isServerStreamingVersion = false
g_addTestCommands = false
g_gameVersionDisplay = "1.4.2.0"
g_gameVersionDisplayExtra = ""

-- These normalizing services were globals in main.lua. The arithmetic is
-- independently implemented here so main.lua need not initialize the game.
function getNormalizedValues(values, reference)
    if values == nil then return nil end
    local result = {}
    for i, value in ipairs(values) do result[i] = value / reference[(i - 1) % 2 + 1] end
    return result
end
function getNormalizedUVs(values, reference)
    local v = getNormalizedValues(values, reference or { 1024, 1024 })
    return { v[1], v[2], v[1], v[4], v[3], v[2], v[3], v[4] }
end
function getCorrectTextSize(value) return value * g_aspectScaleY end
function getFunction(name)
    local value = _G
    for component in name:gmatch("[^.]+") do
        if type(value) ~= "table" then return nil end
        value = value[component]
    end
    if type(value) == "function" then return value end
    return nil
end

-- The English menu labels only require ASCII case conversion. A host-provided
-- UTF-8 implementation takes precedence when additional languages are enabled.
utf8ToUpper = utf8ToUpper or string.upper

source("dataS/scripts/shared/class.lua")
source("dataS/scripts/shared/input.lua")
source("dataS/scripts/Utils.lua")
source("dataS/scripts/I18N.lua")
source("dataS/scripts/gui/Overlay.lua")
source("dataS/scripts/gui/Gui.lua")
initGuiLibrary("dataS/scripts/gui")
source("dataS/scripts/gui/FocusManager.lua")
source("dataS/scripts/gui/MainScreen.lua")
source("dataS/scripts/gui/CreditsScreen.lua")

-- Authored keyboard service implementing the subset consumed by recovered GUI
-- elements. Actual mouse hit testing, button focus and callbacks stay in Lua.
InputBinding = {}
for _, name in ipairs({
    "MENU", "MENU_CANCEL", "MENU_ACCEPT", "MENU_UP", "MENU_DOWN", "MENU_LEFT",
    "MENU_RIGHT", "MENU_AXIS_UP_DOWN", "MENU_AXIS_LEFT_RIGHT",
    "CONSOLE_MENU_ACTIVATE", "CONSOLE_MENU_CANCEL", "CONSOLE_MENU_OK",
    "CONSOLE_MENU_BACK", "CONSOLE_MULTISELECTION2_PREVIOUS",
    "CONSOLE_MULTISELECTION2_NEXT", "CONSOLE_MULTISELECTION1_PREVIOUS",
    "CONSOLE_MULTISELECTION1_NEXT"
}) do InputBinding[name] = name end
local pressed, events = {}, {}
function InputBinding.isPressed(action) return pressed[action] == true end
function InputBinding.hasEvent(action, consume)
    local value = events[action] == true
    if consume and action ~= nil then events[action] = nil end
    return value
end
function InputBinding.getDigitalInputAxis() return 0 end
function InputBinding.getAnalogInputAxis() return 0 end
function InputBinding.isAxisZero(value) return value == 0 end
function InputBinding.setShowMouseCursor(visible)
    -- The host uses the normal Windows cursor for this window.
    InputBinding.mouseCursorVisible = visible
end

-- Explicit offline adapters: menu-only host has no account presence, remote
-- notifications, DLC changes, game networking, web navigation or audio mixer.
-- These are enumerated in the startup trace, never hidden by a generic fallback.
PresenceModes = { PRESENCE_IDLE = 0 }
function setPresenceMode(mode)
    assert(mode == PresenceModes.PRESENCE_IDLE, "Unsupported presence mode")
end
function notificationsLoaded() return true end
function getNumOfNotifications() return 0 end
function storeHaveDlcsChanged() return false end
function storeAreDlcsCorrupted() return false end
function storeHasNativeGUI() return false end
function playStreamedSample() end -- Audio playback is outside this milestone.
function masterServerConnectFront()
    error("The rebuilt menu has no multiplayer networking backend")
end
function checkMultiplayerAvailable() return 0 end -- Dispatch to the unsupported-screen adapter.
function openWebFile() menuUnavailable("Mod downloads") end
g_careerScreen = {
    setIsMultiplayer = function(self, value) self.isMultiplayer = value end
}

g_i18n = I18N:new()
g_gui = Gui:new(g_languageSuffix)
g_gui:loadProfiles("dataS/guiProfiles.xml")
g_mainScreen = MainScreen:new()
g_creditsScreen = CreditsScreen:new()
g_gui:loadGui("dataS/gui/CreditsScreen.xml", "CreditsScreen", g_creditsScreen)
g_gui:loadGui("dataS/gui/MainScreen.xml", "MainScreen", g_mainScreen)

-- Keep original MainScreen callbacks. Requests for an unimplemented screen are
-- reported visibly without creating a fake submenu or changing currentGui.
local recoveredShowGui = Gui.showGui
function g_gui:showGui(name)
    if self.guis[name] == nil then
        menuUnavailable(name)
        return nil
    end
    return recoveredShowGui(self, name)
end
g_gui:showGui("MainScreen")

local keyActions = {
    [13] = InputBinding.MENU_ACCEPT, [27] = InputBinding.MENU_CANCEL,
    [37] = InputBinding.MENU_LEFT, [38] = InputBinding.MENU_UP,
    [39] = InputBinding.MENU_RIGHT, [40] = InputBinding.MENU_DOWN
}
function fs15_menu_key(vk, isDown)
    local action = keyActions[vk]
    if action ~= nil then
        if isDown and not pressed[action] then events[action] = true end
        pressed[action] = isDown == true
        if isDown and vk >= 37 and vk <= 40 then
            local focused = FocusManager:getFocusedElement()
            if focused == nil and g_gui.currentGuiName == "MainScreen" then
                FocusManager:setFocus(g_mainScreen.careerButton)
            end
        end
    end
    g_gui:keyEvent(0, vk, 0, isDown)
end
function fs15_menu_mouse(x, y, isDown, isUp, button)
    Input.updateMouseButtonState(button, isDown)
    g_gui:mouseEvent(x, y, isDown, isUp, button)
end
function fs15_menu_update(dt)
    g_gui:update(dt)
    events = {}
    Input.updateFrameEnd()
end
function fs15_menu_draw() g_gui:draw() end

function fs15_menu_selfcheck()
    assert(g_gui.currentGuiName == "MainScreen", "Expected MainScreen")
    assert(g_gui.currentGui.target == g_mainScreen, "Recovered MainScreen target missing")
    local visible = 0
    for _, name in ipairs({ "careerButton", "multiplayerButton", "tutorialsButton",
        "achievementsButton", "settingsButton", "creditsButton", "quitButton" }) do
        local button = assert(g_mainScreen[name], "Missing recovered button: " .. name)
        assert(button:getIsVisible(), "Recovered button is hidden: " .. name)
        assert(type(button.onClick) == "function", "Missing original callback: " .. name)
        assert(button.text ~= nil and not button.text:match("^Missing "), "Missing menu translation")
        visible = visible + 1
    end
    assert(visible == 7, "Steam edition must display seven main buttons")
    assert(g_gui.guis.CreditsScreen.target == g_creditsScreen, "Credits source not initialized")
    return "current_gui=MainScreen; visible_buttons=7; recovered_callbacks=7; credits_initialized=true"
end

function fs15_menu_test_inputs()
    fs15_menu_selfcheck()
    local function click(button)
        assert(button:getIsVisible() and not button.disabled, "Test requires an active recovered button")
        local x = button.absPosition[1] + button.size[1] / 2
        local y = button.absPosition[2] + button.size[2] / 2
        fs15_menu_mouse(x, y, true, false, Input.MOUSE_BUTTON_LEFT)
        fs15_menu_mouse(x, y, false, true, Input.MOUSE_BUTTON_LEFT)
    end

    -- Original onCareerClick calls the facade and then asks Gui for CareerScreen.
    click(g_mainScreen.careerButton)
    assert(g_careerScreen.isMultiplayer == false, "Original career callback did not run")
    assert(g_gui.currentGuiName == "MainScreen", "Unsupported career changed the menu")

    -- Exercise the recovered FocusManager navigation and activation logic.
    FocusManager:setFocus(g_mainScreen.careerButton)
    fs15_menu_key(40, true)
    fs15_menu_update(16)
    fs15_menu_key(40, false)
    assert(FocusManager:getFocusedElement() == g_mainScreen.multiplayerButton, "Down key did not move recovered focus")
    fs15_menu_key(13, true)
    fs15_menu_update(16)
    fs15_menu_key(13, false)
    assert(g_mainScreen.lastActiveButton == g_mainScreen.multiplayerButton, "Original multiplayer callback did not run")
    assert(g_gui.currentGuiName == "MainScreen", "Unsupported multiplayer changed the menu")

    -- Credits use recovered source and XML, with their original return callback.
    click(g_mainScreen.creditsButton)
    assert(g_gui.currentGuiName == "CreditsScreen", "Credits callback did not open the recovered screen")
    fs15_menu_update(800)
    assert(#g_creditsScreen.creditsLines > 0, "Recovered credits update did not create a line")
    click(g_creditsScreen.test1234)
    fs15_menu_selfcheck()

    -- Quit runs last. The native host records and verifies its exit request.
    click(g_mainScreen.quitButton)
    print("Menu input checks: career mouse callback; keyboard Down/Enter multiplayer callback; credits mouse open/update/back; quit mouse callback")
end

print("Menu adapter services: keyboard input; XML/2D native bridge; offline presence; zero remote notifications; no DLC updates; no networking; unavailable-screen dialogs; no audio playback")
print("Menu bootstrap: " .. fs15_menu_selfcheck())
