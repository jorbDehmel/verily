/**
 * @brief Inference rule system implementation
 */

#include "inference.hpp"
#include "ast.hpp"
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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
  if (_thm.text == "syntactically_equivalent") {
    if (_thm.size() == 2 &&
        _thm.children.front() == _thm.children.back()) {
      return Theorem({}, {}, _thm, ASTNode(_thm.text));
    }
  } else if (_thm.text == "not_syntactically_equivalent") {
    if (_thm.size() == 2 &&
        _thm.children.front() != _thm.children.back()) {
      return Theorem({}, {}, _thm, ASTNode(_thm.text));
    }
  }

  else {
    for (auto rit = frames.rbegin(); rit != frames.rend();
         ++rit) {
      for (const auto &p : rit->theorems) {
        if (p.second.consequence == _thm) {
          return p.second;
        }
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
      if (p.second.consequence == _what) {
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
      if (p.second.consequence == _what) {
        return p.first;
      }
    }
  }
  std::cerr << "Offender: " << _what << '\n';
  throw std::runtime_error("Failed to find theorem name");
}

void InferenceMaker::add_assumption(
    const std::string &_name, const ASTNode &_what) noexcept {
  const std::string name = fix_name(_name, "h");
  frames.back().theorems[name] = Theorem({}, {}, _what, {});
  if (debug) {
    std::cout << "Added assumption: " << _what << "\n\n";
  }
}

const InferenceMaker::Theorem
InferenceMaker::add_theorem(const std::string &_name,
                            const Theorem &_thm) {
  const std::string name = fix_name(_name, "t");
  frames.back().theorems[_name] = _thm;
  close_goal(_thm.consequence);
  if (debug) {
    std::cout << "Derived theorem " << _thm.consequence
              << "\n\n";
  }
  return _thm;
}

// Note: _name is either "NULL" or an actual name.
std::optional<InferenceMaker::Theorem>
InferenceMaker::construct_proof(const ASTNode &_goal,
                                const ASTNode &_proof,
                                const std::string &_name) {
  if (debug) {
    std::cout << "Checking if " << _proof << " proves " << _goal
              << '\n';
  }

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
      const auto rule = get_theorem(rule_name);
      std::vector<ASTNode> antecedent_thms;
      std::vector<ASTNode> antecedent_pfs;
      for (const auto &arg : on.children) {
        const auto thm = get_theorem(arg.text.text);
        antecedent_thms.push_back(thm.consequence);
        antecedent_pfs.push_back(
            thm.proof.value_or(ASTNode("ASSUMPTION")));
      }
      const auto res = rule.apply(antecedent_thms);
      const Theorem t(
          {}, {}, res,
          ASTNode(
              "THEOREM",
              {res, ASTNode("APPLICATION",
                            {rule.to_ASTNode(),
                             ASTNode("_", antecedent_pfs)})}));
      add_theorem(as.empty() ? fresh_with_prefix("l")
                             : as.at(0).text.text,
                  t);
    } else if (p == "SWEEP") {
      // Apply everything to everything for some number of
      // rounds
      std::cout
          << "WARNING: Invoking sweep! You should "
             "replace this with explicit fw statements!\n";

      const std::function<bool(
          const InferenceRule &,
          const std::list<std::pair<std::string, Theorem>> &,
          const std::string &, const std::vector<ASTNode> &)>
          apply_all =
              [&](const InferenceRule &_r,
                  const std::list<
                      std::pair<std::string, Theorem>>
                      _facts,
                  const std::string &_rule_name,
                  const std::vector<ASTNode> &_antecedents)
          -> bool {
        if (_r.requirements.empty()) {
          const auto thm = _r.consequence;
          if (has(thm)) {
            return false;
          }
          add_theorem(
              fresh_with_prefix("s"),
              Theorem({}, {}, thm,
                      ASTNode(_rule_name, _antecedents)));
          return true;
        }
        bool out = false;
        for (const auto &fact : _facts) {
          // Replace first antecedent with this fact if
          // possible. If not, continue.
          std::set<ASTNode> fvs = _r.free_variables;
          std::list<std::pair<ASTNode, ASTNode>> subs;
          if (fact.second.consequence.is_of_form(
                  _r.requirements.front(), fvs, subs)) {
            // Adjust rule
            InferenceRule modified_rule;
            modified_rule.consequence =
                _r.consequence.replace(subs);
            modified_rule.free_variables = fvs;
            for (size_t i = 1; i < _r.requirements.size();
                 ++i) {
              modified_rule.requirements.push_back(
                  _r.requirements.at(i).replace(subs));
            }

            // Recurse
            std::vector<ASTNode> modified_antecedents =
                _antecedents;
            modified_antecedents.push_back(
                fact.second.consequence);
            out |= apply_all(modified_rule, _facts, _rule_name,
                             modified_antecedents);
          }
        }
        return out;
      };

      const auto n = pf.at(0).text.text;
      bool inf = false;
      size_t rounds = 0;
      if (n == "inf") {
        inf = true;
      } else {
        try {
          rounds = std::stoull(n);
        } catch (...) {
          throw std::runtime_error(
              "Invalid argument '" + n +
              "' to sweep: Must be a number or inf");
        }
      }

      bool progress = true;
      for (size_t cur_round = 0;
           (inf || cur_round < rounds) && progress;
           ++cur_round) {
        const auto facts = get_all_facts();
        for (const auto &rule : facts) {
          // Apply to all combinations of facts
          progress |=
              apply_all(rule.second, facts, rule.first, {});
        }

        // Note: Sweep never changes to goal
        if (has(_goal)) {
          std::cout << "WARNING: Halting sweep after "
                    << cur_round << '\n';
          break;
        }
      }
    } else if (p == "REWRITE") {
      const auto rule_name = pf.at(0).text.text;
      const auto on = pf.at(1).at(0).text.text;
      const auto as = pf.at(2);
      const auto rule = get_theorem(rule_name);
      bool did_something = false;
      const auto ante = get_theorem(on);
      const auto res =
          rule.apply_subexpr(ante.consequence, did_something);
      if (!did_something) {
        std::cerr << "Failed to rewrite " << ante.consequence
                  << " with rule " << rule_name << ' ' << rule
                  << '\n';
        throw std::runtime_error("Failed rewrite");
      }
      const Theorem t({}, {}, res, pf);
      add_theorem(as.empty() ? fresh_with_prefix("l")
                             : as.at(0).text.text,
                  t);
    } else if (p == "BACKWARD") {
      const auto rule_name = pf.at(0).text.text;
      const auto rule = get_theorem(rule_name);
      const auto cur_goal = frames.back().goals.front();
      backward_apply(rule_name, rule, cur_goal);
    } else if (p == "WTS") {
      const auto goal = pf.at(0);
      const auto proof = pf.at(1);
      construct_proof(goal, proof, fresh_with_prefix("wts"));
    } else if (p == "THEOREM") {
      const auto over = pf.at(0);
      const auto given = pf.at(1);
      const auto deduce = pf.at(2);
      const auto proof = pf.at(3);
      const auto name = pf.at(4).text.text;
      const auto real_name =
          name == "" ? fresh_with_prefix("t") : name;
      std::set<ASTNode> fvs;
      for (const auto &v : over.children) {
        fvs.insert(v);
      }
      push(fvs);
      for (const auto &assumption : given.children) {
        add_assumption(fresh_with_prefix("h"), assumption);
      }
      construct_proof(deduce.at(0), proof, real_name);
      pop();
    } else if (p == "HAVE") {
      const auto name = pf.at(0).text.text;
      const auto proving = pf.at(1);
      add_theorem(fix_name(name, "have"), get_theorem(proving));
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
  add_theorem(_name, *thm);
  return *thm;
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
    std::vector<ASTNode> partial_subproofs;
    for (const auto &to_prove_schema : _rule.requirements) {
      const auto to_prove =
          to_prove_schema.replace(substitutions);
      replaced_requirements.push_back(to_prove);
      partial_subproofs.push_back(
          ASTNode("THEOREM", {to_prove, {"REPLACE_ME"}}));
    }

    Theorem t(
        {}, {}, _goal,
        ASTNode(
            "THEOREM",
            {_goal,
             ASTNode("APPLICATION",
                     {_rule.to_ASTNode(),
                      ASTNode("_", {partial_subproofs})})}));
    add_theorem(fresh_with_prefix("bw"), t);

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
      std::cout << "  " << p.first << ": "
                << p.second.consequence << '\n';
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
              << "\n";
  }
}

std::list<std::pair<std::string, InferenceMaker::Theorem>>
InferenceMaker::get_all_facts() const noexcept {
  std::list<std::pair<std::string, InferenceMaker::Theorem>>
      out;
  for (const auto &frame : frames) {
    for (const auto &p : frame.theorems) {
      out.push_back(p);
    }
  }
  return out;
}

bool InferenceMaker::name_is_used(
    const std::string &_name) const noexcept {
  for (const auto &frame : frames) {
    if (frame.theorems.contains(_name)) {
      return true;
    }
  }
  return false;
}

std::string InferenceMaker::fresh_with_prefix(
    const std::string &_prefix) const noexcept {
  size_t i = 0;
  while (name_is_used(_prefix + std::to_string(i))) {
    ++i;
  }
  return _prefix + std::to_string(i);
}

std::string InferenceMaker::fix_name(
    const std::string &_name,
    const std::string &_prefix) const noexcept {
  if (_name == "NULL") {
    return fresh_with_prefix(_prefix);
  }
  return _name;
}

void InferenceMaker::check_proof(const Theorem &_thm) {
  const std::function<void(const ASTNode &, const ASTNode &)>
      check_subproof = [&](const ASTNode &_goal,
                           const ASTNode &_pf) {
        if (debug) {
          std::cout << "Checking if " << _pf << " proves "
                    << _goal << "\n";
        }

        if (_pf.text != "THEOREM") {
          std::cerr << "Current goal: " << _goal
                    << "\nCurrent proof: " << _pf << "\n";
          throw std::runtime_error(
              "Malformed proof in proof check: Expected "
              "'THEOREM', but saw '" +
              _pf.text.text + "'");
        } else if (_pf.size() != 2) {
          std::cerr << "Current goal: " << _goal
                    << "\nCurrent proof: " << _pf << "\n";
          throw std::runtime_error(
              "Malformed proof in proof check: 'THEOREM' node "
              "had wrong number of children");
        } else if (_pf.at(0) != _goal) {
          std::cerr << "Current goal is " << _goal
                    << ", but proof " << _pf
                    << " claims to prove " << _pf.at(0) << '\n';
          throw std::runtime_error(
              "Malformed proof in proof check: Goal mismatch!");
        }

        const auto pf = _pf.at(1);

        if (pf.text == "ASSUMPTION") {
          for (const auto &antecedent : _thm.requirements) {
            if (antecedent == _goal) {
              return;
            }
          }

          std::cerr << "Current goal: " << _goal
                    << "\nCurrent proof: " << _pf << "\n";
          throw std::runtime_error(
              "Malformed proof in proof check: Claimed "
              "assumption not present in antecedents");
        }

        else if (pf.text == "APPLICATION") {
          if (pf.size() != 2 || pf.at(1).text != "_") {
            std::cerr << "Current goal: " << _goal
                      << "\nCurrent proof: " << _pf << "\n";
            throw std::runtime_error(
                "Malformed proof in proof check: 'APPLICATION' "
                "node is malformed");
          }

          const auto rule_pf = pf.at(0);
          const auto arg_pfs = pf.at(1).children;

          const InferenceRule rule =
              InferenceRule::from_ASTNode(rule_pf);
          check_proof(rule);

          if (rule.requirements.size() != arg_pfs.size()) {
            std::cerr << "Current goal: " << _goal
                      << "\nCurrent proof: " << _pf << "\n";
            throw std::runtime_error(
                "Malformed proof in proof check: Rule "
                "application has wrong number of args");
          }

          std::list<std::pair<ASTNode, ASTNode>> subs;
          auto fvs = rule.free_variables;
          if (!_goal.is_of_form(rule.consequence, fvs, subs)) {
            std::cerr << "Current goal: " << _goal
                      << "\nCurrent proof: " << _pf;
            throw std::runtime_error(
                "Malformed proof in proof check: Goal is not "
                "of proper form for rule application");
          }

          std::list<std::pair<ASTNode, ASTNode>> to_check;
          for (size_t i = 0; i < rule.requirements.size();
               ++i) {
            if (arg_pfs.at(i).at(0).is_of_form(
                    rule.requirements.at(i), fvs, subs)) {
              const auto arg_goal =
                  rule.requirements.at(i).replace(subs);
              if (debug) {
                std::cout << "Pattern-matched rule " << rule
                          << " against goal " << _goal
                          << " to get new goal " << i
                          << " w/ subs\n";
                for (const auto &p : subs) {
                  std::cout << p.first << " := " << p.second
                            << '\n';
                }
                std::cout << rule.requirements.at(i)
                          << " became " << arg_goal << '\n';
              }
              to_check.push_back({arg_goal, arg_pfs.at(i)});
            } else {
              std::cerr << "Current goal: " << _goal
                        << "\nCurrent proof: " << _pf;
              throw std::runtime_error(
                  "Malformed proof in proof check: Failed "
                  "argument pattern match");
            }
          }

          // Only now do we have all the substitutions
          for (const auto &p : to_check) {
            check_subproof(p.first.replace(subs), p.second);
          }
        }

        else {
          std::cerr << "Current goal: " << _goal
                    << "\nCurrent proof: " << _pf << "\n";
          throw std::runtime_error(
              "Malformed proof in proof check: Expected "
              "'ASSUMPTION' or 'APPLICATION', but saw '" +
              pf.text.text + "'");
        }
      };

  if (debug) {
    std::cout << "Checking pf " << _thm << '\n';
  }
  if (!_thm.proof.has_value()) {
    const auto facts = get_all_facts();
    bool saw = false;
    for (const auto &fact : facts) {
      if (fact.second == _thm) {
        saw = true;
        break;
      }
    }
    if (!saw) {
      throw std::runtime_error(
          "Cannot check proof for unknown axiom!");
    }
    return;
  }
  check_subproof(_thm.consequence, _thm.proof.value());
}
