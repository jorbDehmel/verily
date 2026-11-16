/**
 * @brief Version / package manager for verily
 *
 * Hark! Hark, Triton, Hark! Bellow, bid our father the Sea king
 * rise from the depths full foul in his fury black waves
 * teeming with salt foam to smother this young mouth with
 * pungent slime, to choke ye, engorging your organs 'til ye
 * turn blue and bloated with bilge and brine and can scream no
 * more- Only when he, crowned in cockle shells with slithering
 * tentacle tail and steaming beard- take up his fell be-finned
 * arm, his coral-tine trident screeches banshee-like in the
 * tempest, and plunges right through yer gullet, bursting ye- A
 * bulging bladder no more, but a blasted bloody film now, and
 * nothing for the harpies and the souls of dead sailors to peck
 * and claw and feed upon, only to be lapped up and swallowed by
 * the infinite waters of the Dread Emperor himself- Forgotten
 * to any man, to any time, forgotten to any god or devil,
 * forgotten even to the sea, for any stuff or part of Winslow,
 * even any scantling of your soul is Winslow no more, but is
 * now, itself, the sea!
 */

#include "src/packages.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

int main(int c, char *v[]) {
  PackageManager pm;
  if (c < 2) {
    std::cout
        << "Arguments are required: See --help for more.\n";
  }

  for (int i = 1; i < c; ++i) {
    const std::string arg = v[i];
    if (arg == "--help") {
      // clang-format off
      std::cout <<
        "Hark: A version / package manager for the verily theorem prover  \n"
        " CLI flag            | Arguments       | Function                \n"
        "---------------------|-----------------|-------------------------\n"
        " --help              | 0               | Print this              \n"
        " --ls                | 1: name         | List package versions   \n"
        " --add               | 1: url          | Add package from URL    \n"
        " --add-local         | 1: path         | Add package from path   \n"
        " --remove            | 2: name version | Remove package          \n"
        " --ls-versions       | 0               | List verily versions    \n"
        " --add-version       | 1: url          | Add verily version      \n"
        " --add-local-version | 1: url          | Add local verily version\n"
        " --remove-version    | 1: version      | Remove verily version   \n"
        " --set-version       | 1: version      | Set verily version      \n"
        " --purge             | 0               | Uninstall verily        \n"
        "MIT License, 2025-2026. Version " << version << "\n"
      ;
      // clang-format on
    } else if (arg == "--version") {
      std::cout << version << '\n';
    }

    else if (arg == "--ls") {
      assert(i + 1 < c);
      const std::string name = v[++i];
      const auto versions = pm.all_versions(name);
      std::cout << "Package '" << name << "' has version(s):\n";
      for (const auto &v : versions) {
        std::cout << "- " << v << '\n';
      }
    } else if (arg == "--add") {
      assert(i + 1 < c);
      const std::string url = v[++i];
      std::cout << "Adding package w/ URL " << url << '\n';
      pm.add_package(url);
    } else if (arg == "--add-local") {
      assert(i + 1 < c);
      const std::string path = v[++i];
      std::cout << "Adding package from local directory "
                << path << '\n';
      pm.add_local_package(path);
    } else if (arg == "--remove") {
      assert(i + 2 < c);
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
      assert(i + 1 < c);
      const std::string url = v[++i];
      std::cout << "Adding verily version '" << url << "'\n";
      pm.add_verily_version(url);
    } else if (arg == "--add-local-version") {
      assert(i + 1 < c);
      const std::filesystem::path path = v[++i];
      std::cout << "Adding verily version '" << path << "'\n";
      pm.add_local_verily_version(path);
    } else if (arg == "--remove-version") {
      assert(i + 1 < c);
      const auto version = PackageVersion::from_str(v[++i]);
      std::cout << "Removing verily version '" << version
                << "'\n";
      pm.remove_verily_version(version);
    } else if (arg == "--set-version") {
      assert(i + 1 < c);
      const auto version = PackageVersion::from_str(v[++i]);
      std::cout << "Setting verily version to '" << version
                << "'\n";
      pm.set_verily_version(version);
    } else if (arg == "--purge") {
      std::cout << "Are you sure you want to erase all verily "
                   "versions and packages? [y/N] ";
      char answer;
      std::cin >> answer;
      if (answer == 'y') {
        std::filesystem::remove_all(pm.packages_path);
      } else {
        std::cerr << "Aborted.\n";
      }
    }

    else {
      std::cerr << "Ignoring unknown command '" << arg
                << "': See --help\n";
    }
  }
  return 0;
}
