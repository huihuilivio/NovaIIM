# server - NovaIIM 服务端

nova_collect_sources(${CMAKE_CURRENT_SOURCE_DIR} SERVER_SOURCES)

if(SERVER_SOURCES)
    add_executable(im_server ${SERVER_SOURCES})
    nova_set_compiler_options(im_server)
    target_include_directories(im_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(im_server PRIVATE
        nova_protocol
        # libhv
        # spdlog
        # ormpp
    )
else()
    message(STATUS "[NovaIIM] Server sources not yet added, skipping im_server target")
endif()
