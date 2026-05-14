/**
 * @brief Abstract syntax trees, tokens, and token streams. This
 * is MIT-licensed copyware. Jordan Dehmel, 2026.
 */

#include "ast.hpp"

void TokenStream::expect(std::set<std::string> what) {
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
}

bool ASTNode::operator==(const ASTNode &_other) const noexcept {
  if (text != _other.text) {
    return false;
  }
  if (children.size() != _other.children.size()) {
    return false;
  }
  for (uint i = 0; i < children.size(); ++i) {
    if (!children[i].operator==(_other.children[i])) {
      return false;
    }
  }
  return true;
}

bool ASTNode::contains(const ASTNode &_what) const noexcept {
  if (operator==(_what)) {
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
  if (operator==(_to_replace)) {
    return _replace_with;
  } else {
    ASTNode out(text);
    for (const auto &child : children) {
      out.children.push_back(
          child.replace(_to_replace, _replace_with));
    }
    return out;
  }
}

ASTNode ASTNode::replace(
    const std::list<std::pair<ASTNode, ASTNode>> &_replacements)
    const noexcept {
  for (const auto &p : _replacements) {
    if (operator==(p.first)) {
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
                         ASTSet &_free_variables,
                         std::list<std::pair<ASTNode, ASTNode>>
                             &_substitutions) const {
  for (const auto &p : _substitutions) {
    if (p.first == _form) {
      return operator==(p.second);
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
