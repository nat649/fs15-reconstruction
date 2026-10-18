-- Authored subset bootstrap. Executes caller-provided FS15 source; no main.lua,
-- menu drawing, native engine startup or catch-all missing-service fallback.
g_addTestCommands = false
GS_PLATFORM_TYPE_PC, GS_PLATFORM_TYPE_PS3, GS_PLATFORM_TYPE_PS4 = 0, 1, 4
GS_PLATFORM_TYPE = GS_PLATFORM_TYPE_PC
source('dataS/scripts/shared/class.lua')
source('dataS/scripts/Utils.lua')
source('dataS/scripts/I18N.lua')
local catalog = I18N:new()
local labels = {}
for _, key in ipairs({'Button_Career', 'Button_Multiplayer', 'Button_Tutorials',
    'ButtonAchievements', 'Button_Settings', 'Button_Credits', 'Button_Quit'}) do
    assert(catalog:hasText(key), 'Missing main-menu translation: ' .. key)
    labels[key] = catalog:getText(key)
end
fs15_i18n_result = {
    texts = catalog.texts, descriptions = catalog.descriptions, labels = labels,
    fluidFactor = catalog.fluidFactor, powerFactorHP = catalog.powerFactorHP,
    powerFactorKW = catalog.powerFactorKW
}
