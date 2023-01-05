#include "common.hpp"
#include "json.hpp"
#include <string>
#include <stdio.h>
#define popen _popen
#define pclose _pclose

std::filesystem::path libdirPath;

std::string executeWOut(const std::string& command) {
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
  if (!pipe) error("Failed to execute '%s'!\n", command.c_str());
  std::string result;
  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
    result += buffer;
  }
  return result;
}

void searchPackage(const std::string& name) {
  auto response = nlohmann::json::parse(executeWOut("gh search repos \"" + name + "\" --json fullName --json language --json license"));
  for (const auto& lib : response) {
    std::string language = lib["language"].get<std::string>();
    if (language != "C" && language != "C++") continue;
    puts(lib["fullName"].get<std::string>().c_str());
  }
}

void installPackage(const std::string& name) {
  if (!std::filesystem::exists(libdirPath)) std::filesystem::create_directories(libdirPath);
  std::filesystem::current_path(libdirPath);
  system(("git clone https://github.com/" + name).c_str());
  printf("TODO: add auto-generated library.orebuild, for now go to '%s' and make one yourself!\n", libdirPath.string().c_str());
}
