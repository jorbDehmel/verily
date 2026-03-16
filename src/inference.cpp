/**
 * @brief Inference rule system implementation
 */

#include "inference.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

ASTNode
InferenceMaker::proof_to_ast(const size_t &_thm_index) const {
  if (special_proofs.contains(_thm_index)) {
    return special_proofs.at(_thm_index);
  }

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
  pending.erase(_what);
  known.push_back({known.size(), _what, -1, {}});

  if (meta_proving && _what.text == "==") {
    if (debug) {
      std::cout << "Congruence adding " << _what.children.at(0)
                << " and " << _what.children.at(1) << "\n";
    }
    congruences.relate(_what.children.at(0),
                       _what.children.at(1));
  }

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
    std::cerr << "WARNING: Rule requires alternation! " << *this
              << "\n";
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

  if (_what.get_height() > max_tree_height) {
    if (debug) {
      std::cerr << "Killing branch: Tree too big!";
    }
    return {};
  }

  // If we're out of passes
  if (_passes <= 0) {
    if (debug) {
      std::cerr << "Out of passes in backward mode!\n";
    }
    return {};
  }

  // Examine each rule
  for (uint rule_index = 0; rule_index < rules.size();
       ++rule_index) {
    const auto rule = rules.at(rule_index);
    if (debug) {
      std::cout << "Checking rule " << rule << "\n";
    }

    // If _what is of the form of the implication of the rule
    auto free_variables = rule.free_variables;
    std::list<std::pair<ASTNode, ASTNode>> substitutions;
    if (is_of_form(_what, rule.consequence, free_variables,
                   substitutions)) {
      if (!free_variables.empty()) {
        if (debug) {
          std::cerr << "Remaining FVs:";
          for (const auto &fv : free_variables) {
            std::cerr << ' ' << fv;
          }
          std::cerr << ", skipping " << rule << " when WTS "
                    << _what << "\n";
        }

        continue;
      }

      const std::string fresh_uid =
          std::to_string(known.size());
      std::list<std::pair<ASTNode, ASTNode>>
          freshening_replacements;

      // Adds all FRESH(a) => FRESH_a_12345 to
      // freshening_replacements, where the int suffix is unique
      // to this rule application
      std::function<void(
          const ASTNode &, const std::string &,
          std::list<std::pair<ASTNode, ASTNode>> &)>
          handle_fresh_vars =
              [&handle_fresh_vars](
                  const ASTNode &_root, const std::string &_uid,
                  std::list<std::pair<ASTNode, ASTNode>>
                      &_freshening_replacements) -> void {
        if (_root.text == "fresh") {
          if (_root.children.size() != 1) {
            throw std::runtime_error(
                "'fresh' takes 1 argument");
          }
          if (!_root.children.front().children.empty()) {
            throw std::runtime_error("Argument to 'fresh' must "
                                     "be an atomic identifier");
          }
          _freshening_replacements.push_back(
              {_root, ASTNode("FRESH_" +
                              _root.children.front().text.text +
                              "_" + _uid)});
        } else {
          for (const auto &child : _root.children) {
            handle_fresh_vars(child, _uid,
                              _freshening_replacements);
          }
        }
      };

      if (_what.contains("fresh")) {
        throw std::runtime_error(
            "Consequence of rule application in backwards mode "
            "has 'fresh' calls: How did you even do that?");
      }
      std::list<ASTNode> replaced_requirements;
      for (const auto &to_prove_schema : rule.requirements) {
        const auto to_prove =
            to_prove_schema.replace(substitutions);
        replaced_requirements.push_back(to_prove);

        handle_fresh_vars(to_prove, fresh_uid,
                          freshening_replacements);
      }

      assert(replaced_requirements.size() ==
             rule.requirements.size());

      // Now we have to prove that, given these substitutions,
      // ALL of the LHS of the implication are provable
      bool rule_works = true;
      std::list<size_t> premises;
      for (const auto &to_prove_minus_fresh :
           replaced_requirements) {
        const ASTNode to_prove = to_prove_minus_fresh.replace(
            freshening_replacements);
        if (debug) {
          std::cout << "Checking premise " << to_prove << "\n";
        }

        const std::optional<Theorem> res =
            prove(to_prove, _passes);

        if (!res.has_value()) {
          rule_works = false;
          if (debug) {
            std::cout << "Rule failed!\n";
          }
          break;
        }
        premises.push_back(res.value().index);
      }

      if (rule_works) {
        // Add the proven thing and return
        bool trash = true;
        if (debug) {
          std::cout << "Rule worked!\n";
        }
        return add_theorem(_what, rule_index, premises, trash);
      }
    } else if (debug) {
      std::cout << "Theorem is not of rule's form.\n";
    }
  }

  if (debug) {
    std::cout << "backward_prove failed\n";
  }

  // No rule worked
  if (enable_alternation) {
    cur_alternation_is_forward = true;
    return prove(_what, _passes);
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

      if (thm.contains("fresh")) {
        throw std::runtime_error(
            "Antecedent of rule application in forwards mode "
            "has 'fresh' calls: How did you even do that?");
      }

      ++req_ind;
    }

    // Add the thing
    bool actually_added = true;
    std::list<size_t> premises;
    for (const auto &item : _cur_indices) {
      premises.push_back(item);
    }

    const std::string fresh_uid = std::to_string(known.size());

    std::function<ASTNode(const ASTNode &)> deal_with_fresh =
        [&](const ASTNode &_root) -> ASTNode {
      if (_root.text == "fresh") {
        if (_root.children.size() != 1) {
          throw std::runtime_error("'fresh' takes 1 argument");
        }
        if (!_root.children.front().children.empty()) {
          throw std::runtime_error("Argument to 'fresh' must "
                                   "be an atomic identifier");
        }
        return ASTNode("FRESH_" +
                       _root.children.front().text.text + "_" +
                       fresh_uid);
      } else {
        ASTNode out;
        out.text = _root.text;
        for (const auto &child : _root.children) {
          out.children.push_back(deal_with_fresh(child));
        }
        return out;
      }
    };

    const ASTNode replaced_cons = deal_with_fresh(
        rule.consequence.replace(substitutions));

    const auto res = add_theorem(replaced_cons, _rule_index,
                                 premises, actually_added);

    nontheorem_pairings.insert({_rule_index, _cur_indices});
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

  if (debug) {
    std::cout << "forward_prove failed\n";
  }

  // No rule worked
  if (enable_alternation) {
    // Alternate to backward_prove (with reduced pass bound)
    cur_alternation_is_forward = false;
    return prove(_what, _passes);
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

  if (known.size() >= theorem_limit) {
    throw std::runtime_error(
        "Emergency stop: Too many theorems!");
  }

  if (meta_proving && beta_reduced_thm.text == "==") {
    congruences.relate(beta_reduced_thm.children.at(0),
                       beta_reduced_thm.children.at(1));
  }

  const Theorem out = {.index = known.size(),
                       .thm = beta_reduced_thm,
                       .rule_index = _rule_index,
                       .premises = _premises};
  known.push_back(out);
  pending.erase(beta_reduced_thm);

  if (debug) {
    std::cout << "Derived theorem " << out << "\n\n";
  }

  return out;
}

ASTNode InferenceMaker::theorem_to_proof(
    const ASTNode &_theorem) const {
  for (auto rit = known.rbegin(); rit != known.rend(); ++rit) {
    if (rit->thm == _theorem) {
      return proof_to_ast(rit->index);
    }
  }
  throw std::runtime_error("Cannot get proof of nontheorem!");
}

std::optional<InferenceMaker::Theorem>
InferenceMaker::prove(const ASTNode &_theorem,
                      const int &_passes) {
  if (debug) {
    std::cout << "WTS " << _theorem << " w/ height "
              << _theorem.get_height() << "\nWith pending:\n";
    for (const auto &p : pending) {
      std::cout << " - " << p << "\n";
    }
  }

  if (_theorem.get_height() > max_tree_height) {
    if (debug) {
      std::cerr << "Killing branch: Tree too big!";
    }
    return {};
  }

  bool is_pending = false;
  for (const auto &p : pending) {
    if (p == _theorem) {
      is_pending = true;
      break;
    }
  }

  if (is_pending) {
    const auto res = has(_theorem);
    if (res >= 0) {
      pending.erase(_theorem);
      return get_theorem(res);
    } else {
      return {};
    }
  }
  pending.insert(_theorem);

  // Meta special case(s)
  if (meta_proving) {
    if (debug) {
      std::cout << "Checking meta-proving techniques...\n";
    }

    if (_theorem.text == "implies") {
      const ASTNode premise = _theorem.children.at(0);
      const ASTNode consequence = _theorem.children.at(1);

      push();
      special_proofs[add_axiom(premise)] =
          ASTNode("assumption", {premise});
      const auto res = prove(consequence, _passes - 1);
      pending.erase(consequence);

      if (res.has_value()) {
        // Success
        const auto proof_of_consequence =
            proof_to_ast(res.value().index);
        pop();

        const int out_ind = add_axiom(_theorem);
        special_proofs[out_ind] =
            ASTNode("meta", {proof_of_consequence, _theorem});
        return get_theorem(out_ind);
      }

      // Failed to derive
      pop();
    } else if (_theorem.text == "==") {
      if (debug) {
        std::cout << "Congruence checking "
                  << _theorem.children.at(0) << " against "
                  << _theorem.children.at(1) << "\n";
      }
      if (congruences.are_related(_theorem.children.at(0),
                                  _theorem.children.at(1))) {
        if (debug) {
          std::cout << "Congruence check worked.\n";
        }
        const int out_ind = add_axiom(_theorem);
        special_proofs[out_ind] =
            ASTNode("meta", {ASTNode("congruence"), _theorem});
        return get_theorem(out_ind);
      }
    }

    if (debug) {
      std::cout << "No meta-proving techniques worked.\n";
    }
  }

  // Normal case
  if (enable_alternation && cur_alternation_is_forward) {
    if (debug) {
      std::cout << "Forward solving\n";
    }
    return forward_prove(_theorem, _passes - 1);
  } else {
    if (debug) {
      std::cout << "Backward solving\n";
    }
    return backward_prove(_theorem, _passes - 1);
  }
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
