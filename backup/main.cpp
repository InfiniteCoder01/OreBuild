#include "common.hpp"

std::map<std::string, std::map<std::string, std::string>> modules;
std::map<std::string, std::string>* platform;

void instantiate(std::string& command, const std::string& variable, std::string value) {
  size_t idx = command.find(variable);
  if (idx == std::string::npos) {
    if (value.empty()) return;
    fprintf(stderr, "Missing command parameter: %s!\n", variable.c_str());
    fflush(stderr);
    exit(-1);
  }
  if (command[idx - 1] == '*') {
    size_t from = command.find_last_of(' ', idx) + 1;
    std::string param = command.substr(from, idx - from - 1);
    value = replace(value, " ", " " + param);
    if (!value.empty()) value.erase(value.begin());
    command.replace(command.begin() + from, command.begin() + idx + variable.size(), value);
  } else {
    value.erase(value.begin());
    command = replace(command, variable, value);
  }
}

std::pair<std::string, std::string> buildModule(std::string name) {
  std::map<std::string, std::string>& module = modules[name];
  std::string output = getOrDefault(module, std::string("output"), "build/" + name);
  std::string suffix = (name != "Main" && !module.count("output")) ? ".o" : "";

  std::string includes;
  std::vector<std::string> files;
  for (std::string file : wildcard(module["files"])) {
    files.push_back(file);
  }

  includes = getOrDefault(module, std::string("include"), std::string(""));
  bool skip = true;
  for (std::string module : split(getOrDefault(module, std::string("submodule"), std::string("")))) {
    std::pair<std::string, std::string> out = buildModule(module.c_str());
    if (out.first.empty()) return out;
    includes += ' ' + out.first;
    if(out.second[0] == '0') skip = false;
    files.push_back(out.second.substr(1));
  }

  printf("Building module %s...", name.c_str());
  std::string command = (name == "Main" ? (*platform)["command"] : getOrDefault(*platform, std::string("submoduleCommand"), (*platform)["command"]));
  command += " " + getOrDefault(module, std::string("flags"), std::string(""));
  instantiate(command, "$include", includes);
  bool success = true;
  if (command[0] == '*') {
    bool nl = false;
    output += '/';
    command.erase(command.begin());
    std::string bins;
    for (std::string file : files) {
      std::string bin = output + file.substr(file.find_last_of("/\\") + 1) + suffix;
      bins += ' ' + bin;
      if (lastModified(bin) >= lastModified(file)) continue;
      else if (!nl) {
        puts("");
        skip = false;
        nl = true;
      }
      std::string cmd = replace(replace(command, "$files", file), "$out", bin);
      if (cmd[0] == '@') cmd.erase(cmd.begin());
      else puts(cmd.c_str());
      if (system(cmd.c_str()) != 0) {
        success = false;
        break;
      }
    }
    output = bins;
  } else {
    output += suffix;
    command = replace(command, "$out", output);
    std::string filesS;
    for (std::string file : files) {
      filesS += ' ' + file;
      if (file.substr(file.size() - 2) != ".o" && lastModified(output) < lastModified(file)) skip = false;
    }
    if (!skip) {
      puts("");
      instantiate(command, "$files", filesS);
      if (command[0] == '@') command.erase(command.begin());
      else puts(command.c_str());
      success = system(command.c_str()) == 0;
    }
  }

  if (skip) puts(" Skipped!");
  if (!success) return std::make_pair<std::string, std::string>("", "");
  return std::make_pair(getOrDefault(module, std::string("path"), std::string(name)), std::to_string(skip) + output);
}

int main(int argc, char** argv) {
  std::map<std::string, std::map<std::string, std::string>> platforms;
  std::string currentPlatform = "", defaultPlatform = "Windows";
  std::string currentModule = "";

  if (argc == 2) defaultPlatform = argv[1];
  else if (argc != 1) {
    fprintf(stderr, "Usage: OreBuild [Platform Name]\n");
    fflush(stderr);
    exit(-1);
  }

  FILE* file = fopen("OreBuild", "r");
  while (true) {
    skipSpaces(file);
    if (feof(file)) break;
    {
      int c = fgetc(file);
      if (c == '/' && fpeek(file) == '/') {
        while (fgetc(file) != '\n') continue;
        continue;
      }
      ungetc(c, file);
    }
    if ((!currentPlatform.empty() || !currentModule.empty()) && fpeek(file) == '}') {
      fgetc(file);
      currentPlatform = currentModule = "";
      continue;
    }
    std::string item = readID(file);
    if (currentPlatform.empty() && currentModule.empty()) {
      if (item == "platform") {
        std::string platform = readString(file);
        if (platform.empty()) error(file, "Platform name is empty!");
        expect(file, '{');
        currentPlatform = platform;
      } else if (item == "module") {
        std::string module = readString(file);
        if (module.empty()) error(file, "Module name is empty!");
        expect(file, '{');
        currentModule = module;
      } else {
        error(file, "Undefined token: " + item);
      }
    } else if (currentModule.empty()) {
      expect(file, ':');
      std::string value = readString(file);
      expect(file, ';');
      platforms[currentPlatform][item] = value;
    } else {
      expect(file, ':');
      std::string value = readString(file);
      expect(file, ';');
      bool space = item == "submodule" || item == "include";
      if (modules[currentModule].count(item)) modules[currentModule][item] += (space ? " " : "") + value;
      else modules[currentModule][item] = (space && item != "submodule" ? " " : "") + value;
    }
  }
  fclose(file);
  printf("Building for %s:\n", defaultPlatform.c_str());
  platform = &platforms[defaultPlatform];
  if (!modules.count("Main")) {
    fprintf(stderr, "Error: No 'Main' module found!\n");
    fflush(stderr);
    exit(-1);
  }
  if (buildModule("Main").first.empty()) puts("Build failed!");
  else puts("Build succed!");
  return 0;
}
