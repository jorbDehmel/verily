/**
 * @brief Tests the inference maker object
 */

#include "src/inference.hpp"
#include "src/parse.hpp"
#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

const std::filesystem::path null_fp = "NO_FP_GIVEN";

struct Settings {
  bool saw_error = false;
  bool debug = false;
  bool alternate = false;
  bool time = false;
  bool print_latex = false;
  uintmax_t pass_limit = 64;
  std::filesystem::path fp = null_fp;
  std::set<size_t> axioms;
  std::set<size_t> proven_theorems;
};

/// Turns all underscores to spaces
std::string sanitize_name(const std::string &_s) {
  std::string out;
  for (const auto &c : _s) {
    if (c == '_') {
      out.push_back(' ');
    } else {
      out.push_back(c);
    }
  }
  return out;
}

ASTNode proof_to_ast(const InferenceMaker &im,
                     const size_t &_thm_index) {
  const auto thm = im.get_theorem(_thm_index);
  if (thm.rule_index < 0) {
    return ASTNode("axiom", {thm.thm});
  } else {
    ASTNode premises_block("premises");
    for (const auto &premise : thm.premises) {
      premises_block.children.push_back(
          proof_to_ast(im, premise));
    }

    const auto rule = im.get_rule(thm.rule_index);
    const std::string rule_name =
        rule.name.value_or(std::to_string(thm.rule_index));

    return ASTNode(
        "theorem",
        {thm.thm,
         ASTNode("rule_application",
                 {ASTNode("rule", {ASTNode(rule_name)}),
                  premises_block})});
  }
}

/// Prints the rules, axioms, and selected theorems in latex
/// 'inferrule' notation
void print_latex(const InferenceMaker &_im,
                 const std::set<size_t> &_axioms,
                 const std::set<size_t> &_thms_to_print,
                 std::ostream &_strm) {
  // Print an AST in latex notation (EG 'and' -> '\land')
  const std::function<void(const ASTNode &)> print_ast_latex =
      [&](const ASTNode &_what) -> void {
    const auto t = _what.text;

    // Normal PL stuff
    if (t == "and") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\land ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "or") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\lor ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "not") {
      _strm << " \\lnot ";
      print_ast_latex(_what.children.at(0));
    } else if (t == "implies") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\implies ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "iff") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\iff ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "in") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\in ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "==") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " = ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "prime") {
      print_ast_latex(_what.children.at(0));
      _strm << "' ";
    }

    // Proof stuff
    else if (t == "axiom") {
      const auto axiom = _what.children.at(0);
      _strm << "\\inferrule*{\\,}{\n";
      print_ast_latex(axiom);
      _strm << "\n}";
    } else if (t == "theorem") {
      const auto thm = _what.children.at(0);
      const auto rule_app = _what.children.at(1);
      const std::string rule_name =
          rule_app.children.at(0).children.at(0).text.text;
      const auto premises = rule_app.children.at(1);

      _strm << "\\inferrule*[right=" << sanitize_name(rule_name)
            << "]{\n";
      bool first = true;
      for (const auto &premise : premises.children) {
        if (first) {
          first = false;
        } else {
          _strm << "\n    \\\\\n    ";
        }
        print_ast_latex(premise);
      }
      _strm << "\n}{\n";
      print_ast_latex(thm);
      _strm << "\n}";
    }

    // Default case: Just print the s-expr itself.
    else {
      _strm << "\\texttt{" << _what << "}";
    }
  };

  _strm << "\\textbf{Rules:}\n\n";

  size_t rule_index = 0;
  for (const auto &rule : _im.rules) {
    if (!rule.free_variables.empty()) {
      _strm << "For generic";
      bool first = true;
      for (const auto &fv : rule.free_variables) {
        if (first) {
          first = false;
        } else {
          _strm << ",";
        }
        _strm << " \\texttt{" << fv << "}";
      }
      _strm << ":\n\n";
    }

    const std::string rule_name =
        rule.name.value_or(std::to_string(rule_index));
    ++rule_index;
    _strm << "\\[\n"
             "  \\inferrule*[right="
          << sanitize_name(rule_name) << "]{\n    ";

    // Premises
    bool first = true;
    for (const auto &premise : rule.requirements) {
      if (first) {
        first = false;
      } else {
        _strm << "\n    \\\\\n    ";
      }
      print_ast_latex(premise);
    }

    _strm << "  }{\n    ";

    // Consequence
    print_ast_latex(rule.consequence);

    _strm << "  }\n"
             "\\]\n\n";
  }

  _strm << "\\textbf{Axioms:}\n\n";

  for (const auto &axiom : _axioms) {
    _strm << "\\[\n";
    print_ast_latex(proof_to_ast(_im, axiom));
    _strm << "\n\\]\n\n";
  }

  _strm << "\\textbf{Selected Theorems:}\n\n";

  for (const auto &theorem : _thms_to_print) {
    _strm << "\\[\n";
    print_ast_latex(proof_to_ast(_im, theorem));
    _strm << "\n\\]\n\n";
  }
}

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
    const auto consequence =
        stmt.children.at(2).children.front();
    const std::string name = stmt.children.at(3).text.text;

    std::set<ASTNode> free_variables;
    std::list<ASTNode> requirements;
    for (const auto &child : over.children) {
      free_variables.insert(child);
    }
    for (const auto &child : given.children) {
      requirements.push_back(child);
    }
    InferenceMaker::InferenceRule ir(free_variables,
                                     requirements, consequence);

    if (name != "NULL") {
      ir.name = name;
    }

    im.add_rule(ir);
  }

  // Thing to prove
  else if (stmt.text == Token("PROVE_FORWARD")) {
    // (THEOREM to_prove)
    const auto res = im.forward_prove(stmt.children.front(),
                                      _settings.pass_limit);
    if (res.has_value()) {
      _settings.proven_theorems.insert(res.value().index);
    } else {
      _settings.saw_error = true;
      std::cerr << "ERROR:   Failed to prove "
                << stmt.children.front() << "\n\n";
    }
  }

  else if (stmt.text == Token("PROVE_SMT")) {
    throw std::runtime_error("'prove_smt' is unimplemented!");
  }

  else if (stmt.text == Token("PROVE_BACKWARD") ||
           stmt.text == Token("THEOREM")) {
    // (THEOREM to_prove)
    const auto res = im.backward_prove(stmt.children.front(),
                                       _settings.pass_limit);
    if (res.has_value()) {
      _settings.proven_theorems.insert(res.value().index);
    } else {
      _settings.saw_error = true;
      std::cerr << "ERROR:   Failed to prove "
                << stmt.children.front() << "\n\n";
    }
  }

  // Axiom
  else if (stmt.text == Token("AXIOM")) {
    // (AXIOM a)
    const size_t index = im.add_axiom(stmt.children.front());
    _settings.axioms.insert(index);
  }

  // Inclusion
  else if (stmt.text == Token("INCLUDE")) {
    // (INCLUDE path)
    const auto written = stmt.children.front().text.text;
    const auto path = std::filesystem::absolute(
        _settings.fp.parent_path() / written);
    do_file(_settings, im);
  }

  else {
    std::cout << "WARNING: Skipping statement " << stmt << "\n";
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
    } else if (arg == "--latex") {
      settings.print_latex = !settings.print_latex;
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
        " --alternate    | false   | Toggles alternation     \n"
        " --pass_limit N | 64      | Sets the depth limit    \n"
        " --latex        | false   | Prints latex at end     \n"
        "                                                    \n"
        "You can give it a filepath as an argument, in which \n"
        "case that file will be analyzed. If no filepath is  \n"
        "provided, it will read from stdin in a REPL         \n"
        "interface.                                          \n"
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
          std::cout << "Processing CLI statement "
                    << cur_statement << "\n";
        }

        const ASTNode global =
            Parser(lex_text(cur_statement, settings.fp))
                .parse();

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
      std::cerr << "WARNING: Discarding partial statement "
                << cur_statement << "\n";
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
    std::cout << proof_to_ast(im, index) << "\n\n";
  }

  if (settings.time) {
    const auto elapsed_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            stop - start)
            .count();
    std::cout << "Took " << (elapsed_us / 1'000.0) << "ms\n"
              << "Total theorems: " << im.known.size() << "\n"
              << "Mean theorems per second: "
              << (1'000'000.0 * im.known.size() / elapsed_us)
              << "\n";
  }

  if (settings.print_latex) {
    print_latex(im, settings.axioms, settings.proven_theorems,
                std::cout);
  }

  if (settings.saw_error) {
    return 1;
  }
  return 0;
}
