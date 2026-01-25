/**
 * @brief Tests the inference maker object
 */

#include "src/inference.hpp"
#include "src/parse.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

const std::filesystem::path null_fp = "NO_FP_GIVEN";

struct Settings {
  bool saw_error = false;
  bool debug = false;
  bool alternate = true;
  bool time = false;
  uintmax_t pass_limit = 64;
  std::filesystem::path fp = null_fp;
  std::set<size_t> proven_theorems;
};

void do_file(Settings &settings, InferenceMaker &im);

void process_statement(Settings &_settings, InferenceMaker &im,
                       const ASTNode &stmt) {
  if (_settings.debug) {
    std::cout << "On stmt " << stmt << "\n\n";
  }

  // Rule
  if (stmt.text == Token("RULE")) {
    // (RULE (OVER x y z) (GIVEN fee fi fo) (DEDUCE
    // fum fli foo flib))
    const auto over = stmt.children.at(0);
    const auto given = stmt.children.at(1);
    const auto consequence = stmt.children.at(2).children.front();

    std::set<ASTNode> free_variables;
    std::list<ASTNode> requirements;
    for (const auto &child : over.children) {
      free_variables.insert(child);
    }
    for (const auto &child : given.children) {
      requirements.push_back(child);
    }
    InferenceMaker::InferenceRule ir(free_variables, requirements, consequence);
    im.add_rule(ir);
  }

  // Thing to prove
  else if (stmt.text == Token("PROVE_FORWARD")) {
    // (THEOREM to_prove)
    const auto res =
        im.forward_prove(stmt.children.front(), _settings.pass_limit);
    if (res.has_value()) {
      _settings.proven_theorems.insert(res.value().index);
    } else {
      _settings.saw_error = true;
      std::cerr << "ERROR: Failed to prove " << stmt.children.front() << "\n\n";
    }
  }

  else if (stmt.text == Token("PROVE_SMT")) {
    throw std::runtime_error("'prove_smt' is unimplemented!");
  }

  else if (stmt.text == Token("PROVE_BACKWARD") ||
           stmt.text == Token("THEOREM")) {
    // (THEOREM to_prove)
    const auto res =
        im.backward_prove(stmt.children.front(), _settings.pass_limit);
    if (res.has_value()) {
      _settings.proven_theorems.insert(res.value().index);
    } else {
      _settings.saw_error = true;
      std::cerr << "ERROR: Failed to prove " << stmt.children.front() << "\n\n";
    }
  }

  // Axiom
  else if (stmt.text == Token("AXIOM")) {
    // (AXIOM a)
    im.add_axiom(stmt.children.front());
  }

  // Inclusion
  else if (stmt.text == Token("INCLUDE")) {
    // (INCLUDE path)
    const auto written = stmt.children.front().text.text;
    const auto path =
        std::filesystem::absolute(_settings.fp.parent_path() / written);
    do_file(_settings, im);
  }

  else {
    std::cout << "WARNING: Skipping statement " << stmt << "\n\n";
  }
}

void do_file(Settings &settings, InferenceMaker &im) {
  Parser p(lex_file(settings.fp));
  p.debug = settings.debug;
  const auto root = p.parse();

  if (settings.debug) {
    std::cout << "Root: " << root << "\n\n";
  }

  for (const auto &stmt : root.children) {
    process_statement(settings, im, stmt);
  }
}

void print_proof(const InferenceMaker &im, const size_t &_index) {
  const auto thm = im.get_theorem(_index);
  if (thm.rule_index < 0) {
    std::cout << "(axiom " << thm.thm << ")";
  } else {
    // (theorem (a) (rule_application (b) (premises c d e)))
    std::cout << "(theorem " << thm.thm << " (rule_application ";

    const auto rule = im.get_rule(thm.rule_index);

    // <fvs>(premises) -> consequence
    // (rule (over a b c) (given d e f) consequence)
    std::cout << "(rule (over";

    for (const auto &fv : rule.free_variables) {
      std::cout << ' ' << fv;
    }

    // End over
    std::cout << ") (given";

    for (const auto &req : rule.requirements) {
      std::cout << ' ' << req;
    }

    // End given, then end rule
    std::cout << ") " << rule.consequence << ")";

    std::cout << " (premises";
    for (const auto &premise : thm.premises) {
      std::cout << ' ';
      print_proof(im, premise);
    }
    // end premises, rule_application, theorem
    std::cout << ")))";
  }
}

int main(int argc, char *argv[]) {
  Settings settings;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--debug") {
      settings.debug = !settings.debug;
    } else if (arg == "--alternate") {
      settings.alternate = !settings.alternate;
    } else if (arg == "--pass_limit") {
      assert(i + 1 < argc);
      ++i;
      settings.pass_limit = std::stoi(argv[i]);
    } else if (arg == "--time") {
      settings.time = !settings.time;
    } else if (arg == "--help") {
      // clang-format off
      std::cout <<
        "+--------------------------------------------------+\n"
        "|                     Verily                       |\n"
        "+--------------------------------------------------+\n"
        "A deductive theorem prover. MIT License, 2025.      \n"
        "                                                    \n"
        " CLI flag       | Default | Meaning                 \n"
        "----------------|---------|-------------------------\n"
        " --help         |         | Prints this text        \n"
        " --debug        | false   | Toggles debug mode      \n"
        " --alternate    | true    | Toggles alternation     \n"
        " --pass_limit N | 64      | Sets the depth limit    \n"
      ;
      // clang-format on
      return 2;
    }

    else {
      settings.fp = arg;
    }
  }

  std::chrono::high_resolution_clock::time_point start, stop;
  InferenceMaker im;
  im.debug = settings.debug;
  im.enable_alternation = settings.alternate;

  if (settings.fp != null_fp) {
    // File mode
    if (settings.time) {
      start = std::chrono::high_resolution_clock::now();
    }
    do_file(settings, im);
    if (settings.time) {
      stop = std::chrono::high_resolution_clock::now();
    }
  }

  else {
    // CLI mode
    if (settings.time) {
      std::cerr << "WARNING: Cannot time in CLI mode\n";
      settings.time = false;
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
        if (settings.debug) {
          std::cout << "Processing CLI statement " << cur_statement << "\n";
        }

        const ASTNode global =
            Parser(lex_text(cur_statement, settings.fp)).parse();

        for (const auto &stmt : global.children) {
          if (stmt.text != "NULL") {
            process_statement(settings, im, stmt);
          }
        }

        // Clear
        cur_statement.clear();
      }
    }
    if (!cur_statement.empty()) {
      std::cerr << "WARNING: Discarding partial statement " << cur_statement;
    }
  }

  if (settings.debug) {
    std::cout << "All " << im.rules.size() << " rules:\n";
    for (uint i = 0; i < im.rules.size(); ++i) {
      std::cout << " " << i << " " << im.rules.at(i) << '\n';
    }
    std::cout << "\nAll " << im.known.size() << " theorems:\n";
    for (uint i = 0; i < im.known.size(); ++i) {
      std::cout << " " << i << " " << im.known.at(i) << '\n';
    }
  }

  for (const auto &index : settings.proven_theorems) {
    print_proof(im, index);
    std::cout << "\n\n";
  }

  if (settings.time) {
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
            .count();
    std::cout << "Took " << (elapsed_us / 1'000.0) << "ms\n"
              << "Total theorems: " << im.known.size() << "\n"
              << "Mean theorems per second: "
              << (1'000'000.0 * im.known.size() / elapsed_us) << "\n";
  }

  if (settings.saw_error) {
    return 1;
  }
  return 0;
}
