-- Authored integration for the recovered career screen and mission metadata.
-- The native save provider reads careerSavegame.xml; this adapter never saves,
-- deletes, loads a saved simulation, or executes a mission script.

local installed = false
local fixtureDepth, nativeGetInfo = 0, nil

local function unavailableWrite()
    menuUnavailable("Savegame changes (read-only career browser)")
end

local function finiteNumber(value)
    return type(value) == "number" and value == value and value > -math.huge and value < math.huge
end

local function supportedDisplayValues(savegame)
    return finiteNumber(savegame.difficulty) and savegame.difficulty >= 1 and savegame.difficulty <= 3
        and savegame.difficulty == math.floor(savegame.difficulty)
        and finiteNumber(savegame.money) and finiteNumber(savegame.playTime) and savegame.playTime >= 0
end

function fs15_install_career()
    assert(not installed, "Career adapter is already installed")
    assert(MapsUtil ~= nil, "Install the base-map menu catalogue before career")
    assert(type(saveGetInfoById) == "function", "Read-only save provider is required")

    -- These sources declare the original classes. World fruit/fill registries
    -- remain uninitialized; no vehicle, crop or simulation method is executed.
    source("dataS/scripts/FruitUtil.lua")
    source("dataS/scripts/vehicles/specializations/Fillable.lua")
    source("dataS/scripts/MissionInfo.lua")
    source("dataS/scripts/FSMissionInfo.lua")
    source("dataS/scripts/FSCareerMissionInfo.lua")
    source("dataS/scripts/gui/CareerScreen.lua")

    local recoveredOpen = CareerScreen.onOpen
    function CareerScreen:onOpen()
        recoveredOpen(self)
        self.buttonDeletePC:setDisabled(true)
        self.buttonDeleteConsole:setDisabled(true)
    end

    -- Keep the original list cloning, XML metadata parsing and text formatting.
    -- A failed metadata parse must not turn an occupied slot into a new game.
    local recoveredUpdateText = CareerScreen.updateSavegameText
    function CareerScreen:updateSavegameText(index)
        local savegame = self.savegames[index]
        local metadata, displayName = saveGetInfoById(index)
        -- Money is parsed by the recovered class with tonumber(string), so the
        -- native XML float reader's finite-value check cannot protect it here.
        -- Invalid metadata remains occupied/read only after resetting defaults.
        if savegame.isValid and (self.fs15ScanFailed or not supportedDisplayValues(savegame)) then
            savegame:loadDefaults()
        end
        savegame.fs15HasMetadata = self.fs15ScanFailed or metadata ~= ""
        savegame.fs15DisplayName = displayName
        savegame.fs15MetadataUnavailable = self.fs15ScanFailed or savegame.fs15HasMetadata and not savegame.isValid
        recoveredUpdateText(self, index)
        if savegame.fs15MetadataUnavailable and not savegame.isInvalidUser then
            local message = self.fs15ScanFailed
                and "Savegames could not be scanned. This slot is unavailable and read only."
                or "Save metadata is unavailable or unsupported. This slot is read only."
            self.listItemInfoText[index]:setText(message)
        end
    end

    local recoveredScanComplete = CareerScreen.onSaveGameUpdateComplete
    function CareerScreen:onSaveGameUpdateComplete(errorCode)
        self.fs15ScanFailed = errorCode ~= Savegame.ERROR_OK and errorCode ~= Savegame.ERROR_DATA_CORRUPT
        if errorCode == Savegame.ERROR_DATA_CORRUPT then
            -- Failed slots are explicitly labelled above. The original recovery
            -- dialog can lead to destructive operations unavailable in this host.
            self.ignoreCorruptOnNextUpdate = true
        elseif errorCode ~= Savegame.ERROR_OK then
            self:setIsWaitingForSaveGameInfo(false)
            self:updateSavegameListElements()
            menuUnavailable("Read savegame metadata (scan failed)")
            return
        end
        recoveredScanComplete(self, errorCode)
    end

    function CareerScreen:startSavegame(savegame)
        assert(savegame ~= nil, "Select a career slot before starting")
        self.currentSavegame = savegame
        if savegame.fs15MetadataUnavailable or savegame.isInvalidUser then
            menuUnavailable("Read saved game metadata for this slot")
        elseif savegame.isValid then
            self:startCurrentSavegame()
        else
            -- This opens the original DifficultyScreen. No storage allocation
            -- is requested because a menu selection never creates a savegame.
            self:onYesNoNotEnoughSpaceForNewSaveGame(true)
        end
    end

    function CareerScreen:startCurrentSavegame()
        local savegame = assert(self.currentSavegame, "No selected career slot")
        self.lastWorldRequest = {
            mapId = savegame.mapId,
            difficulty = savegame.difficulty,
            savegameIndex = savegame.savegameIndex,
            isExisting = savegame.isValid == true
        }
        fs15_last_world_request = self.lastWorldRequest
        if savegame.isValid then
            menuUnavailable("Load saved game simulation (map scene viewer available for new selections)")
        else
            assert(menuLaunchWorld(savegame.mapId, savegame.difficulty, savegame.savegameIndex), "Map scene runtime was not launched")
        end
    end

    -- Buttons and every recovered entry point to save mutations are blocked.
    -- The native provider independently rejects its write/delete API as well.
    CareerScreen.onClickCancel = unavailableWrite
    CareerScreen.onYesNoDeleteSavegame = unavailableWrite
    CareerScreen.deleteCurrentSavegame = unavailableWrite
    CareerScreen.backupSavegame = unavailableWrite
    CareerScreen.saveSavegame = unavailableWrite
    CareerScreen.onSaveStartComplete = unavailableWrite
    CareerScreen.onSaveComplete = unavailableWrite
    FSCareerMissionInfo.saveToXML = unavailableWrite

    g_careerScreen = CareerScreen:new()
    g_careerScreen:setIsMultiplayer(false)
    g_gui:loadGui("dataS/gui/CareerScreen.xml", "CareerScreen", g_careerScreen)
    g_careerScreen.buttonDeletePC:setDisabled(true)
    g_careerScreen.buttonDeleteConsole:setDisabled(true)
    installed = true
    print("Career adapter: recovered CareerScreen and mission metadata classes; 20 read-only slots; new selections launch map scenes; save writes/deletes and saved simulation unavailable")
end

function fs15_career_selfcheck()
    assert(installed, "Career adapter is not installed")
    assert(g_gui.guis.CareerScreen.target == g_careerScreen, "Recovered CareerScreen target missing")
    local screen = g_careerScreen
    if #screen.savegames == 0 then screen:recreateSavegameList() end
    assert(CareerScreen.numSavegames == 20, "Expected twenty PC save slots")
    assert(#screen.savegames == 20 and #screen.savegameList.listItems == 20, "Recovered career list did not clone twenty slots")
    assert(screen.savegameList.visibleItems == 3, "Original three-row layout changed")
    assert(screen.buttonDeletePC.disabled and screen.buttonDeleteConsole.disabled, "Delete must remain disabled")
    local existing, empty, unavailable = 0, 0, 0
    for index, savegame in ipairs(screen.savegames) do
        assert(savegame.savegameIndex == index, "Save slot identity mismatch")
        assert(screen.listItemTexts[index] and screen.listItemInfoText[index], "Missing original slot widget")
        if savegame.isValid then
            existing = existing + 1
            assert(savegame.map and screen.mapNameTexts[index].text == savegame.map.title, "Saved map title was not displayed")
            assert(screen.moneyTexts[index].text == g_i18n:formatMoney(savegame.money), "Saved money was not displayed")
            assert(screen.dateTexts[index].text == savegame.saveDate, "Saved date was not displayed")
            assert(screen.difficultyTexts[index].text == g_i18n:getText("Diff" .. savegame.difficulty), "Saved difficulty was not displayed")
        elseif savegame.fs15MetadataUnavailable then
            unavailable = unavailable + 1
            assert(screen.listItemTexts[index].visible, "Unavailable metadata must remain visible")
        else
            empty = empty + 1
            assert(screen.listItemInfoText[index].text == g_i18n:getText("This_savegame_is_currently_unused"), "Empty slot label missing")
        end
    end
    return string.format("career_slots=20; visible_rows=3; existing=%d; empty=%d; unavailable=%d; delete_disabled=true; metadata_classes=recovered", existing, empty, unavailable)
end

-- Authored deterministic metadata for input checks. These functions only swap a
-- Lua getter temporarily: no fixture is written into the real user save folder.
function fs15_career_test_fixture_begin()
    assert(installed, "Career adapter is not installed")
    if fixtureDepth == 0 then
        nativeGetInfo = saveGetInfoById
        saveGetInfoById = function(index)
            if index == 1 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" difficulty="3" money="123456" playTime="125" saveDate="2026-10-14"/>', "Authored read-only test save"
            elseif index == 3 then
                return "corrupt", "Authored corrupt metadata fixture"
            elseif index == 4 then
                return '<careerSavegame revision="2" valid="true" mapId="Missing_Test_Map"/>', "Authored unsupported-map fixture"
            elseif index == 5 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" difficulty="99"/>', "Authored invalid-difficulty fixture"
            elseif index == 6 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" money="nan"/>', "Authored nonfinite-money fixture"
            elseif index == 7 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" money="inf"/>', "Authored infinite-money fixture"
            elseif index == 8 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" playTime="-1"/>', "Authored negative-duration fixture"
            elseif index == 9 then
                return '<careerSavegame revision="2" valid="true" mapId="Map01" money="-500"/>', "Authored negative-money fixture"
            end
            return "", "Authored empty slot " .. index
        end
        g_careerScreen:recreateSavegameList()
    end
    fixtureDepth = fixtureDepth + 1
end

function fs15_career_test_fixture_end()
    assert(fixtureDepth > 0, "Career test fixture is not active")
    fixtureDepth = fixtureDepth - 1
    if fixtureDepth == 0 then
        saveGetInfoById, nativeGetInfo = nativeGetInfo, nil
        g_careerScreen:recreateSavegameList()
    end
end

local function click(element)
    assert(element:getIsVisible() and not element.disabled, "Test requires an active career element")
    local x = element.absPosition[1] + element.size[1] / 2
    local y = element.absPosition[2] + element.size[2] / 2
    fs15_menu_mouse(x, y, true, false, Input.MOUSE_BUTTON_LEFT)
    fs15_menu_mouse(x, y, false, true, Input.MOUSE_BUTTON_LEFT)
end

local function callbackButton(parent, callback)
    if parent.onClick == callback and parent:getIsVisible() then return parent end
    for _, child in ipairs(parent.elements or {}) do
        local found = callbackButton(child, callback)
        if found ~= nil then return found end
    end
    return nil
end

function fs15_career_test_inputs()
    assert(g_gui.currentGuiName == "MainScreen", "Career checks start at MainScreen")
    fs15_career_test_fixture_begin()
    local ok, failure = xpcall(function()
        click(g_mainScreen.careerButton)
        assert(g_gui.currentGuiName == "CareerScreen", "Original career callback did not open CareerScreen")
        print("Career input check: " .. fs15_career_selfcheck())
        local screen, list = g_careerScreen, g_careerScreen.savegameList
        local widgets = g_gui.guis.CareerScreen
        local start = assert(callbackButton(widgets, CareerScreen.onClickOk), "Recovered Start button missing")
        local back = assert(callbackButton(widgets, CareerScreen.onClickBack), "Recovered Back button missing")
        assert(screen.savegames[1].money == 123456 and screen.savegames[1].playTime == 125, "Recovered metadata parser lost fixture values")
        assert(screen.timePlayedTexts[1].text == "02:05 " .. g_i18n:getText("shortTime"), "Recovered duration formatter failed")
        assert(screen.savegames[9].isValid and screen.savegames[9].money == -500, "Finite negative money must remain valid")

        -- The native bridge forwards wheel events to the original list widget.
        local x = list.absPosition[1] + list.size[1] / 2
        local y = list.absPosition[2] + list.size[2] / 2
        fs15_menu_mouse(x, y, true, false, Input.MOUSE_BUTTON_WHEEL_DOWN)
        fs15_menu_mouse(x, y, false, true, Input.MOUSE_BUTTON_WHEEL_DOWN)
        assert(list.firstVisibleItem == 2, "Recovered wheel scrolling failed")
        list:setSelectedRow(20, true)
        assert(screen.selectedIndex == 20 and list.firstVisibleItem == 18, "Slot twenty is not reachable")

        list:setSelectedRow(1, true)
        click(start)
        assert(g_gui.currentGuiName == "CareerScreen", "Existing save crossed the world boundary")
        assert(screen.lastWorldRequest.isExisting and screen.lastWorldRequest.mapId == "Map01", "Existing save did not reach explicit load boundary")
        local previousRequest = screen.lastWorldRequest
        for _, row in ipairs({3, 4, 5, 6, 7, 8}) do
            assert(screen.savegames[row].fs15MetadataUnavailable and not screen.savegames[row].isValid, "Unsupported metadata was not marked read only")
            list:setSelectedRow(row, true)
            click(start)
            assert(g_gui.currentGuiName == "CareerScreen" and screen.lastWorldRequest == previousRequest, "Unavailable save metadata entered a new game")
        end

        -- A failed scan does not prove any slot is empty. Even fixture slot two
        -- must stay unavailable until a later successful scan rebuilds the list.
        screen:onSaveGameUpdateComplete(Savegame.ERROR_SCAN_FAILED)
        for _, savegame in ipairs(screen.savegames) do
            assert(savegame.fs15MetadataUnavailable and not savegame.isValid, "Failed scan exposed an empty/new-game slot")
        end
        list:setSelectedRow(2, true)
        click(start)
        assert(g_gui.currentGuiName == "CareerScreen" and screen.lastWorldRequest == previousRequest, "Failed scan entered a new game")
        screen:onSaveGameUpdateComplete(Savegame.ERROR_OK)
        assert(not screen.fs15ScanFailed and screen.savegames[1].isValid, "Successful scan did not restore valid metadata")
        assert(not screen.savegames[2].fs15MetadataUnavailable, "Successful scan did not restore the empty slot")

        list:setSelectedRow(2, true)
        click(start)
        assert(g_gui.currentGuiName == "DifficultyScreen", "Empty slot did not reach the original difficulty screen")
        g_difficultyScreen:onClickBack()
        assert(g_gui.currentGuiName == "CareerScreen", "Difficulty Back did not return to career")
        screen:onClickCancel()
        assert(screen.buttonDeletePC.disabled and g_gui.currentGuiName == "CareerScreen", "Read-only delete guard failed")
        click(back)
        assert(g_gui.currentGuiName == "MainScreen", "Recovered career Back did not return to main")
        print("Career input checks: original mouse Career/Start/Back; wheel scroll; all twenty slots; recovered XML metadata and duration; existing-world boundary; unreadable/numeric metadata guards; failed-scan recovery; empty-slot Difficulty; disabled Delete")
    end, debug.traceback)
    fs15_career_test_fixture_end()
    if not ok then error(failure, 0) end
end
