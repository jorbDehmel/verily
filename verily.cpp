/**
 * @brief Tests the inference maker object
 */

#include "src/core.hpp"
#include "src/inference.hpp"
#include "src/parse.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

const std::string version = "0.1.0";

void print_help() {
  // clang-format off
  std::cout <<
    "+-----------------------------------------------------+\n"
    "|                       Verily                        |\n"
    "+-----------------------------------------------------+\n"
    "A deductive theorem prover. MIT License, 2025-2026.    \n"
    "                                                       \n"
    " CLI flag          | Default | Meaning                 \n"
    "-------------------|---------|-------------------------\n"
    " --debug           | false   | Toggles debug mode      \n"
    " --help            |         | Prints this text        \n"
    " --json            | false   | Prints json to file     \n"
    " --quiet           | false   | Toggles quiet mode      \n"
    "                                                       \n"
    "You can give it a filepath as an argument, in which    \n"
    "case that file will be analyzed. If no filepath is     \n"
    "provided, it will read from stdin in a REPL interface. \n"
    "                                                       \n"
    "Version " << version << "\n"
  ;
  // clang-format on
}

int main(int argc, char *argv[]) {
  bool time = false;
  bool print_json = false;

  std::filesystem::path fp = null_fp;
  Core verily;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--debug") {
      verily.debug = !verily.debug;
      verily.im.debug = verily.debug;
      verily.parser.debug = verily.debug;
    } else if (arg == "--time") {
      time = !time;
    } else if (arg == "--json") {
      print_json = !print_json;
    } else if (arg == "--quiet") {
      verily.im.quiet = !verily.im.quiet;
    } else if (arg == "--help") {
      print_help();
      return 1;
    }

    else if (arg.starts_with("--")) {
      std::cout << "Unknown flag '" << arg
                << "'. Use '--help' to get help.\n";
      return 2;
    }

    else {
      fp = arg;
    }
  }

  verily.handle_setting =
      [&](Core &_c, const std::string &_setting) -> void {
    if (_setting == "debug") {
      _c.debug = !_c.debug;
      _c.im.debug = _c.debug;
      verily.parser.debug = verily.debug;
    } else if (_setting == "json") {
      print_json = !print_json;
    } else if (_setting == "time") {
      time = !time;
    } else if (_setting == "quiet") {
      _c.im.quiet = !_c.im.quiet;
    }

    else if (_setting == "debug=true") {
      _c.debug = true;
      _c.im.debug = _c.debug;
      verily.parser.debug = verily.debug;
    } else if (_setting == "debug=false") {
      _c.debug = false;
      _c.im.debug = _c.debug;
      verily.parser.debug = verily.debug;
    } else if (_setting == "json=true") {
      print_json = true;
    } else if (_setting == "json=false") {
      print_json = false;
    } else if (_setting == "time=true") {
      time = true;
    } else if (_setting == "time=false") {
      time = false;
    } else if (_setting == "time=quiet") {
      _c.im.quiet = true;
    } else if (_setting == "time=quiet") {
      _c.im.quiet = false;
    }

    else if (!_c.im.quiet) {
      std::cout << "WARNING: Unknown setting " << _setting
                << "\n";
    }
  };

  std::chrono::high_resolution_clock::time_point start, stop;
  if (fp != null_fp) {
    // File mode
    if (time) {
      start = std::chrono::high_resolution_clock::now();
    }
    verily.do_file(fp);
    if (time) {
      stop = std::chrono::high_resolution_clock::now();
    }
  }

  else {
    // CLI mode
    if (time) {
      std::cout << "WARNING: Cannot time in CLI mode\n";
      time = false;
    } else if (verily.im.quiet) {
      std::cout
          << "WARNING: Cannot use '--quiet' in CLI mode\n";
      verily.im.quiet = false;
    }

    std::cout << "Verily CLI mode: CTL+D / EOF to exit.\n";

    std::string cur_statement;
    bool is_running = true;
    while (!std::cin.eof() && is_running) {
      std::string line;
      std::cout << "> ";
      std::getline(std::cin, line);
      if (!cur_statement.empty()) {
        cur_statement += '\n';
      }
      cur_statement += line;

      if (cur_statement.ends_with(';')) {
        // Execute statement
        if (verily.debug) {
          std::cout << "Processing CLI statement "
                    << cur_statement << "\n";
        }

        try {
          verily.append_parse(cur_statement, fp);
          const ASTNode global = verily.parse_and_process(fp);

          for (const auto &stmt : global.children) {
            if (stmt == ASTNode("HELP")) {
              print_help();
            } else if (stmt == ASTNode("QUIT")) {
              is_running = false;
              break;
            } else if (stmt.text != "NULL") {
              verily.process_statement(stmt, fp);
            }
          }
        } catch (std::runtime_error &_e) {
          std::cout << "Caught error: " << _e.what() << "\n";
        } catch (...) {
          std::cout << "Unknown error!\n";
        }

        // Clear
        cur_statement.clear();
      }
    }
    if (!cur_statement.empty()) {
      std::cout << "WARNING: Discarding partial statement "
                << cur_statement << "\n";
    }
  }

  if (verily.debug) {
    verily.ls();
  }

  if (!verily.im.quiet) {
    std::cout << "\n";
    for (const auto &index : verily.proven_theorems) {
      std::cout << verily.im.proof_to_ast(index) << "\n\n";
    }
  }

  if (time) {
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            stop - start)
            .count();
    std::cout << "Took " << (elapsed_us / 1'000.0) << "ms\n"
              << "Total theorems: " << verily.im.facts.size()
              << "\n"
              << "Mean theorems per second: "
              << (1'000'000.0 * verily.im.facts.size() /
                  elapsed_us)
              << "\n";
  }

  if (print_json) {
    std::ofstream f(fp.string() + ".json");
    if (!f.is_open()) {
      std::cout << "Failed to open json file\n";
      return 4;
    }
    verily.json(f);
  }

  if (!verily.im.goals.empty()) {
    std::cerr << "Pending (unproven) theorems:\n";
    for (const auto &p : verily.im.goals) {
      std::cerr << p << '\n';
    }
  }

  if (verily.saw_error) {
    return 5;
  }
  return 0;
}
