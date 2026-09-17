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

const std::string version = "0.0.10";

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
    " --alternate       | false   | Toggles alternation     \n"
    " --debug           | false   | Toggles debug mode      \n"
    " --help            |         | Prints this text        \n"
    " --json            | false   | Prints json to file     \n"
    " --latex           | false   | Prints latex to file    \n"
    " --max_theorems    | 10,000  | Sets the max # theorems \n"
    " --max_tree_height | 100     | Set the max AST height  \n"
    " --meta_prove      | true    | Toggles meta proving    \n"
    " --pass_limit N    | 64      | Sets the depth limit    \n"
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
  bool print_latex = false;
  std::string latex_mode = "tree";
  bool print_json = false;

  std::filesystem::path fp = null_fp;
  Core verily;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--debug") {
      verily.debug = !verily.debug;
      verily.im.debug = verily.debug;
    } else if (arg == "--alternate") {
      verily.im.enable_alternation =
          !verily.im.enable_alternation;
    } else if (arg == "--pass_limit") {
      assert(i + 1 < argc);
      ++i;
      verily.pass_limit = std::stoi(argv[i]);
    } else if (arg == "--max_theorems") {
      assert(i + 1 < argc);
      ++i;
      verily.im.theorem_limit = std::stoi(argv[i]);
    } else if (arg == "--max_tree_height") {
      assert(i + 1 < argc);
      ++i;
      verily.im.max_tree_height = std::stoi(argv[i]);
    } else if (arg == "--time") {
      time = !time;
    } else if (arg == "--latex") {
      print_latex = !print_latex;
    } else if (arg == "--json") {
      print_json = !print_json;
    } else if (arg == "--meta_prove") {
      verily.im.meta_proving = !verily.im.meta_proving;
    } else if (arg == "--quiet") {
      verily.im.quiet = !verily.im.quiet;
    } else if (arg == "--help") {
      print_help();
      return 1;
    } else if (arg == "--latex_mode") {
      assert(i + 1 < argc);
      ++i;
      latex_mode = argv[i];
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
    } else if (_setting == "latex") {
      print_latex = !print_latex;
    } else if (_setting == "json") {
      print_json = !print_json;
    } else if (_setting == "alternate") {
      _c.im.enable_alternation = !_c.im.enable_alternation;
    } else if (_setting == "meta_prove") {
      _c.im.meta_proving = !_c.im.meta_proving;
    } else if (_setting == "time") {
      time = !time;
    } else if (_setting == "quiet") {
      _c.im.quiet = !_c.im.quiet;
    }

    else if (_setting == "debug=true") {
      _c.debug = true;
      _c.im.debug = _c.debug;
    } else if (_setting == "debug=false") {
      _c.debug = false;
      _c.im.debug = _c.debug;
    } else if (_setting == "latex=true") {
      print_latex = true;
    } else if (_setting == "latex=false") {
      print_latex = false;
    } else if (_setting == "json=true") {
      print_json = true;
    } else if (_setting == "json=false") {
      print_json = false;
    } else if (_setting == "alternate=true") {
      _c.im.enable_alternation = true;
    } else if (_setting == "alternate=false") {
      _c.im.enable_alternation = false;
    } else if (_setting == "meta_prove=true") {
      _c.im.meta_proving = true;
    } else if (_setting == "meta_prove=false") {
      _c.im.meta_proving = false;
    } else if (_setting == "time=true") {
      time = true;
    } else if (_setting == "time=false") {
      time = false;
    } else if (_setting == "time=quiet") {
      _c.im.quiet = true;
    } else if (_setting == "time=quiet") {
      _c.im.quiet = false;
    }

    else if (_setting.starts_with("pass_limit=")) {
      const size_t l = std::stoull(_setting.substr(11));
      _c.pass_limit = l;
    } else if (_setting.starts_with("max_tree_height=")) {
      const size_t l = std::stoull(_setting.substr(16));
      _c.im.max_tree_height = l;
    } else if (_setting.starts_with("max_theorems=")) {
      const size_t l = std::stoull(_setting.substr(13));
      _c.im.theorem_limit = l;
    } else if (_setting.starts_with("latex_mode=")) {
      latex_mode = _setting.substr(11);
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
          const ASTNode global =
              Parser(lex_text(cur_statement, fp)).parse();

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
              << "Total theorems: " << verily.im.known.size()
              << "\n"
              << "Mean theorems per second: "
              << (1'000'000.0 * verily.im.known.size() /
                  elapsed_us)
              << "\n";
  }

  if (print_latex) {
    std::ofstream f(fp.string() + ".tex");
    if (!f.is_open()) {
      std::cout << "Failed to open latex file\n";
      return 3;
    }
    verily.latex(f, latex_mode);
  }
  if (print_json) {
    std::ofstream f(fp.string() + ".json");
    if (!f.is_open()) {
      std::cout << "Failed to open json file\n";
      return 4;
    }
    verily.json(f);
  }

  if (!verily.im.pending.empty()) {
    std::cerr << "Pending (unproven) theorems:\n";
    for (const auto &p : verily.im.pending) {
      std::cerr << p << '\n';
    }
  }

  if (verily.saw_error) {
    return 5;
  }
  return 0;
}
