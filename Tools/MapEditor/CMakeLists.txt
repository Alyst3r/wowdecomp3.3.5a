if (WOWDECOMP_ARCH_64)
    set(MAPEDITOR_OUTPUT_NAME MapEditor64)
else ()
    set(MAPEDITOR_OUTPUT_NAME MapEditor32)
endif ()

file(GLOB MAPEDITOR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB MAPEDITOR_CLIENT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/WoW/Source/*.cpp")

if (WOWDECOMP_ARCH_64)
    file(GLOB MAPEDITOR_PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/WoW/Source/Win64/*.cpp")
else ()
    file(GLOB MAPEDITOR_PLATFORM_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/WoW/Source/Win32/*.cpp")
endif ()

list(FILTER MAPEDITOR_PLATFORM_SOURCES EXCLUDE REGEX "WinMain[.]cpp$")

add_executable(MapEditor ${MAPEDITOR_SOURCES} ${MAPEDITOR_CLIENT_SOURCES} ${MAPEDITOR_PLATFORM_SOURCES})

add_custom_command(TARGET MapEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Interface ${CMAKE_SOURCE_DIR}/bin/Win/Interface
)

set_target_properties(MapEditor PROPERTIES
    OUTPUT_NAME ${MAPEDITOR_OUTPUT_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Win
)

foreach (config ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
    string(TOUPPER ${config} config)
    set_target_properties(MapEditor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_SOURCE_DIR}/bin/Win)
endforeach ()

target_include_directories(MapEditor
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_options(MapEditor PRIVATE "/INCREMENTAL:NO" "/DEBUG")

target_link_libraries(MapEditor
    PRIVATE
        BLPFile
        Base
        BlizzardCore
        ComSat
        ConsoleBase
        Event
        FFX
        Frame
        FrameScript
        lua
        FrameXML
        Gx
        Gxu
        Images
        Model2
        Os
        Scrn
        Services
        ShaderEffect
        Tempest
        SoundEngine
        SteelseriesMouse
        Storm
        WoWCommon
        Battlenet
        Console
        ConsoleDevice
        DB
        Game
        Glue
        Magic
        Minigame
        Net
        Object
        ObjectAlloc
        ObjectMgrClient
        SoundInterface
        UIUtil
        Ui
        WorldClient
        LiquidSystem
        WowServices
        WowSvcs
        dbghelp
)
