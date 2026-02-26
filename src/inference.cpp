/**
 * @brief Inference rule system implementation
 */

#include "inference.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

std::optional<InferenceMaker::InferenceRule>
InferenceMaker::InferenceRule::remove_first_req(
    const ASTNode &_sub) const noexcept {
  std::set<ASTNode> new_fv = free_variables;
  std::list<std::pair<ASTNode, ASTNode>> subs;

  // If not a valid replacement, return none
  if (!is_of_form(_sub, requirements.front(), new_fv, subs)) {
    return {};
  }

  std::list<ASTNode> new_reqs;
  bool first = true;
  for (const auto &req : requirements) {
    if (first) {
      first = false;
    } else {
      new_reqs.push_back(req.replace(subs));
    }
  }

  // Else, adjust and return accordingly
  return InferenceRule(new_fv, new_reqs,
                       consequence.replace(subs));
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
  if (_index >= known.size()) {
    throw std::runtime_error("Invalid theorem index " +
                             std::to_string(_index));
  }
  return known.at(_index);
}

bool InferenceMaker::is_of_form(
    const ASTNode &_to_examine, const ASTNode &_form,
    std::set<ASTNode> &_free_variables,
    std::list<std::pair<ASTNode, ASTNode>> &_substitutions) {
  // Existing replacements
  for (const auto &p : _substitutions) {
    if (p.first == _form) {
      return _to_examine == p.second;
    }
  }

  // New replacement
  if (_free_variables.contains(_form)) {
    _substitutions.push_back({_form, _to_examine});
    _free_variables.erase(_form);
    return true;
  }

  // Else, _form is not a free variable directly. Recurse like
  // a funky equality
  if (_to_examine.text != _form.text ||
      _to_examine.children.size() != _form.children.size()) {
    return false;
  }
  for (uint child = 0; child < _to_examine.children.size();
       ++child) {
    if (!is_of_form(_to_examine.children.at(child),
                    _form.children.at(child), _free_variables,
                    _substitutions)) {
      return false;
    }
  }
  return true;
}

int InferenceMaker::has(const ASTNode &_what) const noexcept {
  for (int i = known.size() - 1; i >= 0; --i) {
    if (known.at(i).thm == _what) {
      return i;
    }
  }
  return -1;
}

size_t
InferenceMaker::add_axiom(const ASTNode &_what) noexcept {
  known.push_back({known.size(), _what, -1, {}});
  if (debug) {
    std::cout << "Added axiom: " << _what << "\n\n";
  }
  return known.back().index;
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
    const std::list<ASTNode> &_req, const ASTNode &_cons)
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
    std::cerr << "WARNING: Rule is not backward-derivable! "
              << *this << "\n";
  } else {
    std::cerr << "Rule which is neither forward- nor "
                 "backward-derivable: "
              << *this << "\n\n";
    throw std::runtime_error(
        "Rule is neither forward-derivable nor "
        "backward-derivable: Not all free variables occur in "
        "the requirements nor do all of them occur in the "
        "consequence");
  }
}

std::optional<InferenceMaker::Theorem>
InferenceMaker::backward_prove(const ASTNode &_what,
                               const int &_passes) {
  // If we have already proven this, return that proof
  const int res = has(_what);
  if (res >= 0) {
    return get_theorem(res);
  }

  if (_passes <= 0) {
    return {};
  }

  // Examine each rule
  for (uint rule_index = 0; rule_index < rules.size();
       ++rule_index) {
    const auto rule = rules.at(rule_index);
    if (rule.type == InferenceRule::FORWARD_ONLY) {
      continue;
    }

    // If _what is of the form of the implication of the rule
    auto free_variables = rule.free_variables;
    std::list<std::pair<ASTNode, ASTNode>> substitutions;
    if (is_of_form(_what, rule.consequence, free_variables,
                   substitutions)) {
      assert(free_variables.empty());

      // Now we have to prove that, given these substitutions,
      // ALL of the LHS of the implication are provable
      bool rule_works = true;
      std::list<size_t> premises;
      for (const auto &to_prove_schema : rule.requirements) {
        const auto to_prove =
            to_prove_schema.replace(substitutions);

        const std::optional<Theorem> res =
            backward_prove(to_prove, _passes - 1);

        if (!res.has_value()) {
          rule_works = false;
          break;
        }
        premises.push_back(res.value().index);
      }

      if (rule_works) {
        // Add the proven thing and return
        bool trash = true;
        return add_theorem(_what, rule_index, premises, trash);
      }
    }
  }

  // No rule worked
  if (enable_alternation) {
    // Alternate to forward_prove (with reduced pass bound)
    return forward_prove(_what, _passes - 1);
  }

  return {};
}

void InferenceMaker::inst_all(
    const uint &_rule_index, const uint &_first_n_thms,
    const std::vector<uint> &_cur_indices) {

  static std::set<std::pair<uint, std::vector<uint>>>
      nontheorem_pairings;

  const auto rule = rules.at(_rule_index);

  if (_cur_indices.size() < rule.requirements.size()) {
    std::vector<uint> to_visit;
    for (uint i = 0; i < known.size() && i < _first_n_thms;
         ++i) {
      std::vector<uint> next_ind = _cur_indices;
      next_ind.push_back(i);

      inst_all(_rule_index, _first_n_thms, next_ind);
    }
  } else {
    if (nontheorem_pairings.contains(
            {_rule_index, _cur_indices})) {
      return;
    }

    // Determine substitutions, if they exist
    auto fv = rule.free_variables;
    std::list<std::pair<ASTNode, ASTNode>> substitutions;
    uint req_ind = 0;
    for (const auto &corresponding_requirement :
         rule.requirements) {
      const auto thm =
          get_theorem(_cur_indices.at(req_ind)).thm;
      if (!is_of_form(
              thm,
              corresponding_requirement.replace(substitutions),
              fv, substitutions)) {
        nontheorem_pairings.insert({_rule_index, _cur_indices});
        return;
      }
      ++req_ind;
    }

    // Add the thing
    bool actually_added = true;
    std::list<size_t> premises;
    for (const auto &item : _cur_indices) {
      premises.push_back(item);
    }

    const auto replaced_cons =
        rule.consequence.replace(substitutions);
    const auto res =
        add_theorem(rule.consequence.replace(substitutions),
                    _rule_index, premises, actually_added);

    if (!actually_added) {
      nontheorem_pairings.insert({_rule_index, _cur_indices});
    }
  }
}

std::optional<InferenceMaker::Theorem>
InferenceMaker::forward_prove(const ASTNode &_what,
                              const int &_passes) {
  // If we have already proven this, return that proof
  const int res = has(_what);
  if (res >= 0) {
    return get_theorem(res);
  }

  // For however many passes
  for (int cur_pass = 0; cur_pass < _passes; ++cur_pass) {
    // Apply all rules
    uint n_instantiated = 0;

    // For each rule
    for (uint rule_index = 0; rule_index < rules.size();
         ++rule_index) {
      const auto rule = rules.at(rule_index);
      if (rule.type == InferenceRule::BACKWARD_ONLY) {
        if (debug) {
          std::cout << "In forward pass " << cur_pass << " of "
                    << _passes << " skipping rule " << rule
                    << " of total " << rules.size() << "\n";
        }
        ++rule_index;
        continue;
      }

      // Attempt to find ONE instantiation
      if (debug) {
        std::cout << "In forward pass " << cur_pass << " of "
                  << _passes << " examining rule " << rule
                  << " of total " << rules.size() << "\n";
      }

      const auto n_known_before = known.size();
      inst_all(rule_index, n_known_before);
      if (known.size() != n_known_before) {
        n_instantiated += (known.size() - n_known_before);

        // If the thing is proven, return early
        const auto ind = has(_what);
        if (ind >= 0) {
          return get_theorem(ind);
        }
      }
    }

    if (debug) {
      std::cout << "Pass " << cur_pass << " produced "
                << n_instantiated << " new theorems\n\n";
    }

    // Continuing would be dumb
    if (n_instantiated == 0) {
      break;
    }
  }

  // No rule worked
  if (enable_alternation) {
    // Alternate to backward_prove (with reduced pass bound)
    return backward_prove(_what, _passes - 1);
  }

  return {};
}

const InferenceMaker::Theorem InferenceMaker::add_theorem(
    const ASTNode &_thm, const uint &_rule_index,
    const std::list<size_t> &_premises, bool &_actually_added) {
  const auto beta_reduced_thm = _thm.beta_star();

  const auto res = has(beta_reduced_thm);
  if (res >= 0) {
    _actually_added = false;
    return get_theorem(res);
  }

  const Theorem out = {.index = known.size(),
                       .thm = beta_reduced_thm,
                       .rule_index = _rule_index,
                       .premises = _premises};
  known.push_back(out);

  if (debug) {
    std::cout << "Derived theorem " << out << "\n\n";
  }

  return out;
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
