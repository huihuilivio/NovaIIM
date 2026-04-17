#include "core/application.h"
#include "core/app_config.h"

#include <CLI/CLI.hpp>

#include <string>

int main(int argc, char* argv[]) {
    CLI::App app{"NovaIIM Server - High-performance IM backend"};
    app.set_version_flag("-v,--version", "0.1.0");

    std::string config_path;
    app.add_option("-c,--config", config_path, "Path to config YAML file")->required()->check(CLI::ExistingFile);

    CLI11_PARSE(app, argc, argv);

    nova::AppConfig cfg;
    if (!nova::LoadConfig(cfg, config_path)) {
        return 1;
    }

    return nova::Application::Run(cfg);
}
