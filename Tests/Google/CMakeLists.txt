add_subdirectory(Dependencies)
add_subdirectory(Core)

if(RTS_BUILD_ZEROHOUR)
    add_executable(z_googletest)
    target_link_libraries(z_googletest PRIVATE
        corei_googletest
        z_gameenginedevice
        z_wwaudio
        zi_always
    )
    # Stage the x86 Generals Online runtime beside the test executable.
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(go_test_vendor_dir
            "${CMAKE_CURRENT_SOURCE_DIR}/../../GeneralsMD/Code/GameEngine/Source/GameNetwork/GeneralsOnline/Vendor")

        foreach(dll IN ITEMS
            libcurl/libcurl.dll
            libcurl/zlib1.dll
            ValveNetworkingSockets/GameNetworkingSockets.dll
            ValveNetworkingSockets/abseil_dll.dll
            ValveNetworkingSockets/libprotobuf.dll
            ValveNetworkingSockets/libcrypto-3.dll
            ValveNetworkingSockets/libssl-3.dll
        )
            add_custom_command(TARGET z_googletest POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${go_test_vendor_dir}/${dll}"
                    "$<TARGET_FILE_DIR:z_googletest>"
                VERBATIM
            )
        endforeach()
    endif()
    add_test(NAME z_googletest COMMAND z_googletest)
endif()

if(RTS_BUILD_GENERALS)
    add_executable(g_googletest)
    target_link_libraries(g_googletest PRIVATE
        corei_googletest
        g_gameenginedevice
        g_wwaudio
        gi_always
    )
    add_test(NAME g_googletest COMMAND g_googletest)
endif()
