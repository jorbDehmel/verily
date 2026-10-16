/**
 * @brief Inference rule system implementation
 */

#include "inference.hpp"
#include <cassert>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

ASTNode InferenceMaker::proof_to_ast(
    const std::string &_thm_name) const {
  const auto thm = get_theorem(_thm_name);
  ASTNode premises_block("premises");
  for (const auto &premise : thm.premises) {
    premises_block.children.push_back(
        proof_to_ast(get_name(premise)));
  }
  const auto rule = get_rule(thm.rule_name);
  return ASTNode(
      "theorem",
      {thm.thm,
       ASTNode("rule_application",
               {ASTNode("rule", {ASTNode(thm.rule_name)}),
                premises_block})});
}

const InferenceRule
InferenceMaker::get_rule(const std::string &_name) const {
  for (auto rit = frames.rbegin(); rit != frames.rend();
       ++rit) {
    if (rit->rules.contains(_name)) {
      return rit->rules.at(_name);
    }
  }
  throw std::runtime_error("Unknown rule '" + _name + "'");
}

const InferenceMaker::Theorem
InferenceMaker::get_theorem(const std::string &_name) const {
  for (auto rit = frames.rbegin(); rit != frames.rend();
       ++rit) {
    if (rit->theorems.contains(_name)) {
      return rit->theorems.at(_name);
    }
  }
  throw std::runtime_error("Unknown theorem '" + _name + "'");
}

const InferenceMaker::Theorem
InferenceMaker::get_theorem(const ASTNode &_thm) const {
  for (auto rit = frames.rbegin(); rit != frames.rend();
       ++rit) {
    for (const auto &p : rit->theorems) {
      if (p.second.thm == _thm) {
        return p.second;
      }
    }
  }
  std::cerr << "Offender: " << _thm << '\n';
  throw std::runtime_error("Failed to find theorem");
}

bool InferenceMaker::has(const ASTNode &_what) const noexcept {
  for (auto rit = frames.rbegin(); rit != frames.rend();
       ++rit) {
    for (const auto &p : rit->theorems) {
      if (p.second.thm == _what) {
        return true;
      }
    }
  }
  return false;
}

std::string
InferenceMaker::get_name(const ASTNode &_what) const {
  for (auto rit = frames.rbegin(); rit != frames.rend();
       ++rit) {
    for (const auto &p : rit->theorems) {
      if (p.second.thm == _what) {
        return p.first;
      }
    }
  }
  std::cerr << "Offender: " << _what << '\n';
  throw std::runtime_error("Failed to find theorem name");
}

void InferenceMaker::add_assumption(
    const std::string &_name, const ASTNode &_what) noexcept {
  const std::string name =
      (_name == "NULL"
           ? "h" + std::to_string(frames.back().theorems.size())
           : _name);
  frames.back().theorems[name] =
      Theorem(_what, "ASSUMPTION", {});
  if (debug) {
    std::cout << "Added assumption: " << _what << "\n\n";
  }
}

void InferenceMaker::add_rule(const std::string &_name,
                              const InferenceRule &_rule) {
  const std::string name =
      (_name == "NULL"
           ? "r" + std::to_string(frames.back().rules.size())
           : _name);
  frames.back().rules[name] = _rule;
  if (debug) {
    std::cout << "Added rule: " << _rule << "\n\n";
  }
}

const InferenceMaker::Theorem
InferenceMaker::add_theorem(const std::string &_name,
                            const Theorem &_thm) {
  const std::string name =
      (_name == "NULL"
           ? "t" + std::to_string(frames.back().theorems.size())
           : _name);
  frames.back().theorems[_name] = _thm;
  close_goal(_thm.thm);
  if (debug) {
    std::cout << "Derived theorem " << _thm.thm << "\n\n";
  }
  return _thm;
}

std::ostream &operator<<(std::ostream &_strm,
                         const InferenceRule &_rule) {
  _strm << "<";
  bool first = true;
  for (const auto &fv : _rule.free_variables) {
    if (first) {
      first = false;
    } else {
      _strm << ", ";
    }
    _strm << fv;
  }
  _strm << ">(";

  first = true;
  for (const auto &p : _rule.requirements) {
    if (first) {
      first = false;
    } else {
      _strm << ", ";
    }
    _strm << p;
  }
  _strm << ") -> " << _rule.consequence;
  return _strm;
}

std::ostream &operator<<(std::ostream &_strm,
                         const InferenceMaker::Theorem &_thm) {
  _strm << "thm: " << _thm.thm << " due to rule "
        << _thm.rule_name << " on premises (";
  bool first = true;
  for (const auto &premise : _thm.premises) {
    if (first) {
      first = false;
    } else {
      _strm << ", ";
    }
    _strm << premise;
  }
  _strm << ")";
  return _strm;
}

// Note: _name is either "NULL" or an actual name.
void InferenceMaker::check_proof(const ASTNode &_goal,
                                 const ASTNode &_proof,
                                 const std::string &_name) {
  if (debug) {
    std::cout << "Checking if " << _proof << " proves " << _goal
              << '\n';
  }

  push();
  open_goal(_goal);
  if (_proof.text != "PROOF") {
    throw std::runtime_error("Cannot check a non-proof!");
  }

  // Check proof
  for (const auto &pf : _proof.children) {
    if (frames.back().goals.empty() && pf.text.text != "LS") {
      std::cerr << "WARNING: No goals remain! Ignoring " << pf
                << "\n";
      continue;
    }

    const auto p = pf.text.text;
    if (p == "FORWARD") {
      const auto rule_name = pf.at(0).text.text;
      const auto on = pf.at(1);
      const auto as = pf.at(2);
      const auto rule = get_rule(rule_name);
      std::vector<ASTNode> antecedent_thms;
      for (const auto &arg : on.children) {
        const auto thm = get_theorem(arg.text.text);
        antecedent_thms.push_back(thm.thm);
      }
      const auto res = rule.apply(antecedent_thms);
      const Theorem t(res, rule_name, antecedent_thms);
      add_theorem(as.empty()
                      ? "thm_" +
                            std::to_string(
                                frames.back().theorems.size())
                      : as.at(0).text.text,
                  t);
    } else if (p == "BACKWARD") {
      const auto rule_name = pf.at(0).text.text;
      const auto rule = get_rule(rule_name);
      const auto cur_goal = frames.back().goals.front();
      backward_apply(rule_name, rule, cur_goal);
    } else if (p == "WTS") {
      const auto goal = pf.at(0);
      const auto proof = pf.at(1);
      check_proof(goal, proof,
                  "wts_" + std::to_string(
                               frames.back().theorems.size()));
    }

    if (p == "LS" || debug) {
      std::cout << "ls at " << pf.text.file << ":"
                << pf.text.line << "." << pf.text.col << "\n";
      ls();
    }
  }

  // We must have proven it by now
  std::optional<Theorem> thm;
  if (has(_goal)) {
    thm = get_theorem(_goal);
  } else {
    const auto tok = _proof.text;
    std::cerr << "Unsolved goal: " << _goal
              << " at end of proof beginning at " << tok.file
              << ":" << tok.line << "." << tok.col << '\n';
    throw std::runtime_error("Proof does not solve its goal");
  }

  pop();

  add_theorem(_name, *thm);
}

void InferenceMaker::backward_apply(const std::string &_name,
                                    const InferenceRule &_rule,
                                    const ASTNode &_goal) {
  // If _what is of the form of the implication of the rule
  auto free_variables = _rule.free_variables;
  std::list<std::pair<ASTNode, ASTNode>> substitutions;
  if (_goal.is_of_form(_rule.consequence, free_variables,
                       substitutions)) {
    if (!free_variables.empty()) {
      if (debug) {
        std::cout << "Remaining FVs:";
        for (const auto &fv : free_variables) {
          std::cout << ' ' << fv;
        }
        std::cout << ", skipping " << _rule << " when WTS "
                  << _goal << "\n";
      }
      throw std::runtime_error(
          "Failed to resolve all free variables in 'backward'");
    }

    if (debug) {
      std::cout << "Trying rule: " << _rule << "\n";
    }

    std::vector<ASTNode> replaced_requirements;
    for (const auto &to_prove_schema : _rule.requirements) {
      const auto to_prove =
          to_prove_schema.replace(substitutions);
      replaced_requirements.push_back(to_prove);
    }

    Theorem t(_goal, _name, replaced_requirements);
    add_theorem(
        "bw_" + std::to_string(frames.back().theorems.size()),
        t);

    assert(replaced_requirements.size() ==
           _rule.requirements.size());

    // These are our new goals
    close_goal(_goal);
    for (const auto &goal : replaced_requirements) {
      open_goal(goal);
    }
    return;
  }

  // Error!
  std::cerr << "Expected " << _goal << " to be of form "
            << _rule.consequence.replace(substitutions)
            << " with free variables [";
  for (const auto &fv : free_variables) {
    std::cerr << ' ' << fv;
  }
  std::cerr << " ] (but failed pattern-match)\n";
  throw std::runtime_error("Failed backward rewrite");
}

void InferenceMaker::ls() const noexcept {
  uint i = 0;
  for (const auto &frame : frames) {
    std::cout << "Frame " << i << ":\n";
    ++i;
    for (const auto &p : frame.theorems) {
      std::cout << "  " << p.first << ": " << p.second.thm
                << '\n';
    }
    if (!frame.goals.empty()) {
      std::cout << "  Goals: ";
      bool first = true;
      for (const auto &p : frame.goals) {
        if (first) {
          first = false;
        } else {
          std::cout << ", ";
        }
        std::cout << p;
      }
      std::cout << "\n";
    }
  }
  if (!frames.back().goals.empty()) {
    std::cout << "Current goal: " << frames.back().goals.front()
              << "\n\n";
  }
}
