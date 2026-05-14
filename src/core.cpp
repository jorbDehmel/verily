#include "core.hpp"
#include "cdcl.hpp"
#include "inference.hpp"
#include <functional>
#include <stdexcept>

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
    } else if (t == "::") {
      print_ast_latex(_what.children.at(0));
      _strm << " \\texttt{::}";
      print_ast_latex(_what.children.at(1));
    } else if (t == "to") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\to ";
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
    } else if (t == "derives") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\vdash ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    } else if (t == "models") {
      _strm << "(";
      print_ast_latex(_what.children.at(0));
      _strm << " \\models ";
      print_ast_latex(_what.children.at(1));
      _strm << ")";
    }

    // Math
    else if (t == "^") {
      _strm << "{";
      print_ast_latex(_what.children.at(0));
      _strm << "}^{";
      print_ast_latex(_what.children.at(1));
      _strm << "}";
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
    } else if (t == "lambda") {
      _strm << "( \\lambda ";
      print_ast_latex(_what.children.at(0));
      _strm << " . ";
      print_ast_latex(_what.children.at(1));
      _strm << " )";
    } else if (t == "REPLACE") {
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
    } else if (t == "assumption") {
      const auto axiom = _what.children.at(0);
      _strm << "\\inferrule*[right=assumption]{\\,}{\n";
      print_ast_latex(axiom);
      _strm << "\n}";
    } else if (t == "meta") {
      _strm << "\\inferrule*[right=meta]{\\left[";

      bool first = true;
      for (uint index = 0; index + 1 < _what.children.size();
           ++index) {
        const ASTNode premise = _what.children.at(index);
        if (first) {
          first = false;
        } else {
          _strm << "\n";
        }
        print_ast_latex(premise);
      }

      _strm << "\\right]}{\n";
      const auto consequent = _what.children.back();
      print_ast_latex(consequent);
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
    } else if (t == "@") {
      // Fn call
      print_ast_latex(_what.children.front());
      _strm << "(";
      uint index = 0;
      for (const auto &child : _what.children) {
        switch (index) {
        default:
          _strm << ", ";
        case 1:
          print_ast_latex(child);
        case 0:
          break;
        }
        ++index;
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

  if (!im.rules.empty()) {
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
          _strm << "\n\\\\\n";
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
  }

  if (!axioms.empty()) {
    _strm << "\\textbf{Axioms:}\n\n";

    for (const auto &axiom : axioms) {
      const auto t = im.get_theorem(axiom);
      const auto proof = im.proof_to_ast(axiom);

      _strm << "\\texttt{"
            << sanitize_name(
                   t.name.value_or(std::to_string(axiom)))
            << "}:\n\n";

      _strm << "\\[\n";
      print_ast_latex(proof);
      _strm << "\n\\]\n\n";
    }
  }

  if (!proven_theorems.empty()) {
    _strm << "\\textbf{Selected Theorems:}\n\n";

    for (const auto &theorem : proven_theorems) {
      const auto t = im.get_theorem(theorem);
      const auto proof = im.proof_to_ast(theorem);

      _strm << "\\texttt{"
            << sanitize_name(
                   t.name.value_or(std::to_string(theorem)))
            << "}:\n\n";

      _strm << "\\[\n";
      print_ast_latex(proof);
      _strm << "\n\\]\n\n";
    }
  }

  _strm << "\\end{document}\n";
}

void Core::json(std::ostream &_strm) const {
  const std::function<void(const ASTNode &)> print_ast_json =
      [&](const ASTNode &_what) -> void {
    const auto t = _what.text;
    if (t == "_") {
      _strm << "[";
      bool first = true;
      for (const auto &child : _what.children) {
        if (first) {
          first = false;
        } else {
          _strm << ", ";
        }
        print_ast_json(child);
      }
      _strm << "]";
    } else if (_what.children.empty()) {
      _strm << "\"" << t.text << "\"";
    } else {
      _strm << "[\"" << t.text << "\"";
      for (const auto &child : _what.children) {
        _strm << ", ";
        print_ast_json(child);
      }
      _strm << "]";
    }
  };

  _strm << "{\n"
           "  \"rules\": [\n"
           "    ";
  size_t rule_index = 0;
  bool first_rule = true;
  for (const auto &rule : im.rules) {
    if (first_rule) {
      first_rule = false;
    } else {
      _strm << ",\n"
               "    ";
    }
    const std::string rule_name =
        rule.name.value_or(std::to_string(rule_index));
    ++rule_index;
    _strm << "{\n"
             "      \"name\": \""
          << rule_name
          << "\",\n"
             "      \"generics\": [\n"
             "        ";
    bool first = true;
    for (const auto &fv : rule.free_variables) {
      if (first) {
        first = false;
      } else {
        _strm << ",\n"
                 "        ";
      }
      print_ast_json(fv);
    }
    _strm << "\n"
             "      ],\n"
             "      \"premises\": [\n";
    first = true;
    for (const auto &premise : rule.requirements) {
      if (first) {
        first = false;
        _strm << "        ";
      } else {
        _strm << ",\n"
                 "        ";
      }
      print_ast_json(premise);
    }
    _strm << "\n"
             "      ],\n"
             "      \"consequence\": ";
    print_ast_json(rule.consequence);
    _strm << "\n"
             "    }";
  }

  _strm << "\n"
           "  ],\n"
           "  \"axioms\": [\n"
           "    ";
  bool first = true;
  for (const auto &axiom : axioms) {
    if (first) {
      first = false;
    } else {
      _strm << ",\n"
               "    ";
    }
    print_ast_json(im.proof_to_ast(axiom));
  }

  _strm << "\n"
           "  ],\n"
           "  \"selected-theorems\": [\n"
           "    ";
  first = true;
  for (const auto &theorem : proven_theorems) {
    if (first) {
      first = false;
    } else {
      _strm << ",\n"
               "    ";
    }
    print_ast_json(im.proof_to_ast(theorem));
  }
  _strm << "\n"
           "  ]\n"
           "}\n";
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

    ASTSet free_variables;
    std::vector<ASTNode> requirements;
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

  else if (_stmt.text == Token("NULL")) {
    return;
  }

  else if (_stmt.text == Token("SCOPE")) {
    for (const auto &stmt : _stmt.children) {
      process_statement(stmt, _cur_path);
    }
  }

  // Thing to prove
  else if (_stmt.text == Token("PROVE_FORWARD")) {
    // (THEOREM name to_prove)
    const auto name = _stmt.children.at(0).text.text;
    const auto thm = _stmt.children.at(1);
    const auto res = im.forward_prove(thm, pass_limit);
    if (res.has_value()) {
      proven_theorems.insert(res.value().index);
      if (!name.empty()) {
        im.name_theorem(thm, name);
      }
    } else {
      saw_error = true;
      if (!im.quiet) {
        std::cout << "ERROR:   Failed to prove " << thm << "\n";
      }
    }
  }

  else if (_stmt.text == Token("PROVE_SMT")) {
    // Takes in a single CNF formula, automatically deduces the
    // leaves, and runs it through a naive SMT solver. This is
    // a baby parser of its own.

    const auto name = _stmt.children.at(0).text.text;
    const auto to_prove = _stmt.children.at(1);

    // Deconstruct CNF statement and handle symbols
    CNF phi;
    std::vector<ASTNode> atoms;
    atoms.push_back(ASTNode("NULL"));

    std::function<int(const ASTNode &)> process_atom =
        [&](auto _t) {
          if (_t.text.text == "not") {
            return -process_atom(_t.children.front());
          }

          for (size_t i = 0; i < atoms.size(); ++i) {
            if (atoms[i] == _t) {
              return (int)i;
            }
          }

          atoms.push_back(_t);
          return (int)(atoms.size() - 1);
        };

    // 'or' clauses return nonempty sets, 'and' clauses return
    // empty sets, atoms return single-element sets (but that's
    // a special case of 'or', anyway).
    std::function<std::set<int>(const ASTNode &)> process_node =
        [&](auto _t) {
          if (_t.text.text == "and") {
            for (const auto &child : _t.children) {
              const auto res = process_node(child);
              if (!res.empty()) {
                // Child was an 'or' clause
                phi.clauses.push_back(res);
              }
            }
            return std::set<int>{};
          } else if (_t.text.text == "or") {
            std::set<int> out;
            for (const auto &child : _t.children) {
              if (child.text.text == "and") {
                // Special case: Recursion can't be trusted here
                out.insert(process_atom(child));
              } else {
                const auto res = process_node(child);
                for (const auto &element : res) {
                  out.insert(element);
                }
              }
            }
            return out;
          } else {
            return std::set<int>{process_atom(_t)};
          }
        };

    const auto res = process_node(to_prove);
    if (!res.empty()) {
      phi.clauses.push_back(res);
    }

    // Process, including basic theory checks
    CDCL c(phi);
    c.theory_check = [&](const std::set<int> &_solution)
        -> std::optional<std::set<int>> {
      // If the given solution is not provably true, return a
      // (naive) conflict.

      for (const auto &asserted_value : _solution) {
        bool clause_error = false;
        if (asserted_value > 0) {
          // Positive assertion: Make sure it is provable
          if (!im.prove(atoms.at(asserted_value), pass_limit)) {
            clause_error = true;
          }
        } else {
          // Negative assertion: Make sure it's negation is
          // provable
          if (!im.prove(
                  ASTNode("not", {atoms.at(-asserted_value)}),
                  pass_limit)) {
            clause_error = true;
          }
        }
        if (clause_error) {
          return std::set<int>{-asserted_value};
        }
      }

      return {};
    };

    const auto result = c.go();

    if (result.has_value()) {
      // The result has already passed the theory check, meaning
      // every asserted clause is provable. Therefore, the
      // result is tautologically true: Add it as a theorem.

      ASTNode smt_proof("SMT");
      for (const auto &thm : result.value()) {
        if (thm < 0) {
          smt_proof.children.push_back(im.theorem_to_proof(
              ASTNode("not", {atoms.at(-thm)})));
        } else {
          smt_proof.children.push_back(
              im.theorem_to_proof(atoms.at(thm)));
        }
      }

      const int out_ind = im.add_axiom(to_prove);
      im.special_proofs[out_ind] =
          ASTNode("meta", {smt_proof, to_prove});

      if (!name.empty()) {
        im.name_theorem(to_prove, name);
      }

      proven_theorems.insert(out_ind);
    } else {
      if (!im.quiet) {
        std::cout << "ERROR:   Failed to SMT prove "
                  << _stmt.children.front() << "\n";
      }
      saw_error = true;
    }
  }

  else if (_stmt.text == Token("PROVE_BACKWARD") ||
           _stmt.text == Token("THEOREM")) {
    // (THEOREM to_prove)
    const auto name = _stmt.children.at(0).text.text;
    const auto theorem = _stmt.children.at(1);

    const auto res = im.prove(theorem, pass_limit);
    if (!res.has_value()) {
      if (!im.quiet) {
        std::cout << "ERROR:   Failed to prove " << theorem
                  << "\n";
      }
      saw_error = true;
    } else {
      proven_theorems.insert(res.value().index);
      if (!name.empty()) {
        im.name_theorem(theorem, name);
      }
    }
  }

  else if (_stmt.text == "LS") {
    ls();
  }

  // Axiom
  else if (_stmt.text == Token("AXIOM")) {
    // (AXIOM name a)
    const auto name = _stmt.children.at(0).text.text;
    const auto thm = _stmt.children.at(1);
    const size_t index = im.add_axiom(thm);
    if (!name.empty()) {
      im.name_theorem(thm, name);
    }
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

  // Functions
  else if (_stmt.text == Token("FUNCTION")) {
    /*
    const auto out = ASTNode(Token("FUNCTION"),
                          {name, args, reqs_and_ens, body});
    */
    const ASTNode name = _stmt.children.at(0);
    const ASTNode args = _stmt.children.at(1);
    const ASTNode reqs_and_ens = _stmt.children.at(2);
    const ASTNode body = _stmt.children.at(3);

    // Write the definition as a rule

    ASTSet fvs;
    std::vector<ASTNode> reqs;
    std::vector<ASTNode> ens;
    std::vector<ASTNode> call_args = {name};

    for (const auto &arg : args.children) {
      // {argname, domain}
      const ASTNode argname = arg.children.at(0);
      const ASTNode domain = arg.children.at(1);
      fvs.insert(argname);
      call_args.push_back(argname);

      if (domain != "NULL") {
        reqs.push_back(ASTNode("in", {argname, domain}));
      }
    }
    for (const auto &req_or_ens : reqs_and_ens.children) {
      if (req_or_ens.text == "requires") {
        reqs.push_back(req_or_ens.children.at(0));
      } else {
        ens.push_back(req_or_ens.children.at(0));
      }
    }

    InferenceMaker::InferenceRule r(
        fvs, reqs,
        ASTNode("==", {ASTNode("@", call_args), body}));
    r.name = name.text.text;
    im.add_rule(r);

    // VC check
    if (!ens.empty() && !im.quiet) {
      std::cout
          << "Function keyword 'ensures' is unimplemented!\n";
    }
  }

  else if (_stmt.text == Token("SETTING")) {
    const std::string t = _stmt.children.front().text.text;

    if (t == "debug") {
      debug = !debug;
      im.debug = debug;
    } else if (t == "latex") {
      print_latex = !print_latex;
    } else if (t == "json") {
      print_json = !print_json;
    } else if (t == "alternate") {
      im.enable_alternation = !im.enable_alternation;
    } else if (t == "meta_prove") {
      im.meta_proving = !im.meta_proving;
    } else if (t == "time") {
      time = !time;
    } else if (t == "quiet") {
      im.quiet = !im.quiet;
    }

    else if (t == "debug=true") {
      debug = true;
      im.debug = debug;
    } else if (t == "debug=false") {
      debug = false;
      im.debug = debug;
    } else if (t == "latex=true") {
      print_latex = true;
    } else if (t == "latex=false") {
      print_latex = false;
    } else if (t == "json=true") {
      print_json = true;
    } else if (t == "json=false") {
      print_json = false;
    } else if (t == "alternate=true") {
      im.enable_alternation = true;
    } else if (t == "alternate=false") {
      im.enable_alternation = false;
    } else if (t == "meta_prove=true") {
      im.meta_proving = true;
    } else if (t == "meta_prove=false") {
      im.meta_proving = false;
    } else if (t == "time=true") {
      time = true;
    } else if (t == "time=false") {
      time = false;
    } else if (t == "time=quiet") {
      im.quiet = true;
    } else if (t == "time=quiet") {
      im.quiet = false;
    }

    else if (t.starts_with("pass_limit=")) {
      const size_t l = std::stoull(t.substr(11));
      pass_limit = l;
    } else if (t.starts_with("max_tree_height=")) {
      const size_t l = std::stoull(t.substr(16));
      im.max_tree_height = l;
    } else if (t.starts_with("max_theorems=")) {
      const size_t l = std::stoull(t.substr(13));
      im.theorem_limit = l;
    }

    else if (!im.quiet) {
      std::cout << "WARNING: Unknown setting " << t << "\n";
    }
  }

  else if (_stmt.text == "WTS") {
    im.pending.push_back(_stmt.children.at(0));
  }

  else if (_stmt.text == "APPLY") {
    // apply X to Y, Z, A;
    // apply all;
    // apply X;

    // (APPLY rule_name argument_list)
    const std::string rule_name =
        _stmt.children.at(0).text.text;
    const ASTNode thm_list = _stmt.children.at(1);
    const std::string result_name =
        _stmt.children.at(2).text.text;

    if (rule_name == "all") {
      if (!result_name.empty()) {
        throw std::runtime_error(
            "'apply all as ...' makes no sense");
      }
      const auto first_n = im.known.size();
      for (size_t rule_index = 0; rule_index < im.rules.size();
           ++rule_index) {
        im.inst_all(rule_index, first_n);
      }
      if (first_n == im.known.size()) {
        throw std::runtime_error("No rules could be applied");
      }
    } else {
      // Find and verify rule
      size_t rule_index = 0;
      const auto rule = im.get_rule(rule_name, rule_index);

      if (thm_list.children.empty()) {
        if (!result_name.empty()) {
          throw std::runtime_error(
              "'apply ... as ...' (without 'to' clause) makes "
              "no sense");
        }

        const auto before = im.known.size();
        im.inst_all(rule_index, before);

        if (before == im.known.size()) {
          throw std::runtime_error("Rule " + rule_name +
                                   " could not be applied");
        }
      } else {
        if (rule.requirements.size() !=
            thm_list.children.size()) {
          std::cerr << "Rule " << rule << " requires "
                    << rule.requirements.size()
                    << " arguments (";
          bool first = true;
          for (const auto &req : rule.requirements) {
            if (first) {
              first = false;
            } else {
              std::cerr << ' ';
            }
            std::cerr << req;
          }
          std::cerr << "), but " << thm_list.children.size()
                    << " were provided " << thm_list << '\n';
          throw std::runtime_error("Rule application mismatch");
        }

        // Find theorems
        std::vector<InferenceMaker::Theorem> thms;
        std::vector<size_t> premise_indices;
        for (const auto &t : thm_list.children) {
          const auto thm = im.get_theorem(t.text.text);
          thms.push_back(thm);
          premise_indices.push_back(thm.index);
        }

        // Try to apply
        const auto res = rule.apply(thms, im.known.size());

        bool trash = false;
        std::optional<std::string> n = {};
        if (!result_name.empty()) {
          n = result_name;
        }
        const auto thm_add_res =
            im.add_theorem(res, result_name, rule_index,
                           premise_indices, trash);
        proven_theorems.insert(thm_add_res.index);
      }
    }
  }

  else if (!im.quiet) {
    std::cout << "WARNING: Skipping unknown statement " << _stmt
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

void Core::ls() const noexcept {
  std::cout << "All " << im.rules.size() << " rules:\n";
  for (uint i = 0; i < im.rules.size(); ++i) {
    const auto r = im.rules.at(i);
    std::cout << " " << r.name.value_or(std::to_string(i))
              << " " << r << '\n';
  }
  std::cout << "\nAll " << im.known.size() << " theorems:\n";
  for (uint i = 0; i < im.known.size(); ++i) {
    const auto t = im.known.at(i);
    std::cout << " " << t.name.value_or(std::to_string(i))
              << " " << t << '\n';
  }
  std::cout << "\nAll " << im.pending.size() << " pending:\n";
  for (const auto &p : im.pending) {
    std::cout << " " << p << '\n';
  }
}
