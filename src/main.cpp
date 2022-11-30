#include "common.hpp"

const std::vector<std::string> platformItems = {"command", "submoduleCommand", "flags"};  // TODO: platform flags
const std::vector<std::string> moduleItems = {"path", "includePath", "files", "submodule", "output", "include", "flags", "command"};
const std::vector<std::string> moduleKeeps = {"command"};
const std::vector<std::string> moduleRepeats = {"path", "includePath", "files", "submodule", "include", "flags", "command"};
const std::vector<std::string> moduleWildcards = {"path", "includePath", "files", "include"};

void validate(FILE* file, const std::string& item, const std::vector<std::string>& valids, const std::string& err = "Invalid item: ") {
  if (!std::count(valids.begin(), valids.end(), item)) error(file, err + item + "!");
}

std::map<std::string, std::map<std::string, std::vector<std::string>>> modules;
std::map<std::string, std::vector<std::string>> platform;
std::vector<std::string> objects;

bool execute(std::string command) {
  if (command[0] == '@') command.erase(command.begin());
  else puts(command.c_str());
  return system(command.c_str()) == 0;
}

void insert(std::string& command, const std::string& name, const std::vector<std::string>& values) {
  size_t pos = command.find(name);
  if (pos == std::string::npos) error("Build command has no " + name + " attribute!");
  if (pos > 0 && command[pos - 1] == '*') {
    size_t start = command.find_last_of(" ", pos) + 1;
    std::string prefix = command.substr(start, pos - start - 1);
    std::string result;
    bool first = true;
    for (const auto& value : values) {
      if (!first) result += ' ';
      result += prefix + value;
      first = false;
    }
    command.replace(command.begin() + start, command.begin() + pos + name.length(), result);
  } else {
    std::string value;
    bool first = true;
    for (const auto& val : values) {
      if (!first) value += ' ';
      value += val;
      first = false;
    }
    command.replace(command.begin() + pos, command.begin() + pos + name.length(), value);
  }
}

std::pair<bool, bool> buildModule(std::string name) {
  if (name == "Main") objects.clear();
  std::map<std::string, std::vector<std::string>>& current = modules[name];
  if (!current.count("files") || current["files"].empty()) return std::pair(true, true);

  std::vector<std::string> includes = getOrDefault(current, std::string("include"), std::vector<std::string>{});
  std::vector<std::string> files = current["files"];
  bool skip = true;
  for (const auto& submodule : getOrDefault(current, std::string("submodule"), std::vector<std::string>{})) {
    std::pair<bool, bool> result = buildModule(submodule);
    if (!result.first) return std::pair(false, false);
    if (!result.second) skip = false;
    for (const auto& include : getOrDefault(modules[submodule], std::string("includePath"), std::vector<std::string>{})) includes.push_back(include);
  }

  if (name == "Main") {
    for (const auto& object : objects) files.push_back(object);
  }

  std::vector<std::string> flags = getOrDefault(current, std::string("flags"), std::vector<std::string>{});
  std::vector<std::string> commands = getOrDefault(current, std::string("command"), (name != "Main" && platform.count("submoduleCommand")) ? platform["submoduleCommand"] : platform["command"]);
  std::string singleOutput = getOrDefault(current, std::string("output"), std::vector<std::string>{"build/" + name})[0];
  std::map<std::string, std::string> output;
  bool buildSingle = false, buildAll = false;
  for (const auto& command : commands) {
    if (command[0] == '*') buildAll = true;
    else buildSingle = true;
    if (buildSingle && buildAll) break;
  }
  if (buildSingle) {
    if (name != "Main") objects.push_back(singleOutput);
    for (const auto& file : files) {
      if (lastModified(singleOutput) <= lastModified(file)) {
        skip = false;
      }
    }
  }
  if (buildAll) {
    for (const auto& file : files) {
      std::string out = singleOutput + '/' + file.substr(file.find_last_of("/\\") + 1) + ".o";
      if (name != "Main") objects.push_back(out);
      if (lastModified(out) <= lastModified(file)) {
        skip = false;
        output[file] = out;
      }
    }
  }

  if (skip) return std::pair(true, true);
  printf("Building module %s...\n", name.c_str());
  for (auto command : commands) {
    insert(command, "$include", includes);
    for (const auto& flag : flags) command += " " + flag;
    if (command[0] == '*') {
      command.erase(command.begin());
      for (const auto& file : output) {
        if (!execute(replace(replace(command, "$files", file.first), "$out", file.second))) return std::pair(false, false);
      }
    } else {
      insert(command, "$files", files);
      if (!current.count("output")) singleOutput += ".o";
      if (!execute(replace(command, "$out", singleOutput))) return std::pair(false, false);
    }
  }
  return std::pair(true, true);
}

int main(int argc, char** argv) {
  std::string currentPlatform, buildPlatform = "Windows";
  std::string currentModule;

  if (argc == 2) buildPlatform = argv[1];
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
        if (platform.empty() || platform.find(' ') != std::string::npos) error(file, "Platform name is invalid!");
        expect(file, '{');
        currentPlatform = platform;
      } else if (item == "module") {
        std::string module = readString(file);
        if (module.empty() || module.find(' ') != std::string::npos) error(file, "Module name is invalid!");
        expect(file, '{');
        currentModule = module;
      } else {
        error(file, "Undefined token: " + item);
      }
    } else if (!currentPlatform.empty()) {
      validate(file, item, platformItems);
      expect(file, ':');
      std::string value = readString(file);
      expect(file, ';');
      if (currentPlatform == buildPlatform) {
        if (platform.count(item)) platform[item].push_back(value);
        else platform[item] = std::vector<std::string>{value};
      }
    } else if (!currentModule.empty()) {
      validate(file, item, moduleItems);
      expect(file, ':');
      std::string rawValue = readString(file);
      expect(file, ';');
      std::vector<std::string> values;
      if (std::count(moduleKeeps.begin(), moduleKeeps.end(), item)) values = std::vector<std::string>{rawValue};
      else {
        for (auto value : split(rawValue)) {
          for (const auto& var : moduleItems) {
            if (modules[currentModule].count(var)) value = replace(value, '$' + var, modules[currentModule][var][0].c_str());
          }
          if (std::count(moduleWildcards.begin(), moduleWildcards.end(), item)) {
            for (const auto& wildcarded : wildcard(value, item != "files")) values.push_back(wildcarded);
          } else values.push_back(value);
        }
      }
      if (modules[currentModule].count(item)) {
        validate(file, item, moduleRepeats, "Repeated property: ");
        for (const auto& value : values) modules[currentModule][item].push_back(value);
      } else modules[currentModule][item] = values;
    } else error(file, "Impossible error!");
  }
  fclose(file);
  printf("Building for %s:\n", buildPlatform.c_str());
  if (!modules.count("Main")) error("Error: No 'Main' module found!\n");
  if (!platform.count("command")) error("Error: No build command for this platform specified!\n");
  // for (const auto& module : modules) {
  //   puts(module.first.c_str());
  //   for (const auto& prop : module.second) {
  //     std::string s;
  //     for (const auto& str : prop.second) {
  //       s += str + ", ";
  //     }
  //     printf("%s: {%s};\n", prop.first.c_str(), s.c_str());
  //   }
  // }

  if (buildModule("Main").first) {
    puts("Build succed!");
    return 0;
  }
  puts("Build failed!");
  return -1;
}
