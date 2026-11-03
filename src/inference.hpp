/**
 * @brief Stack management, mostly for goals / tactics.
 */

#pragma once

#include "ast.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

/// A maker of inferences. It takes rules and axioms and deduces
/// theorems
class InferenceMaker {
public:
  using Theorem = InferenceRule;

  /// Keeps all the info
  struct Frame {
    /// Generics
    std::set<ASTNode> generics;

    /// Statements which are known to be true
    std::map<std::string, Theorem> theorems;

    /// Statements currently being proven
    std::list<ASTNode> goals;

    /// (goal, pf): Goals which have already been proven
    std::list<std::pair<ASTNode, ASTNode>> closed_goals;
  };

  /// Apply, but backwards.
  void backward_apply(const std::string &_rule_name,
                      const InferenceRule &_rule,
                      const ASTNode &_goal);

  /// True iff we have already proven _what
  bool has(const ASTNode &_what) const noexcept;

  /// Get the name of the proof, throwing if it doesn't exist
  std::string get_name(const ASTNode &_what) const;

  /// Adds an axiomatic (EG not generic, no antecedents) AST
  void add_assumption(const std::string &_name,
                      const ASTNode &_what) noexcept;

  /// Gets a theorem by name
  const Theorem get_theorem(const std::string &_name) const;

  /// Gets a theorem by theorem
  const Theorem get_theorem(const ASTNode &_thm) const;

  /// Get all facts from all existing frames
  std::list<std::pair<std::string, Theorem>>
  get_all_facts() const noexcept;

  /// Adds a theorem
  const Theorem add_theorem(const std::string &_name,
                            const Theorem &_thm);

  /// Turns a 'by' block into an actual theorem
  std::optional<Theorem>
  construct_proof(const ASTNode &_goal, const ASTNode &_proof,
                  const std::string &_name);

  /// Throws an error if the given theorem is bunk
  void check_proof(const Theorem &_thm);

  /// List everything
  void ls() const noexcept;

  /// True iff there exists a theorem or rule with the given
  /// name
  bool name_is_used(const std::string &_name) const noexcept;

  /// Return _prefix + to_string(i), where i is the smallest
  /// integer where the result is unique
  std::string
  fresh_with_prefix(const std::string &_prefix) const noexcept;

  /// If _name is NULL, return a fresh name with the given
  /// prefix. Otherwise, just return _name.
  std::string
  fix_name(const std::string &_name,
           const std::string &_prefix) const noexcept;

  /// Push a frame
  inline void
  push(const std::set<ASTNode> &_generics = {}) noexcept {
    frames.push_back(Frame{.generics = _generics});
  }

  /// Gets all the assumptions in the BACK frame ONLY!
  inline std::vector<ASTNode> assumptions() const {
    std::vector<ASTNode> out;
    for (const auto &thm : frames.back().theorems) {
      if (!thm.second.proof.has_value()) {
        if (!thm.second.requirements.empty()) {
          throw std::runtime_error(
              "Assumption has non-empty antecedent: How did "
              "you even do that?");
        }
        out.push_back(thm.second.consequence);
      }
    }
    return out;
  }

  /// Pop the most recent frame (if there is one)
  inline void pop() {
    if (frames.size() > 1) {
      const auto popped = frames.back();
      frames.pop_back();

      std::vector<ASTNode> frame_assumptions;
      for (const auto &thm : popped.theorems) {
        if (!thm.second.proof.has_value()) {
          if (!thm.second.requirements.empty()) {
            throw std::runtime_error(
                "Assumption has non-empty antecedent: How did "
                "you even do that?");
          }
          frame_assumptions.push_back(thm.second.consequence);
        }
      }
      if (!popped.goals.empty()) {
        std::cerr << "Unresolved goal(s) at end of frame: ";
        bool first = true;
        for (const auto &g : popped.goals) {
          if (first) {
            first = false;
          } else {
            std::cout << ", ";
          }
          std::cout << g;
        }
        std::cerr << '\n';
        throw std::runtime_error(
            "Unresolved goal(s) at end of frame");
      }

      // For fixing backwards proofs
      std::list<std::pair<ASTNode, ASTNode>> subs;
      for (const auto &p : popped.theorems) {
        subs.push_back(
            {ASTNode("THEOREM",
                     {
                         p.second.consequence,
                         ASTNode("REPLACE_ME"),
                     }),
             ASTNode("THEOREM", {
                                    p.second.consequence,
                                    p.second.proof.value_or(
                                        ASTNode("ASSUMPTION")),
                                })});
      }

      for (const auto &goal : popped.closed_goals) {
        ASTNode pf = goal.second;
        ASTNode tmp = pf.replace(subs);
        while (pf != tmp) {
          pf = tmp;
          tmp = pf.replace(subs);
        }
        add_theorem(fresh_with_prefix("t"),
                    Theorem(popped.generics, frame_assumptions,
                            goal.first, pf));
      }
    } else {
      throw std::runtime_error(
          "Attempted to pop global frame!");
    }
  }

  /// Add a goal
  inline void open_goal(const ASTNode &_goal) {
    if (!has(_goal)) {
      if (debug) {
        std::cout << "Adding goal " << _goal << '\n';
      }
      frames.back().goals.push_front(_goal);
    }
  }

  /// Remove a goal (if it was present)
  inline void close_goal(const ASTNode &_goal) {
    if (debug) {
      std::cout << "Dropping goal " << _goal << '\n';
    }
    std::erase_if(
        frames.back().goals, [&](const ASTNode &_item) -> bool {
          if (_item == _goal) {
            frames.back().closed_goals.push_back(
                {_item, get_theorem(_item).proof.value()});
            return true;
          }
          return false;
        });
  }

  /// Where all the data is stored
  std::list<Frame> frames = {Frame()};

  /// If true, prints some extra info
  bool debug = false;
};

/// Print a theorem
std::ostream &operator<<(std::ostream &,
                         const InferenceMaker::Theorem &);
