#include "00Names.hpp"
#include "clipp.h"

#include <iostream>

// Windows: g++ -std=c++17 -Ofast src\*.cpp -o bin\OreBuild.exe -static
// Linux: g++ -std=c++17 -Ofast src/*.cpp -o bin/OreBuild

std::filesystem::path libdirPath;
std::string platform, configuration;

int main(int argc, char** argv) {
  libdirPath = std::filesystem::absolute(getProgramPath().parent_path() / "libraries");

  configuration = "Debug";
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
  platform = "Windows";
#else
  platform = "Linux";
#endif

  // * Parse cli arguments
  using namespace clipp;
  enum class Mode {
    Build,
    Rebuild,
    Run,
    Search,
    Install,
    Help,
  } mode = Mode::Help;

  // clang-format off
  auto buildMode = (
    command("build").set(mode, Mode::Build) | command("rebuild").set(mode, Mode::Rebuild) | command("run").set(mode, Mode::Run),
    (option("-c", "--conf") & value("configuration", configuration)) % "Set the configuration, example Debug or Linux:Debugs"
  );

  std::string package;
  auto searchMode = (
    command("search").set(mode, Mode::Search),
    value("query", package)
  );

  auto installMode = (
    command("install").set(mode, Mode::Install),
    value("github package ID", package)
  );

  auto cli = (
    (buildMode | searchMode | installMode | command("help").set(mode, Mode::Help)),
    option("-v", "--version").call([] {puts("Version 3.0\n");}).doc("Show version")
  );
  // clang-format on

  if (parse(argc, argv, cli)) {
    if (mode == Mode::Help) std::cout << make_man_page(cli, "OreBuild");
    else if (mode == Mode::Search) searchPackage(package);
    else if (mode == Mode::Install) installPackage(package);
    else {
      auto colon = configuration.find(':');
      if (colon != std::string::npos) {
        platform = configuration.substr(0, colon);
        configuration = configuration.substr(colon + 1);
      }

      bool skip = true;
      if (mode == Mode::Rebuild) rebuild = true;
      if (mode == Mode::Build || mode == Mode::Rebuild) buildModule(std::filesystem::absolute("project.orebuild"), skip);
      else if (mode == Mode::Run) return !execute(buildModule(std::filesystem::absolute("project.orebuild"), skip)[0]) * -1;
    }
  } else std::cout << usage_lines(cli, "OreBuild") << '\n';
  return 0;
}
