/**
 * @brief Abstract syntax trees, tokens, and token streams. This
 * is MIT-licensed copyware. Jordan Dehmel, 2026.
 */

#include "ast.hpp"
#include <algorithm>
#include <iostream>

Token TokenStream::expect(std::set<std::string> what) {
  const auto cur_tok = cur();
  if (!what.contains(cur_tok.text)) {
    std::stringstream what_ss;
    what_ss << "{";
    bool first = true;
    for (const auto &s : what) {
      if (first) {
        first = false;
      } else {
        what_ss << ", ";
      }
      what_ss << '"' << s << '"';
    }
    what_ss << "}";

    throw std::runtime_error(
        "Expected " + what_ss.str() + ", but saw " +
        cur_tok.text + " at " + cur_tok.file.string() + ":" +
        std::to_string(cur_tok.line) + "." +
        std::to_string(cur_tok.col));
  }
  next();
  return cur_tok;
}

bool ASTNode::operator==(const ASTNode &_other) const noexcept {
  if (text != _other.text ||
      children.size() != _other.children.size()) {
    return false;
  }
  for (uint i = 0; i < children.size(); ++i) {
    if (children[i] != _other.children[i]) {
      return false;
    }
  }
  return true;
}

bool ASTNode::contains(const ASTNode &_what) const noexcept {
  if (*this == _what) {
    return true;
  }
  for (const auto &child : children) {
    if (child.contains(_what)) {
      return true;
    }
  }
  return false;
}

bool ASTNode::contains(
    const std::string &_what) const noexcept {
  if (text == _what) {
    return true;
  }
  for (const auto &child : children) {
    if (child.contains(_what)) {
      return true;
    }
  }
  return false;
}

ASTNode
ASTNode::replace(const ASTNode &_to_replace,
                 const ASTNode &_replace_with) const noexcept {
  if (*this == _to_replace) {
    return _replace_with;
  }
  ASTNode out(text);
  for (const auto &child : children) {
    out.children.push_back(
        child.replace(_to_replace, _replace_with));
  }
  return out;
}

ASTNode ASTNode::replace(
    const std::list<std::pair<ASTNode, ASTNode>> &_replacements)
    const noexcept {
  for (const auto &p : _replacements) {
    if (*this == p.first) {
      return p.second;
    }
  }
  ASTNode out(text);
  for (const auto &child : children) {
    out.children.push_back(child.replace(_replacements));
  }
  return out;
}

std::ostream &operator<<(std::ostream &_strm,
                         const ASTNode &_node) {
  if (_node.children.empty()) {
    _strm << _node.text.text;
  } else {
    _strm << "(" << _node.text.text;
    for (const auto &child : _node.children) {
      _strm << " " << child;
    }
    _strm << ")";
  }
  return _strm;
}

bool ASTNode::is_of_form(const ASTNode &_form,
                         std::set<ASTNode> &_free_variables,
                         std::list<std::pair<ASTNode, ASTNode>>
                             &_substitutions) const {
  for (const auto &p : _substitutions) {
    if (p.first == _form) {
      return *this == p.second;
    }
  }
  if (_free_variables.contains(_form)) {
    _substitutions.push_back({_form, copy()});
    _free_variables.erase(_form);
    return true;
  }
  if (text != _form.text ||
      children.size() != _form.children.size()) {
    return false;
  }
  for (uint child = 0; child < children.size(); ++child) {
    if (!children.at(child).is_of_form(_form.children.at(child),
                                       _free_variables,
                                       _substitutions)) {
      return false;
    }
  }
  return true;
}

std::optional<ASTNode> InferenceRule::try_apply(
    const std::vector<ASTNode> &_premises) const noexcept {
  if (_premises.size() != requirements.size()) {
    return {};
  }

  ASTNode cons = consequence;
  std::vector<ASTNode> reqs = requirements;

  // Rename so that no free variables occur as literals in any
  // premise
  std::set<ASTNode> fvs;
  for (const auto &fv : free_variables) {
    ASTNode to_add = fv;
    // This must halt, since an AST can only contain finitely
    // many subtrees
    while (std::any_of(_premises.begin(), _premises.end(),
                       [&](const ASTNode &prem) -> bool {
                         return prem.contains(to_add);
                       })) {
      const auto old = to_add;
      to_add = ASTNode("prime", {old});
      for (size_t i = 0; i < reqs.size(); ++i) {
        reqs[i] = reqs[i].replace(old, to_add);
      }
      cons = cons.replace(old, to_add);
    }
    fvs.insert(to_add);
  }

  // Do actual application
  std::list<std::pair<ASTNode, ASTNode>> substitutions;
  for (size_t i = 0; i < _premises.size(); ++i) {
    const auto corresponding_requirement = reqs.at(i);
    const auto thm = _premises.at(i);
    if (!thm.is_of_form(
            corresponding_requirement.replace(substitutions),
            fvs, substitutions)) {
      return {};
    }
  }
  return cons.replace(substitutions);
}

ASTNode InferenceRule::apply(
    const std::vector<ASTNode> &_premises) const {
  const auto res = try_apply(_premises);
  if (!res.has_value()) {
    std::cerr << "Failed to apply " << *this << " to premises";
    for (const auto &p : _premises) {
      std::cerr << ' ' << p;
    }
    std::cerr << '\n';
    throw std::runtime_error("Malformed rule application");
  }
  return res.value();
}

ASTNode InferenceRule::apply_subexpr(
    const ASTNode &_upon, bool &_did_something) const noexcept {
  if (_did_something) {
    return _upon;
  }

  const auto width = requirements.size();
  const auto full_match_res = try_apply({_upon});
  if (full_match_res.has_value()) {
    _did_something = true;
    return full_match_res.value();
  }

  ASTNode out(_upon.text);

  if (_upon.children.size() >= width) {
    // Scan window over children (backwards)
    for (int start = _upon.children.size() - width; start >= 0;
         --start) {
      std::vector<ASTNode> window;
      for (size_t i = start; i < start + width; ++i) {
        window.push_back(_upon.children.at(i));
      }

      // If we can match any window, do replacement
      const auto res = try_apply(window);
      if (res.has_value()) {
        // Reconstruct and return early
        _did_something = true;

        // Before
        for (int i = 0; i < start; ++i) {
          out.children.push_back(_upon.children.at(i));
        }

        // During
        out.children.push_back(res.value());

        // After
        for (size_t i = start + width;
             i < _upon.children.size(); ++i) {
          out.children.push_back(_upon.children.at(i));
        }

        return out;
      }
    }
  }

  // Else, recurse on children
  for (const auto &child : _upon.children) {
    out.children.push_back(
        apply_subexpr(child, _did_something));
  }
  return out;
}
