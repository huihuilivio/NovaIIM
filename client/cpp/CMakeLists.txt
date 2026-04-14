# client - NovaIIM PC客户端

nova_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} CLIENT_SOURCES)

if(CLIENT_SOURCES)
    add_executable(im_client ${CLIENT_SOURCES})
    nova_set_compiler_options(im_client)
    target_include_directories(im_client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(im_client PRIVATE
        nova_protocol
        # Qt6::Quick
        # sqlite3
    )
else()
    message(STATUS "[NovaIIM] Client sources not yet added, skipping im_client target")
endif()
