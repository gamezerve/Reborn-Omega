set(OBJECTDEPARSER_SRC
    "src/StdAfx.cpp"
    "src/ObjectDeparser.cpp"
    "src/ObjectDeparserDialog.cpp"
    "include/StdAfx.h"
    "include/ObjectDeparser.h"
    "include/ObjectDeparserDialog.h"
    "include/resource.h"
    "include/TextDiff.h"
    "src/TextDiff.cpp"
)

add_executable(z_objectdeparser WIN32)

target_sources(z_objectdeparser PRIVATE ${OBJECTDEPARSER_SRC})

target_include_directories(z_objectdeparser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    include
    res
    ${CMAKE_SOURCE_DIR}/Dependencies/DTL
)

target_precompile_headers(z_objectdeparser PRIVATE
    [["Precompiled/precompiled.h"]]
    [["StdAfx.h"]]
)

target_link_libraries(z_objectdeparser PRIVATE
    comctl32
    d3d8lib
    imm32
    vfw32
    winmm
    core_browserdispatch
    z_gameengine
    z_gameenginedevice
    zi_always
)

if(WIN32 OR "${CMAKE_SYSTEM}" MATCHES "Windows")
    target_link_options(z_objectdeparser PRIVATE
        /NODEFAULTLIB:libci.lib
        /NODEFAULTLIB:libc.lib
    )

    target_compile_definitions(z_objectdeparser PRIVATE _AFXDLL)

    target_sources(z_objectdeparser PRIVATE
        res/ObjectDeparser.rc
    )

    

    if(MSVC AND RTS_BUILD_OPTION_REBORN)
    target_link_options(z_objectdeparser PRIVATE
        "/DELAYLOAD:GameNetworkingSockets.dll"
        "/DELAYLOAD:libcurl.dll"
    )

    target_link_libraries(z_objectdeparser PRIVATE delayimp)

    target_sources(z_objectdeparser PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Main/RebornOmegaDllLoad.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Main/RebornOmegaDllLoad.h"
    )
    endif()

    if(RTS_BUILD_OPTION_REBORN)
        set_target_properties(
            z_objectdeparser
            PROPERTIES
            OUTPUT_NAME "${REBORN_OMEGA_DISPLAY_NAME} Object Deparser${RTS_BUILD_OUTPUT_SUFFIX}"
        )
    else()
        set_target_properties(
            z_objectdeparser
            PROPERTIES
            OUTPUT_NAME "ObjectDeparserZH${RTS_BUILD_OUTPUT_SUFFIX}"
        )
    endif()
endif()
