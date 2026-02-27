#include "core.hpp"
#include <functional>

std::string Core::sanitize_name(const std::string &_s) {
  std::string out;
  for (const auto &c : _s) {
    if (c == '_') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

ASTNode Core::proof_to_ast(const size_t &_thm_index) const {
  const auto thm = im.get_theorem(_thm_index);
  if (thm.rule_index < 0) {
    return ASTNode("axiom", {thm.thm});
  } else {
    ASTNode premises_block("premises");
    for (const auto &premise : thm.premises) {
      premises_block.children.push_back(proof_to_ast(premise));
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
void Core::latex(std::ostream &_strm) const {
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

    // Quantification
    else if (t == "forall") {
      _strm << "( \\forall ";
      print_ast_latex(_what.children.at(0));
      _strm << " . ";
      print_ast_latex(_what.children.at(1));
      _strm << " )";
    } else if (t == "exists") {
      _strm << "( \\exists ";
      print_ast_latex(_what.children.at(0));
      _strm << " . ";
      print_ast_latex(_what.children.at(1));
      _strm << " )";
    } else if (t == "REPLACE") {
      /*
      const auto A = children.at(0);
      const auto x = children.at(1);
      const auto B = children.at(2);
      return A.replace(x, B).beta_star();
      */
      print_ast_latex(_what.children.at(0));
      _strm << " [ ";
      print_ast_latex(_what.children.at(1));
      _strm << " := ";
      print_ast_latex(_what.children.at(2));
      _strm << " ]";
    }

    // Proof stuff
    else if (t == "axiom") {
      const auto axiom = _what.children.at(0);
      _strm << "\\inferrule*[right=axiom]{\\,}{\n";
      print_ast_latex(axiom);
      _strm << "\n}";
    } else if (t == "theorem") {
      const auto thm = _what.children.at(0);
      const auto rule_app = _what.children.at(1);
      const std::string rule_name =
          rule_app.children.at(0).children.at(0).text.text;
      const auto premises = rule_app.children.at(1);

      _strm << "\\inferrule*[right=" << sanitize_name(rule_name)
            << "]{";
      bool first = true;
      for (const auto &premise : premises.children) {
        if (first) {
          first = false;
        } else {
          _strm << "\n";
        }
        print_ast_latex(premise);
      }
      if (first) {
        _strm << "\\,";
      }
      _strm << "}{\n";
      print_ast_latex(thm);
      _strm << "\n}";
    }

    // Default case: Just print the s-expr itself.
    else if (t == "_") {
      // List
      _strm << "(";
      bool first = true;
      for (const auto &child : _what.children) {
        if (first) {
          first = false;
        } else {
          _strm << ", ";
        }
        print_ast_latex(child);
      }
      _strm << ")";
    } else if (_what.children.empty()) {
      _strm << "\\texttt{" << sanitize_name(t.text) << "}";
    } else {
      _strm << "\\texttt{" << sanitize_name(t.text) << "}(";
      bool first = true;
      for (const auto &child : _what.children) {
        if (first) {
          first = false;
        } else {
          _strm << ", ";
        }
        print_ast_latex(child);
      }
      _strm << ")";
    }
  };

  _strm << "\\documentclass{article}\n"
           "\\usepackage{amsmath}\n"
           "\\usepackage{amssymb}\n"
           "\\usepackage{mathpartir}\n"
           "\\begin{document}\n\n";

  _strm << "\\textbf{Rules:}\n\n";

  size_t rule_index = 0;
  for (const auto &rule : im.rules) {
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
             "\\inferrule*[right="
          << sanitize_name(rule_name) << "]{";

    // Premises
    bool first = true;
    for (const auto &premise : rule.requirements) {
      if (first) {
        first = false;
      } else {
        _strm << "\n";
      }
      print_ast_latex(premise);
    }
    if (first) {
      _strm << "\\,";
    }

    _strm << "}{\n";

    // Consequence
    print_ast_latex(rule.consequence);

    _strm << "  }\n"
             "\\]\n\n";
  }

  _strm << "\\textbf{Axioms:}\n\n";

  for (const auto &axiom : axioms) {
    _strm << "\\[\n";
    print_ast_latex(proof_to_ast(axiom));
    _strm << "\n\\]\n\n";
  }

  _strm << "\\textbf{Selected Theorems:}\n\n";

  for (const auto &theorem : proven_theorems) {
    _strm << "\\[\n";
    print_ast_latex(proof_to_ast(theorem));
    _strm << "\n\\]\n\n";
  }

  _strm << "\\end{document}\n";
}

void Core::process_statement(
    const ASTNode &_stmt,
    const std::filesystem::path &_cur_path) {
  if (debug) {
    std::cout << "On stmt " << _stmt << "\n\n";
  }

  // Rule
  if (_stmt.text == Token("RULE")) {
    // (RULE (OVER x y z) (GIVEN fee fi fo) (DEDUCE
    // fum fli foo flib))
    const auto over = _stmt.children.at(0);
    const auto given = _stmt.children.at(1);
    const auto consequence =
        _stmt.children.at(2).children.front();
    const std::string name = _stmt.children.at(3).text.text;

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
  else if (_stmt.text == Token("PROVE_FORWARD")) {
    // (THEOREM to_prove)
    const auto res =
        im.forward_prove(_stmt.children.front(), pass_limit);
    if (res.has_value()) {
      proven_theorems.insert(res.value().index);
    } else {
      saw_error = true;
      std::cerr << "ERROR:   Failed to prove "
                << _stmt.children.front() << "\n";
    }
  }

  else if (_stmt.text == Token("PROVE_SMT")) {
    throw std::runtime_error("'prove_smt' is unimplemented!");
  }

  else if (_stmt.text == Token("PROVE_BACKWARD") ||
           _stmt.text == Token("THEOREM")) {
    // (THEOREM to_prove)
    const auto res =
        im.backward_prove(_stmt.children.front(), pass_limit);
    if (res.has_value()) {
      proven_theorems.insert(res.value().index);
    } else {
      saw_error = true;
      std::cerr << "ERROR:   Failed to prove "
                << _stmt.children.front() << "\n";
    }
  }

  // Axiom
  else if (_stmt.text == Token("AXIOM")) {
    // (AXIOM a)
    const size_t index = im.add_axiom(_stmt.children.front());
    axioms.insert(index);
  }

  // Inclusion
  else if (_stmt.text == Token("INCLUDE")) {
    // (INCLUDE path)
    const auto written = _stmt.children.front().text.text;
    const auto path = std::filesystem::absolute(
        _cur_path.parent_path() / written);
    do_file(path);
  }

  else {
    std::cout << "WARNING: Skipping statement " << _stmt
              << "\n";
  }
}

void Core::do_file(const std::filesystem::path &_fp) {
  Parser p(lex_file(_fp));
  p.debug = debug;
  const auto root = p.parse();

  if (debug) {
    std::cout << "Root: " << root << "\n\n";
  }

  for (const auto &stmt : root.children) {
    process_statement(stmt, _fp);
  }
}
