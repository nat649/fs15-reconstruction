-- Authored menu integration. Selection, descriptions, markers, list cloning and
-- navigation run the recovered FS15 classes and XML. No mission is loaded here.

function fs15_install_newgame_screens()
    assert(g_difficultyScreen == nil and g_mapSelectionScreen == nil,
        "New-game screens must be installed once")
    if MapsUtil == nil then source("dataS/scripts/MapsUtil.lua") end
    source("dataS/scripts/gui/DifficultyScreen.lua")
    source("dataS/scripts/gui/MapSelectionScreen.lua")

    -- Same two base-map registrations as recovered main.lua, lines 1289-1295.
    -- Loading the mission classes or vehicle files belongs to the world engine.
    assert(#MapsUtil.mapList == 0, "Expected an uninitialized base-map catalogue")
    MapsUtil.addMapItem("Map01", "dataS/scripts/missions/CareerMap01.lua", "CareerMap01",
        "dataS2/menu/briefingScreen/careerMap01", "careerMap01",
        "profileTemplate/careerVehicles_map01.xml", g_i18n:getText("Map01_Title"),
        g_i18n:getText("Map01_Description"), "data/maps/map01/map_preview.png", "", nil, true)
    MapsUtil.addMapItem("Map02", "dataS/scripts/missions/CareerMap02.lua", "CareerMap02",
        "dataS2/menu/briefingScreen/americanMap", "americanMap",
        "profileTemplate/careerVehicles_map02.xml", g_i18n:getText("AmericanMap_Title"),
        g_i18n:getText("AmericanMap_Description"), "data/maps/map02/map_preview.png", "", nil, true)

    g_difficultyScreen = DifficultyScreen:new()
    g_mapSelectionScreen = MapSelectionScreen:new()
    g_gui:loadGui("dataS/gui/DifficultyScreen.xml", "DifficultyScreen", g_difficultyScreen)
    g_gui:loadGui("dataS/gui/MapSelectionScreen.xml", "MapSelectionScreen", g_mapSelectionScreen)
end

local function element(screen, id)
    local focus = assert(FocusManager.guiFocusData[screen], "Missing recovered focus table: " .. screen)
    return assert(focus.idToElementMapping[tostring(id)], "Missing recovered focus element: " .. screen .. ":" .. id)
end

local function click(button)
    assert(button:getIsVisible() and not button.disabled, "Expected an active recovered button")
    local x = button.absPosition[1] + button.size[1] / 2
    local y = button.absPosition[2] + button.size[2] / 2
    fs15_menu_mouse(x, y, true, false, Input.MOUSE_BUTTON_LEFT)
    fs15_menu_mouse(x, y, false, true, Input.MOUSE_BUTTON_LEFT)
end

local function select_empty_career()
    assert(g_careerScreen ~= nil, "Career screen must be installed")
    g_careerScreen:setIsMultiplayer(false)
    g_gui:showGui("CareerScreen")
    fs15_menu_update(16)
    local index
    for i, savegame in ipairs(g_careerScreen.savegames) do
        if not savegame.isValid and not savegame.isInvalidUser and not savegame.fs15MetadataUnavailable then
            index = i
            break
        end
    end
    assert(index ~= nil, "All career slots are occupied; choose an empty slot to start a new game")
    g_careerScreen.savegameList:setSelectedRow(index)
    return index
end

-- Used by the host's direct-screen preview command. This follows normal career
-- callbacks with an actually empty slot; it never creates or overwrites a save.
function fs15_newgame_show(name)
    assert(name == "DifficultyScreen" or name == "MapSelectionScreen", "Unexpected new-game screen")
    select_empty_career()
    g_careerScreen:onClickOk()
    assert(g_gui.currentGuiName == "DifficultyScreen", "Empty career did not reach difficulty selection")
    if name == "MapSelectionScreen" then
        g_difficultyScreen:onClickNormal()
        assert(g_gui.currentGuiName == name, "Difficulty callback did not reach map selection")
    end
end

function fs15_newgame_state()
    local savegame = g_careerScreen and g_careerScreen.currentSavegame
    local row = g_mapSelectionScreen.mapList.selectedRow
    local map = g_mapSelectionScreen.maps and g_mapSelectionScreen.maps[row]
    return {
        screen = g_gui.currentGuiName,
        difficulty = savegame and savegame.difficulty,
        mapId = map and map.id,
        selectedRow = row,
        worldRequested = g_careerScreen and g_careerScreen.lastWorldRequest ~= nil
    }
end

function fs15_newgame_selfcheck()
    assert(g_gui.guis.DifficultyScreen.target == g_difficultyScreen, "Difficulty source not initialized")
    assert(g_gui.guis.MapSelectionScreen.target == g_mapSelectionScreen, "Map selection source not initialized")
    assert(#MapsUtil.mapList == 2 and MapsUtil.getMapById("Map01") and MapsUtil.getMapById("Map02"),
        "Expected the two installed base maps")
    for i, map in ipairs(MapsUtil.mapList) do
        assert(map.customEnvironment == nil and map.isMultiplayerSupported, "Unexpected mod map")
        assert(map.title and map.title ~= "" and not map.title:match("^Missing "), "Missing map title")
        assert(map.description and map.description ~= "" and not map.description:match("^Missing "), "Missing map description")
        assert(map.iconFilename == string.format("data/maps/map%02d/map_preview.png", i), "Unexpected base-map preview")
    end
    for i = 1, 3 do
        local button = element("DifficultyScreen", i)
        assert(type(button.onClick) == "function" and type(button.onFocus) == "function", "Missing original difficulty callback")
    end
    assert(type(element("MapSelectionScreen", 2).onClick) == "function", "Missing original map Start callback")
    return "difficulty_choices=3; base_maps=2; recovered_newgame_callbacks=true; original_map_scene=true; simulation=false"
end

-- The caller brackets this with the authored career fixture, so existing user
-- saves are neither required nor changed. Mouse events go through recovered GUI
-- hit testing, then the original career/difficulty/map callbacks.
function fs15_newgame_test_inputs()
    fs15_newgame_selfcheck()
    assert(g_gui.currentGuiName == "MainScreen", "New-game test must start from MainScreen")
    click(g_mainScreen.careerButton)
    local slot = select_empty_career()
    menuCapture("CareerScreen")
    click(element("CareerScreen", 2))
    assert(g_gui.currentGuiName == "DifficultyScreen", "Career Start did not open difficulty selection")
    menuCapture("DifficultyScreen")
    click(element("DifficultyScreen", 4))
    assert(g_gui.currentGuiName == "CareerScreen", "Difficulty Back did not restore CareerScreen")

    for difficulty = 1, 3 do
        g_careerScreen.savegameList:setSelectedRow(slot)
        click(element("CareerScreen", 2))
        assert(g_gui.currentGuiName == "DifficultyScreen", "Career Start did not reopen difficulty selection")
        click(element("DifficultyScreen", difficulty))
        assert(g_careerScreen.currentSavegame.difficulty == difficulty, "Original difficulty callback did not store selection")
        assert(g_gui.currentGuiName == "MapSelectionScreen", "Difficulty callback did not open map selection")
        assert(#g_mapSelectionScreen.maps == 2 and #g_mapSelectionScreen.mapList.listItems == 2,
            "Original map list did not clone both base-map items")
        for row, item in ipairs(g_mapSelectionScreen.mapList.listItems) do
            assert(item.elements[1].overlay.filename == MapsUtil.mapList[row].iconFilename, "Map item has wrong preview")
            assert(item.elements[2].text == MapsUtil.mapList[row].title, "Map item has wrong title")
            assert(item.elements[3].text == MapsUtil.mapList[row].description, "Map item has wrong description")
        end
        -- Avoid a previous test click becoming a double-click on a reopened list.
        fs15_menu_update(1100)
        local list = g_mapSelectionScreen.mapList
        local row = difficulty % 2 + 1
        local x = list.absPosition[1] + list.size[1] / 2
        local y = list.absPosition[2] + list.size[2] - list.listItemStartYOffset
            - (row - 0.5) * list.listItemHeight - (row - 1) * list.listItemSpacing
        fs15_menu_mouse(x, y, true, false, Input.MOUSE_BUTTON_LEFT)
        fs15_menu_mouse(x, y, false, true, Input.MOUSE_BUTTON_LEFT)
        assert(list.selectedRow == row, "Recovered map list mouse selection failed")
        menuCapture("MapSelectionScreen")
        g_careerScreen.lastWorldRequest = nil
        click(element("MapSelectionScreen", 2))
        local request = assert(g_careerScreen.lastWorldRequest, "Map Start did not reach the explicit world boundary")
        assert(request.mapId == MapsUtil.mapList[row].id and request.difficulty == difficulty
            and request.savegameIndex == slot, "World boundary lost the selected map/difficulty/slot")
        assert(g_gui.currentGuiName == "MapSelectionScreen", "Scene launch plan unexpectedly changed the frontend screen")
        click(element("MapSelectionScreen", 3))
        assert(g_gui.currentGuiName == "DifficultyScreen", "Map Back did not restore DifficultyScreen")
        click(element("DifficultyScreen", 4))
        assert(g_gui.currentGuiName == "CareerScreen", "Difficulty Back did not restore CareerScreen")
    end
    click(element("CareerScreen", 4))
    assert(g_gui.currentGuiName == "MainScreen", "Career Back did not restore MainScreen")
    print("New-game input checks: recovered difficulty Back; all three difficulty buttons; both base-map previews and mouse selection; map/difficulty/slot preserved at guarded world boundary; map and difficulty Back")
end
