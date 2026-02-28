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
#include <string>

const std::string version = "0.0.1";

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
    } else if (arg == "--time") {
      verily.time = !verily.time;
    } else if (arg == "--latex") {
      verily.print_latex = !verily.print_latex;
    } else if (arg == "--help") {
      // clang-format off
      std::cout <<
        "+--------------------------------------------------+\n"
        "|                     Verily                       |\n"
        "+--------------------------------------------------+\n"
        "A deductive theorem prover. MIT License, 2025-2026. \n"
        "                                                    \n"
        " CLI flag       | Default | Meaning                 \n"
        "----------------|---------|-------------------------\n"
        " --help         |         | Prints this text        \n"
        " --debug        | false   | Toggles debug mode      \n"
        " --alternate    | false   | Toggles alternation     \n"
        " --pass_limit N | 64      | Sets the depth limit    \n"
        " --latex        | false   | Prints latex to file    \n"
        "                                                    \n"
        "You can give it a filepath as an argument, in which \n"
        "case that file will be analyzed. If no filepath is  \n"
        "provided, it will read from stdin in a REPL         \n"
        "interface.                                          \n"
        "                                                    \n"
        "Version " << version << "\n"
      ;
      // clang-format on
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
            verily.process_statement(stmt, fp);
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
    std::cout << "All " << verily.im.rules.size()
              << " rules:\n";
    for (uint i = 0; i < verily.im.rules.size(); ++i) {
      std::cout << " " << i << " " << verily.im.rules.at(i)
                << '\n';
    }
    std::cout << "\nAll " << verily.im.known.size()
              << " theorems:\n";
    for (uint i = 0; i < verily.im.known.size(); ++i) {
      std::cout << " " << i << " " << verily.im.known.at(i)
                << '\n';
    }
  }

  for (const auto &index : verily.proven_theorems) {
    std::cout << verily.proof_to_ast(index) << "\n\n";
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
      return 2;
    }
    verily.latex(f);
  }

  if (verily.saw_error) {
    return 1;
  }
  return 0;
}
