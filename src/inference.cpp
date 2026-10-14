/**
 * @brief Inference rule system implementation
 */

#include "inference.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

ASTNode
InferenceMaker::proof_to_ast(const size_t &_thm_index) const {
  const auto thm = get_theorem(_thm_index);
  if (thm.rule_index < 0) {
    return ASTNode("axiom", {thm.thm});
  } else {
    ASTNode premises_block("premises");
    for (const auto &premise : thm.premises) {
      premises_block.children.push_back(proof_to_ast(premise));
    }

    const auto rule = get_rule(thm.rule_index);
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

const InferenceMaker::InferenceRule
InferenceMaker::get_rule(const uint &_index) const {
  if (_index >= rules.size()) {
    throw std::runtime_error("Invalid rule index " +
                             std::to_string(_index));
  }
  return rules.at(_index);
}

const InferenceMaker::Theorem
InferenceMaker::get_theorem(const uint &_index) const {
  if (_index >= facts.size()) {
    throw std::runtime_error("Invalid theorem index " +
                             std::to_string(_index));
  }
  return facts.at(_index);
}

const InferenceMaker::InferenceRule
InferenceMaker::get_rule(const std::string &_name,
                         size_t &_index) const {
  int index = -1;
  for (size_t i = 0; i < rules.size(); ++i) {
    const auto rule = rules.at(i);
    if (rule.name.value_or(std::to_string(i)) == _name) {
      if (index != -1) {
        throw std::runtime_error(
            "Multiple rules are identified by '" + _name + "'");
      }
      index = i;
    }
  }

  if (index != -1) {
    _index = index;
    return get_rule(index);
  }

  throw std::runtime_error("Invalid rule identifier " + _name);
}

const InferenceMaker::Theorem
InferenceMaker::get_theorem(const std::string &_name) const {
  int index = -1;
  for (size_t i = 0; i < facts.size(); ++i) {
    const auto thm = facts.at(i);
    if (thm.name.value_or(std::to_string(i)) == _name) {
      if (index != -1) {
        throw std::runtime_error(
            "Multiple theorems are identified by '" + _name +
            "'");
      }
      index = i;
    }
  }

  if (index != -1) {
    return get_theorem(index);
  }

  throw std::runtime_error("Invalid theorem identifier " +
                           _name);
}

int InferenceMaker::has(const ASTNode &_what) const noexcept {
  for (int i = facts.size() - 1; i >= 0; --i) {
    if (facts.at(i).thm == _what) {
      return i;
    }
  }
  return -1;
}

size_t
InferenceMaker::add_axiom(const ASTNode &_what) noexcept {
  pop_pending(_what);
  facts.push_back({{}, facts.size(), _what, -1, {}});
  if (debug) {
    std::cout << "Added axiom: " << _what << "\n\n";
  }
  return facts.back().index;
}

void InferenceMaker::add_rule(const InferenceRule &_rule) {
  rules.push_back(_rule);
  if (debug) {
    std::cout << "Added rule w/ index " << rules.size() - 1
              << ": " << _rule << "\n\n";
  }
}

InferenceMaker::InferenceRule::InferenceRule(
    const std::set<ASTNode> &_fv,
    const std::vector<ASTNode> &_req, const ASTNode &_cons)
    : free_variables(_fv), requirements(_req),
      consequence(_cons) {
  bool has_fvs_in_cons = true;
  bool has_fvs_in_reqs = true;
  for (const auto &fv : free_variables) {
    if (has_fvs_in_cons && !consequence.contains(fv)) {
      has_fvs_in_cons = false;
    } else if (has_fvs_in_reqs) {
      bool has_a_req_w_var = false;
      for (const auto &rule : requirements) {
        if (rule.contains(fv)) {
          has_a_req_w_var = true;
          break;
        }
      }

      if (!has_a_req_w_var) {
        has_fvs_in_reqs = false;
        break;
      }
    }
  }

  // Replace is whack
  if (consequence.contains("REPLACE")) {
    has_fvs_in_cons = false;
  }

  // Classify type
  if (has_fvs_in_cons && has_fvs_in_reqs) {
    type = BIDIRECTIONAL;
  } else if (has_fvs_in_cons) {
    type = BACKWARD_ONLY;
  } else if (has_fvs_in_reqs) {
    type = FORWARD_ONLY;
  } else {
    std::cerr << "Offending rule: " << *this << '\n';
    throw std::runtime_error(
        "Rule is neither forward-derivable nor "
        "backward-derivable: Not all free variables occur in "
        "the requirements nor do all of them occur in the "
        "consequence");
  }
}

ASTNode InferenceMaker::InferenceRule::apply(
    const std::vector<InferenceMaker::Theorem> &_premises)
    const {
  std::set<ASTNode> fv = free_variables;
  std::list<std::pair<ASTNode, ASTNode>> substitutions;
  for (size_t i = 0; i < _premises.size(); ++i) {
    const auto corresponding_requirement = requirements.at(i);
    const auto thm = _premises.at(i);

    if (!thm.thm.is_of_form(
            corresponding_requirement.replace(substitutions),
            fv, substitutions)) {
      std::cerr << "Expected " << thm.thm << " to be of form "
                << corresponding_requirement.replace(
                       substitutions)
                << " with free variables [";
      for (const auto &fv : free_variables) {
        std::cerr << ' ' << fv;
      }
      std::cerr << " ] (but failed pattern-match)\n";
      throw std::runtime_error("Malformed rule application");
    }
  }
  return consequence.replace(substitutions);
}

const InferenceMaker::Theorem InferenceMaker::add_theorem(
    const ASTNode &_thm,
    const std::optional<std::string> &_name,
    const size_t &_rule_index,
    const std::vector<size_t> &_premises,
    bool &_actually_added) {
  const auto res = has(_thm);
  if (res >= 0) {
    _actually_added = false;
    return get_theorem(res);
  }

  const Theorem out = {.name = _name,
                       .index = facts.size(),
                       .thm = _thm,
                       .rule_index =
                           static_cast<intmax_t>(_rule_index),
                       .premises = _premises};
  facts.push_back(out);
  pop_pending(_thm);

  if (debug) {
    std::cout << "Derived theorem " << out << "\n\n";
  }

  return out;
}

ASTNode InferenceMaker::theorem_to_proof(
    const ASTNode &_theorem) const {
  for (auto rit = facts.rbegin(); rit != facts.rend(); ++rit) {
    if (rit->thm == _theorem) {
      return proof_to_ast(rit->index);
    }
  }
  throw std::runtime_error("Cannot get proof of nontheorem!");
}

std::ostream &
operator<<(std::ostream &_strm,
           const InferenceMaker::InferenceRule &_rule) {
  _strm << "[";
  switch (_rule.type) {
  case InferenceMaker::InferenceRule::FORWARD_ONLY:
    _strm << "forward";
    break;
  case InferenceMaker::InferenceRule::BACKWARD_ONLY:
    _strm << "backward";
    break;
  case InferenceMaker::InferenceRule::BIDIRECTIONAL:
    _strm << "bidirectional";
    break;
  }
  _strm << "]<";
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
  if (_thm.rule_index < 0) {
    _strm << "axiom: " << _thm.thm;
    return _strm;
  }

  _strm << "thm " << _thm.index << ": " << _thm.thm
        << " due to rule " << _thm.rule_index
        << " on premises (";
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

  ASTNode goal = _goal;
  goals.push_back(goal);
  if (_proof.text != "PROOF") {
    throw std::runtime_error("Cannot check a non-proof!");
  }

  // Check proof
  for (const auto &pf : _proof.children) {
    const auto p = pf.text.text;
    if (p == "FORWARD") {
      const auto rule_name = pf.at(0).text.text;
      const auto on = pf.at(1);
      const auto as = pf.at(2);
      size_t rule_index = 0;
      const auto rule = get_rule(rule_name, rule_index);

      std::vector<Theorem> antecedents;
      std::vector<size_t> antecedent_indices;
      for (const auto &arg : on.children) {
        const auto thm = get_theorem(arg.text.text);
        antecedents.push_back(thm);
        antecedent_indices.push_back(thm.index);
      }

      const auto res = rule.apply(antecedents);
      bool actually_added = false;
      add_theorem(res,
                  as.empty() ? std::optional<std::string>{}
                             : as.at(0).text.text,
                  rule_index, antecedent_indices,
                  actually_added);
    } else if (p == "BACKWARD") {
      const auto rule_name = pf.at(0).text.text;
      size_t rule_index = 0;
      const auto rule = get_rule(rule_name, rule_index);

      backward_apply(rule, goal);
      if (goals.empty()) {
        return;
      }
      goal = goals.front();
    } else if (p == "WTS") {
      push();
      const auto goal = pf.at(0);
      const auto proof = pf.at(1);
      check_proof(goal, proof, "NULL");
      pop();
    }

    else if (p == "LS") {
      std::cout << "At " << pf.text.file << ":" << pf.text.line
                << "." << pf.text.col << "\n";
      for (const auto &fact : facts) {
        std::cout << fact.name.value_or(
                         std::to_string(fact.index))
                  << ": " << fact.thm << '\n';
      }
      std::cout << "Goal: " << goal << "\n\n";
    }
  }

  // We must have proven it by now
  bool failed = false;
  for (const auto &goal : goals) {
    if (has(goal) >= 0) {
      continue;
    }
    failed = true;
    std::cout << "Unresolved goal: " << goal << '\n';
  }

  if (failed) {
    throw std::runtime_error("Unresolved goal(s)");
  }
}

void InferenceMaker::backward_apply(const InferenceRule &_rule,
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

    const std::string fresh_uid = std::to_string(facts.size());
    std::list<std::pair<ASTNode, ASTNode>>
        freshening_replacements;

    std::list<ASTNode> replaced_requirements;
    for (const auto &to_prove_schema : _rule.requirements) {
      const auto to_prove =
          to_prove_schema.replace(substitutions);
      replaced_requirements.push_back(to_prove);
    }

    assert(replaced_requirements.size() ==
           _rule.requirements.size());

    // These are our new goals
    for (const auto &goal : replaced_requirements) {
      goals.push_back(goal);
    }
    std::erase_if(goals, [&](const ASTNode &_g) -> bool {
      return has(_g);
    });
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
