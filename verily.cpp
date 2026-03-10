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

const std::string version = "0.0.5";

int main(int argc, char *argv[]) {
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
      verily.time = !verily.time;
    } else if (arg == "--latex") {
      verily.print_latex = !verily.print_latex;
    } else if (arg == "--json") {
      verily.print_json = !verily.print_json;
    } else if (arg == "--metaprove") {
      verily.im.meta_proving = !verily.im.meta_proving;
    } else if (arg == "--help") {
      // clang-format off
      std::cout <<
        "+--------------------------------------------------+\n"
        "|                     Verily                       |\n"
        "+--------------------------------------------------+\n"
        "A deductive theorem prover. MIT License, 2025-2026. \n"
        "                                                    \n"
        " CLI flag          | Default | Meaning                 \n"
        "-------------------|---------|-------------------------\n"
        " --alternate       | false   | Toggles alternation     \n"
        " --debug           | false   | Toggles debug mode      \n"
        " --help            |         | Prints this text        \n"
        " --json            | false   | Prints json to file     \n"
        " --latex           | false   | Prints latex to file    \n"
        " --max_theorems    | 10,000  | Sets the max # theorems \n"
        " --max_tree_height | 100     | Set the max AST height  \n"
        " --metaprove       | true    | Toggles meta proving    \n"
        " --pass_limit N    | 64      | Sets the depth limit    \n"
        "                                                    \n"
        "You can give it a filepath as an argument, in which \n"
        "case that file will be analyzed. If no filepath is  \n"
        "provided, it will read from stdin in a REPL         \n"
        "interface.                                          \n"
        "                                                    \n"
        "Version " << version << "\n"
      ;
      // clang-format on
      return 1;
    }

    else if (arg.starts_with("--")) {
      std::cerr << "Unknown flag '" << arg
                << "'. Use '--help' to get help.\n";
      return 2;
    }

    else {
      fp = arg;
    }
  }

  std::chrono::high_resolution_clock::time_point start, stop;
  if (fp != null_fp) {
    // File mode
    if (verily.time) {
      start = std::chrono::high_resolution_clock::now();
    }
    verily.do_file(fp);
    if (verily.time) {
      stop = std::chrono::high_resolution_clock::now();
    }
  }

  else {
    // CLI mode
    if (verily.time) {
      std::cerr << "WARNING: Cannot time in CLI mode\n";
      verily.time = false;
    }

    std::cout << "Verily CLI mode: CTL+D / EOF to exit.\n";

    std::string cur_statement;
    while (!std::cin.eof()) {
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

        const ASTNode global =
            Parser(lex_text(cur_statement, fp)).parse();

        for (const auto &stmt : global.children) {
          if (stmt.text != "NULL") {
            try {
              verily.process_statement(stmt, fp);
            } catch (std::runtime_error &_e) {
              std::cerr << "Caught error: " << _e.what()
                        << "\n";
            } catch (...) {
              std::cerr << "Unknown error!\n";
            }
          }
        }

        // Clear
        cur_statement.clear();
      }
    }
    if (!cur_statement.empty()) {
      std::cerr << "WARNING: Discarding partial statement "
                << cur_statement << "\n";
    }
  }

  if (verily.debug) {
    verily.ls();
  }

  for (const auto &index : verily.proven_theorems) {
    std::cout << verily.im.proof_to_ast(index) << "\n\n";
  }

  if (verily.time) {
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

  if (verily.print_latex) {
    std::ofstream f(fp.string() + ".tex");
    if (!f.is_open()) {
      std::cerr << "Failed to open latex file\n";
      return 3;
    }
    verily.latex(f);
  }
  if (verily.print_json) {
    std::ofstream f(fp.string() + ".json");
    if (!f.is_open()) {
      std::cerr << "Failed to open json file\n";
      return 4;
    }
    verily.json(f);
  }

  if (verily.saw_error) {
    return 5;
  }
  return 0;
}
