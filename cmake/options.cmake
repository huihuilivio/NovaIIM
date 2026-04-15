# options.cmake - 项目构建选项

option(NOVA_BUILD_SERVER  "Build NovaIIM server"  ON)
option(NOVA_BUILD_CLIENT  "Build NovaIIM client"  ON)
option(NOVA_BUILD_TESTS   "Build tests"           OFF)
option(NOVA_BUILD_EXAMPLES "Build examples"       OFF)
option(NOVA_ENABLE_MYSQL  "Enable MySQL database backend (requires MySQL client lib)" ON)
