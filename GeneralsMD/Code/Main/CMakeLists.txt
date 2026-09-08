add_executable(z_generals WIN32)

# target_link_options(z_generals PRIVATE /MAP)

math(EXPR REBORN_OMEGA_BUILD_RANK
    "${REBORN_OMEGA_VERSION_MAJOR} * 10000 + ${REBORN_OMEGA_VERSION_MINOR} * 1000 + ${REBORN_OMEGA_VERSION_PATCH} * 100")

if(REBORN_OMEGA_BETA GREATER 0)
    math(EXPR REBORN_OMEGA_BUILD_RANK "${REBORN_OMEGA_BUILD_RANK} + ${REBORN_OMEGA_BETA}")
else()
    math(EXPR REBORN_OMEGA_BUILD_RANK "${REBORN_OMEGA_BUILD_RANK} + 99")
endif()

if(RTS_BUILD_OPTION_REBORN)
    add_custom_target(PrepareRebornOmegaData
        COMMAND cmd /c powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/PrepareRebornOmegaData.ps1"
            "${REBORN_OMEGA_DISPLAY_NAME}"
        USES_TERMINAL
        COMMENT "Preparing RebornOmegaData"
    )

    add_dependencies(PrepareRebornOmegaData
        z_generals
        z_worldbuilder
    )
endif()

add_custom_target(BuildRebornOmegaInstaller
    COMMAND cmd /c powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_SOURCE_DIR}/scripts/BuildRebornOmegaInstaller.ps1"
        "${REBORN_OMEGA_DISPLAY_NAME}"
        "${REBORN_OMEGA_BUILD_RANK}"
    DEPENDS z_generals z_worldbuilder
    USES_TERMINAL
    COMMENT "Building Reborn Omega installer"
)

# Use a binary name that doesn't conflict with original game.
if("${CMAKE_SYSTEM}" MATCHES "Windows")
    if(RTS_BUILD_OPTION_REBORN)
        set_target_properties(z_generals PROPERTIES
            OUTPUT_NAME "$<$<CONFIG:Debug>:${REBORN_OMEGA_DISPLAY_NAME} (Debug)>$<$<NOT:$<CONFIG:Debug>>:${REBORN_OMEGA_DISPLAY_NAME}${RTS_BUILD_OUTPUT_SUFFIX}>"
        )
    else()
        set_target_properties(z_generals PROPERTIES
            OUTPUT_NAME "$<$<CONFIG:Debug>:generalszh (Debug)>$<$<NOT:$<CONFIG:Debug>>:generalszh${RTS_BUILD_OUTPUT_SUFFIX}>"
        )
    endif()
else()
    set_target_properties(z_generals PROPERTIES OUTPUT_NAME "generalszh${RTS_BUILD_OUTPUT_SUFFIX}")
endif()

target_link_libraries(z_generals PRIVATE
    binkstub
    comctl32
    d3d8
    d3dx8
    dinput8
    dxguid
    imm32
    milesstub
    vfw32
    winhttp
    winmm
    z_gameengine
    z_gameenginedevice
    zi_always
)

# TODO Originally referred to build host and user, replace with git info perhaps?
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/GeneratedVersion.h
"#pragma once

#define VERSION_LOCALBUILDNUM 0
#define VERSION_BUILDUSER \"\"
#define VERSION_BUILDLOC \"\"
"
)

# Based on original binary values for these variables.
file(WRITE ${CMAKE_BINARY_DIR}/BuildVersion.h
"#pragma once

#define VERSION_MAJOR 1
#define VERSION_MINOR 4
#define VERSION_BUILDNUM 601
#define REBORN_OMEGA_VERSION_MAJOR ${REBORN_OMEGA_VERSION_MAJOR}
#define REBORN_OMEGA_VERSION_MINOR ${REBORN_OMEGA_VERSION_MINOR}
#define REBORN_OMEGA_VERSION_PATCH ${REBORN_OMEGA_VERSION_PATCH}
#define REBORN_OMEGA_VERSION_BETA ${REBORN_OMEGA_BETA}
#define REBORN_OMEGA_BUILD_RANK ${REBORN_OMEGA_BUILD_RANK}
#define REBORN_OMEGA_VERSION_TEXT L\"${REBORN_OMEGA_DISPLAY_NAME}\"
#define REBORN_OMEGA_VERSION_TEXT_RC \"${REBORN_OMEGA_DISPLAY_NAME}\"
"
)

if(MSVC)
    target_link_options(z_generals PRIVATE "/NODEFAULTLIB:libci.lib")

    if(RTS_BUILD_OPTION_REBORN)
        # Reborn: Load the direct GO imports through the isolated RebornOmegaData delay-load hook.
        target_link_options(z_generals PRIVATE
            "/DELAYLOAD:GameNetworkingSockets.dll"
            "/DELAYLOAD:libcurl.dll")
        target_link_libraries(z_generals PRIVATE delayimp)
        target_sources(z_generals PRIVATE RebornOmegaDllLoad.cpp)
    endif()
endif()

target_include_directories(z_generals PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

target_sources(z_generals PRIVATE
    WinMain.cpp
    WinMain.h
)

if(WIN32 AND RTS_BUILD_OPTION_REBORN)
    set(GENERALS_ONLINE_VENDOR_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../GameEngine/Source/GameNetwork/GeneralsOnline/Vendor")

    # Reborn: Keep the private GO transport and HTTP runtime under RebornOmegaData.
    add_custom_command(TARGET z_generals POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:z_generals>/RebornOmegaData")

    foreach(GENERALS_ONLINE_RUNTIME IN ITEMS
        "DiscordRPC/discord-rpc.dll"
        "libcurl/libcurl.dll"
        "libcurl/zlib1.dll"
        "ValveNetworkingSockets/GameNetworkingSockets.dll"
        "ValveNetworkingSockets/abseil_dll.dll"
        "ValveNetworkingSockets/libprotobuf.dll"
        "ValveNetworkingSockets/libcrypto-3.dll"
        "ValveNetworkingSockets/libssl-3.dll")
        add_custom_command(TARGET z_generals POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GENERALS_ONLINE_VENDOR_DIR}/${GENERALS_ONLINE_RUNTIME}"
                "$<TARGET_FILE_DIR:z_generals>/RebornOmegaData")

    endforeach()

    add_custom_command(TARGET z_generals POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GENERALS_ONLINE_VENDOR_DIR}/DiscordRPC/LICENSE.txt"
            "$<TARGET_FILE_DIR:z_generals>/RebornOmegaData/discord-rpc-LICENSE.txt")
endif()

# RC files optional for MinGW builds
# Icon: LoadIcon() handles missing resource gracefully (uses default system icon)
# Manifest: DPI awareness metadata (nice to have but not essential)
if(MSVC)
    # VS2005 and later adds default manifest, we need to turn it off to prevent conflict with custom manifest
    if(NOT IS_VS6_BUILD)
        target_link_options(z_generals PRIVATE
            "/MANIFEST:NO"
        )
    endif()

    target_sources(z_generals PRIVATE
        RTS.RC
    )
endif()

# Strip debug symbols to separate file for MinGW Release builds
# This creates generalszh${RTS_BUILD_OUTPUT_SUFFIX}.exe.debug (similar to MSVC .pdb files)
if(MINGW AND COMMAND add_debug_strip_target)
    add_debug_strip_target(z_generals)
endif()
