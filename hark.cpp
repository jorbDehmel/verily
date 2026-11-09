/**
 * @brief Version / package manager for verily
 */

#include "src/packages.hpp"
#include <cassert>
#include <iostream>

int main(int c, char *v[]) {
  PackageManager pm;
  std::cout << "Hark: A version / package manager for the "
               "verily theorem prover (version "
            << version << ")\n";

  for (int i = 1; i < c; ++i) {
    const std::string arg = v[i];
    if (arg == "--help") {
      // clang-format off
      std::cout <<
        " CLI flag            | Number of arguments  | Function                \n"
        "---------------------|----------------------|-------------------------\n"
        " --help              | 0                    | Print this              \n"
        " --ls                | 1: name              | List package versions   \n"
        " --add               | 3: name version url  | Add package from URL    \n"
        " --add-local         | 3: name version path | Add package from path   \n"
        " --remove            | 2: name version      | Remove package          \n"
        " --ls-versions       | 0                    | List verily versions    \n"
        " --add-version       | 2: version url       | Add verily version      \n"
        " --add-local-version | 2: version url       | Add local verily version\n"
        " --remove-version    | 1: version           | Remove verily version   \n"
        " --set-version       | 1: version           | Set verily version      \n"
        "MIT License, 2025-2026.                                               \n"
      ;
      // clang-format on
    }

    else if (arg == "--ls") {
      assert(i + 1 <= c);
      const std::string name = v[++i];
      const auto versions = pm.all_versions(name);
      std::cout << "Package '" << name << "' has version(s):\n";
      for (const auto &v : versions) {
        std::cout << "- " << v << '\n';
      }
    } else if (arg == "--add") {
      assert(i + 3 <= c);
      const std::string name = v[++i];
      const auto version = PackageVersion::from_str(v[++i]);
      const std::string url = v[++i];
      std::cout << "Adding package '" << name
                << "' from git repo w/ URL " << url << '\n';
      pm.add_package(name, version, url);
    } else if (arg == "--add-local") {
      assert(i + 3 <= c);
      const std::string name = v[++i];
      const auto version = PackageVersion::from_str(v[++i]);
      const std::string path = v[++i];
      std::cout << "Adding package '" << name
                << "' from local directory " << path << '\n';
      pm.add_local_package(name, version, path);
    } else if (arg == "--remove") {
      assert(i + 2 <= c);
      const std::string name = v[++i];
      const auto version = PackageVersion::from_str(v[++i]);
      std::cout << "Removing package '" << name << "'\n";
      pm.remove_package(name, version);
    } else if (arg == "--ls-versions") {
      std::cout << "Verily has versions:\n";
      for (const auto &v : pm.all_verily_versions()) {
        std::cout << "- " << v << '\n';
      }
    } else if (arg == "--add-version") {
      assert(i + 2 <= c);
      const auto version = PackageVersion::from_str(v[++i]);
      const std::string url = v[++i];
      std::cout << "Adding verily version '" << version
                << "'\n";
      pm.add_verily_version(version, url);
    } else if (arg == "--add-local-version") {
      assert(i + 2 <= c);
      const auto version = PackageVersion::from_str(v[++i]);
      const std::filesystem::path path = v[++i];
      std::cout << "Adding verily version '" << version
                << "'\n";
      pm.add_local_verily_version(version, path);
    } else if (arg == "--remove-version") {
      assert(i + 1 <= c);
      const auto version = PackageVersion::from_str(v[++i]);
      std::cout << "Removing verily version '" << version
                << "'\n";
      pm.remove_verily_version(version);
    } else if (arg == "--set-version") {
      assert(i + 1 <= c);
      const auto version = PackageVersion::from_str(v[++i]);
      std::cout << "Setting verily version to '" << version
                << "'\n";
      pm.set_verily_version(version);
    }

    else {
      std::cerr << "Ignoring unknown command '" << arg
                << "': See --help\n";
    }
  }
  return 0;
}
