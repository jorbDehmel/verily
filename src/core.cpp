#include "core.hpp"
#include "inference.hpp"
#include "parse.hpp"
#include <cassert>
#include <functional>
#include <string>
#include <sys/types.h>

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
  for (const auto &p : im.frames.front().rules) {
    const auto rule_name = p.first;
    const auto rule = p.second;
    if (first_rule) {
      first_rule = false;
    } else {
      _strm << ",\n"
               "    ";
    }
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
           "  \"selected-theorems\": [\n"
           "    ";
  bool first = true;
  for (const auto &p : im.frames.front().theorems) {
    if (first) {
      first = false;
    } else {
      _strm << ",\n"
               "    ";
    }
    print_ast_json(im.proof_to_ast(p.first));
  }
  _strm << "\n"
           "  ]\n"
           "}\n";
}

void Core::process_statement(
    const ASTNode &_stmt,
    const std::filesystem::path &_cur_path) {
  if (debug) {
    std::cout << "On stmt " << _stmt << "\n";
  }

  if (_stmt.text == Token("AXIOM")) {
    // (AXIOM (OVER x y z) (GIVEN fee fi fo) (DEDUCE
    // fum fli foo flib))
    const auto over = _stmt.children.at(0);
    const auto given = _stmt.children.at(1);
    const auto consequence =
        _stmt.children.at(2).children.front();
    const std::string name = _stmt.children.at(3).text.text;

    std::set<ASTNode> free_variables;
    std::vector<ASTNode> requirements;
    for (const auto &child : over.children) {
      free_variables.insert(child);
    }
    for (const auto &child : given.children) {
      requirements.push_back(child);
    }
    InferenceRule ir(free_variables, requirements, consequence,
                     true);
    im.add_rule(name, ir);
  }

  else if (_stmt.text == "THEOREM") {
    const auto over = _stmt.at(0);
    const auto given = _stmt.at(1);
    const auto deduce = _stmt.at(2);
    const auto proof = _stmt.at(3);
    const auto name = _stmt.at(4).text.text;

    im.push();
    uint i = 0;
    for (const auto &assumption : given.children) {
      im.add_assumption("h" + std::to_string(i), assumption);
      ++i;
    }
    im.check_proof(deduce.at(0), proof, name);
    im.pop();

    std::set<ASTNode> fvs;
    for (const auto &v : over.children) {
      fvs.insert(v);
    }
    const auto reqs = given.children;
    const auto cons = deduce.children.front();
    InferenceRule rule(fvs, reqs, cons, false);
    im.add_rule((name == "NULL"
                     ? "t" + std::to_string(
                                 im.frames.back().rules.size())
                     : name),
                rule);
  }

  else if (_stmt.text == Token("NULL")) {
    return;
  }

  else if (_stmt.text == Token("SCOPE")) {
    im.push();
    for (const auto &stmt : _stmt.children) {
      process_statement(stmt, _cur_path);
    }
    im.pop();
  }

  else if (_stmt.text == "LS") {
    ls();
  }

  // Inclusion
  else if (_stmt.text == Token("INCLUDE")) {
    // (INCLUDE path)
    const auto written = _stmt.children.front().text.text;
    const auto path = std::filesystem::absolute(
        _cur_path.parent_path() / written);
    do_file(path);
  }

  else if (_stmt.text == Token("SETTING")) {
    const std::string t = _stmt.children.front().text.text;
    handle_setting(*this, t);
  }

  else if (_stmt.text == "NOTATION") {
    const auto name = _stmt.at(0).text.text;
    const auto fvs = _stmt.at(1);
    const auto input_pattern = _stmt.at(2);
    const auto raw_output_pattern = _stmt.at(3).at(0);
    const auto before = _stmt.at(4);

    assert(raw_output_pattern.text == "_");
    assert(raw_output_pattern.at(0).empty());
    ASTNode output_pattern(raw_output_pattern.at(0).text);
    for (size_t i = 1; i < raw_output_pattern.size(); ++i) {
      output_pattern.children.push_back(
          raw_output_pattern.at(i));
    }

    std::set<ASTNode> real_fvs;
    for (const auto &fv : fvs.children) {
      real_fvs.insert(fv);
    }
    InferenceRule info(real_fvs, input_pattern.children,
                       output_pattern);

    // Find where to put it
    auto pos = parser.order_of_operations.end();

    if (!before.empty()) {
      const auto b = before.at(0).text.text;
      pos = parser.order_of_operations.begin();
      while (pos != parser.order_of_operations.end() &&
             pos->first != b) {
        ++pos;
      }
    }

    // Insert it
    parser.order_of_operations.insert(
        pos, std::make_pair(name, info));
  }

  else {
    std::cerr << "WARNING: Skipping unknown statement " << _stmt
              << "\n";
  }
}

void Core::do_file(const std::filesystem::path &_fp) {
  if (visited_files.contains(_fp)) {
    return;
  }
  visited_files.insert(_fp);
  const auto ts = lex_file(_fp);
  if (debug) {
    std::cout << "Prepending file ts:";
    for (const auto &tok : ts.data) {
      std::cout << ' ' << tok.text;
    }
    std::cout << '\n';
  }
  parser.ts.prepend(ts.data);
  while (!parser.ts.done()) {
    parse_and_process(_fp);
  }
}

void Core::ls() const noexcept {
  size_t n_axioms = 0;
  std::cout << "All " << im.frames.back().rules.size()
            << " rules:\n";
  for (const auto &p : im.frames.back().rules) {
    if (p.second.axiom) {
      n_axioms += 1;
    }
    std::cout << "  " << p.first << " " << p.second;
    if (!p.second.axiom) {
      std::cout << " [theorem]";
    }
    std::cout << '\n';
  }
  std::cout << "\nAll " << im.frames.back().theorems.size()
            << " theorems:\n";
  for (const auto &p : im.frames.back().theorems) {
    std::cout << " " << p.first << " " << p.second << '\n';
  }
  std::cout << "Total axiom schemas: " << n_axioms << '\n';
}
