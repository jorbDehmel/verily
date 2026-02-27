// Lexes and parses verily.

#include "parse.hpp"
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

bool Token::operator==(const Token &_other) const noexcept {
  return text == _other.text;
}

bool TokenStream::done() const noexcept {
  return pos >= data.size();
}

Token TokenStream::cur() const noexcept {
  if (pos >= data.size()) {
    return Token("EOF");
  }
  return data.at(pos);
}

void TokenStream::next() {
  ++pos;
}

Token TokenStream::cur_next() {
  const auto out = cur();
  next();
  return out;
}

// Assert that the current token is in 'what' and advance
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

TokenStream lex_text(const std::string &text,
                     const std::filesystem::path &fp) {
  std::vector<Token> out;

  int line = 1;
  int col = 0;
  std::string cur;
  bool in_comment = false;

  const auto add_tok = [&]() {
    if (!cur.empty()) {
      if (!in_comment) {
        out.push_back(Token(cur, fp, line, col - cur.size()));
      }
      cur.clear();
    }
  };

  bool in_string = false;
  for (const auto &c : text) {
    if (c == '"') {
      cur.push_back(c);
      if (in_string) {
        add_tok();
      }
      in_string = !in_string;
    }

    else if (in_string) {
      cur.push_back(c);
    }

    else if (std::set<char>({':', ';', '(', ')', '{', '}', '.',
                             ',', '[', ']', '\''})
                 .contains(c)) {
      add_tok();
      cur = c;
      add_tok();
    }

    else if (c == '#') {
      in_comment = true;
    } else if (c == '/' && cur == "/") {
      cur.clear();
      in_comment = true;
    }

    else if (c == '\n') {
      add_tok();
      ++line;
      col = 0;
      in_comment = false;
    } else if (c == ' ' || c == '\t') {
      add_tok();
    }

    else {
      cur += c;
    }

    ++col;
  }
  add_tok();
  return TokenStream(out);
}

TokenStream lex_file(const std::filesystem::path &fp) {
  std::ifstream f(fp);
  if (!f.is_open()) {
    throw std::runtime_error("Failed to open " + fp.string());
  }
  std::string line, text;
  while (!f.eof()) {
    std::getline(f, line);
    text += line + "\n\n";
  }
  return lex_text(text, fp);
}

ASTNode::ASTNode(const Token &_text,
                 const std::vector<ASTNode> &_children)
    : text(_text), children(_children) {
  if (text.file == "N/A") {
    for (const auto &child : children) {
      if (child.text.file != "N/A") {
        text.file = child.text.file;
        text.line = child.text.line;
        text.col = child.text.col;
        break;
      }
    }
  }
}

bool ASTNode::operator==(const ASTNode &_other) const noexcept {
  if (text != _other.text) {
    return false;
  }
  if (children.size() != _other.children.size()) {
    return false;
  }
  for (uint i = 0; i < children.size(); ++i) {
    if (children[i] != _other.children[i]) {
      return false;
    }
  }
  return true;
}

bool ASTNode::operator==(
    const std::string &_other) const noexcept {
  return text.text == _other;
}

std::strong_ordering
ASTNode::operator<=>(const ASTNode &_other) const noexcept {
  return (text.text <=> _other.text.text);
}

bool ASTNode::contains(const ASTNode &_what) const noexcept {
  if (operator==(_what)) {
    return true;
  } else if (_what.children.empty() && _what.text == text) {
    // Special case: Fn literals
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

ASTNode ASTNode::beta_star() const noexcept {
  if (text == "REPLACE") {
    // Apply beta reduction a single time, then recurse
    const auto A = children.at(0);
    const auto x = children.at(1);
    const auto B = children.at(2);
    return A.replace(x, B).beta_star();
  } else {
    // Apply beta star to all children and return
    ASTNode out(text);
    for (const auto &child : children) {
      out.children.push_back(child.beta_star());
    }
    return out;
  }
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

void fancy_print(std::ostream &_strm, const ASTNode &_node,
                 const uint &_depth) {
  if (_node.children.empty()) {
    _strm << _node.text.text;
  } else {
    _strm << "(" << _node.text.text << "\n\n";
    for (const auto &child : _node.children) {
      for (uint i = 0; i < _depth + 1; ++i) {
        _strm << ". ";
      }
      fancy_print(_strm, child, _depth + 1);
      _strm << "\n\n";
    }
    for (uint i = 0; i < _depth; ++i) {
      _strm << ". ";
    }
    _strm << ")";
  }
}

Parser::Parser(const TokenStream &_ts) : ts(_ts) {
  for (uint pos = 0; pos < ts.data.size(); ++pos) {
    const std::string t = ts.data.at(pos).text;
    if (t == "!") {
      ts.data[pos].text = "not";
    } else if (t == "&&") {
      ts.data[pos].text = "and";
    } else if (t == "||") {
      ts.data[pos].text = "or";
    } else if (t == "symbol") {
      ts.data[pos].text = "bind";
    }
  }
}

// Parses a single statement
ASTNode Parser::parse_statement() {
  const Token tok = ts.cur_next();
  const std::string t = tok.text;

  if (t == ";") {
    return ASTNode("NULL");
  }

  else if (t == "function") {
    return parse_function();
  } else if (t == "method") {
    return parse_method();
  } else if (t == "include") {
    const auto written = ts.cur_next().text;
    return ASTNode(
        Token("INCLUDE"),
        {Token(written.substr(1, written.size() - 2))});
  }

  else if (t == "prove_forward") {
    if (ts.cur().text != ":") {
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_FORWARD"), {parse_expr()});
  } else if (t == "prove_backward") {
    if (ts.cur().text != ":") {
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_BACKWARD"), {parse_expr()});
  } else if (t == "prove_smt") {
    if (ts.cur().text != ":") {
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_SMT"), {parse_expr()});
  } else if (t == "theorem") {
    if (ts.cur().text != ":") {
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("THEOREM"), {parse_expr()});
  }

  else if (t == "axiom") {
    if (ts.cur().text != ":") {
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("AXIOM"), {parse_expr()});
  } else if (t == "rule") {
    std::string name = "NULL";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    ASTNode over_block(Token("OVER"));
    if (ts.cur().text == "over") {
      ts.next();
      while (!ts.done() && ts.cur().text != "given" &&
             ts.cur().text != "deduce") {
        over_block.children.push_back(parse_expr());
        while (ts.cur().text == ",") {
          ts.next();
        }
      }
    }

    ASTNode given_block(Token("GIVEN"));
    if (ts.cur().text == "given") {
      ts.next();
      while (!ts.done() && ts.cur().text != "deduce") {
        given_block.children.push_back(parse_expr());
        while (ts.cur().text == ",") {
          ts.next();
        }
      }
    }

    ts.expect({"deduce"});
    ASTNode deduce_block(Token("DEDUCE"), {parse_expr()});
    return ASTNode(
        Token("RULE"),
        {over_block, given_block, deduce_block, ASTNode(name)});
  } else {
    throw std::runtime_error(
        "Unexpected statement start token '" + t + "'");
  }
}

// Parses a global scope
ASTNode Parser::parse() {
  if (debug) {
    std::cout << "Parsing from token stream:\n";
    for (const auto &tok : ts.data) {
      std::cout << tok.text << ' ';
    }
    std::cout << '\n';
  }

  ASTNode out(Token("GLOBAL"));
  while (!ts.done()) {
    const ASTNode cur = parse_statement();
    if (cur.text != "NULL") {
      out.children.push_back(cur);
      if (debug) {
        std::cout << "Parsed: " << out.children.back()
                  << "\n\n";
      }
    }
  }
  return out;
}

// Parses a set / type composed of sets
ASTNode Parser::parse_type() {
  ASTNode cur = ASTNode(ts.cur_next());
  if (cur.text == "(") {
    cur = parse_type();
    ts.expect({")"});
  }

  if (ts.cur().text == "to") {
    ts.next();
    cur = ASTNode(Token("TO"), {cur, parse_type()});
  } else if (ts.cur().text == "cross") {
    ts.next();
    cur = ASTNode(Token("CROSS"), {cur, parse_type()});
  }
  return cur;
}

ASTNode Parser::parse_args() {
  ts.expect({"("});
  ASTNode args(Token("ARGS"));
  while (!ts.done() && ts.cur().text != ")") {
    const auto argname = ts.cur_next();
    ts.expect({"in", ":"});
    const auto domain = ts.cur_next();

    args.children.push_back(
        ASTNode(Token("ARG"), {argname, domain}));

    if (ts.cur().text == ",") {
      ts.next();
    }
  }
  ts.expect({")"});

  return args;
}

ASTNode Parser::parse_req_ens() {
  ASTNode reqs_and_ens(Token("REQS_AND_ENS"));
  while (!ts.done() && (ts.cur().text == "requires" ||
                        ts.cur().text == "ensures")) {
    const auto t = ts.cur_next();
    reqs_and_ens.children.push_back(ASTNode(t, {parse_expr()}));
  }
  return reqs_and_ens;
}

// Parses a (functional) function definition
ASTNode Parser::parse_function() {
  const auto name = ts.cur_next();
  const auto args = parse_args();
  const auto reqs_and_ens = parse_req_ens();

  ts.expect({"{"});
  const auto body = parse_expr();
  ts.expect({"}"});

  const auto out = ASTNode(Token("FUNCTION"),
                           {name, args, reqs_and_ens, body});

  return out;
}

// Parses an imperative method definition
ASTNode Parser::parse_method() {
  const std::function<std::optional<ASTNode>()>
      parse_statement = [&]() -> std::optional<ASTNode> {
    const auto cur = ts.cur_next();

    if (cur.text == "{") {
      ASTNode body(Token("SCOPE"));
      while (!ts.done() && ts.cur().text != "}") {
        const auto to_add = parse_statement();
        if (to_add.has_value()) {
          body.children.push_back(to_add.value());
        }
      }
      ts.expect({"}"});
      return body;
    }

    else if (cur.text == "annotation" ||
             cur.text == "theorem") {
      return ASTNode("THEOREM", {parse_expr()});
    }

    else if (cur.text == "let") {
      const auto name = ts.cur_next();
      ts.expect({"="});
      return ASTNode(Token("LET"), {name, parse_expr()});
    }

    else if (cur.text == "if") {
      ASTNode out(Token("IF"),
                  {parse_expr(), parse_statement().value()});
      if (ts.cur().text == "else") {
        ts.next();
        const auto to_add = parse_statement();
        if (to_add.has_value()) {
          out.children.push_back(to_add.value());
        }
      }
      return out;
    }

    else if (cur.text == "while") {
      return ASTNode(Token("WHILE"),
                     {parse_expr(), parse_statement().value()});
    }

    else if (cur.text == ";") {
      return {};
    }

    else {
      ts.expect({"="});
      return ASTNode(Token("SET"), {cur, parse_expr()});
    }
  };

  const auto name = ts.cur_next();
  const auto args = parse_args();

  ts.expect({"returns"});
  const auto returns = ts.cur_next();

  const auto reqs_and_ens = parse_req_ens();

  const auto body = parse_statement().value();

  ASTNode out(Token("METHOD"),
              {name, args, returns, reqs_and_ens, body});

  return out;
}

// Parses an expression in colinear time WRT number of
// operations, number of tokens in expression. This is a
// little baby push-reduce + linear recursive descent
// parser all of its own.
ASTNode Parser::parse_expr() {
  const static std::set<std::string> expression_terminators = {
      ",",      ";", "requires", "ensures", "given",
      "deduce", "{", "}",        "=",       "]"};
  const static std::set<std::string> keywords = {
      "not", "and", "or", "implies", "iff"};

  std::list<ASTNode> items;
  try {
    while (!ts.done() &&
           !expression_terminators.contains(ts.cur().text)) {
      if (ts.cur().text == ")") {
        break;
      }

      const auto cur = ts.cur_next();

      if (cur.text == "(") {
        if (!items.empty() &&
            !keywords.contains(items.back().text.text)) {
          // Parse call
          if (items.empty()) {
            throw std::runtime_error("Malformed expression");
          }

          ASTNode call(items.back());
          items.pop_back();
          while (!ts.done() && ts.cur().text != ")") {
            call.children.push_back(parse_expr());
            if (ts.cur().text == ",") {
              ts.next();
            }
          }
          ts.expect({")"});
          items.push_back(call);
        } else {
          // Parse parentheses
          items.push_back(parse_expr());
          ts.expect({")"});
        }
      } else if (cur.text == "[") {
        // Replacement notation
        // A[x = B] means "replace x in A with B"
        if (items.empty()) {
          throw std::runtime_error(
              "Malformed expression: replacement operator '[' "
              "must act upon an expression");
        }

        // Pop A
        const auto A = items.back();
        items.pop_back();

        // Parse x
        const auto x = parse_expr();

        ts.expect({"="});

        // Parse B
        const auto B = parse_expr();
        ts.expect({"]"});

        // Push replacement expression
        return ASTNode("REPLACE", {A, x, B});
      }

      // Non-parentheses case
      else {
        // Replacements
        if (cur == ":") {
          // Within an expression, ':' is shorthand for 'in'.
          items.push_back(ASTNode("in"));
        }

        // Normal non-replaced case
        else {
          items.push_back(cur);
        }
      }
    }
    if (ts.done()) {
      throw std::runtime_error("EOF during expression");
    }

    return parse_expr_from_list(items);
  } catch (std::runtime_error &e) {
    std::stringstream err_msg;
    err_msg << "In [";
    bool first = true;
    for (const auto &item : items) {
      if (first) {
        first = false;
      } else {
        err_msg << ' ';
      }
      err_msg << item;
    }
    err_msg << "]:\n" << e.what();
    throw std::runtime_error(err_msg.str());
  }
}

ASTNode Parser::parse_expr_from_list(
    const std::list<ASTNode> &input_items) {
  const static std::list<std::string> order_of_operations = {
      "'",   "*",  "/",   "%",   "+",       "-",
      "in",  "<",  ">",   "<=",  ">=",      "==",
      "not", "or", "and", "iff", "implies",
  };

  std::list<ASTNode> items = input_items;

  if (debug) {
    std::cout << __FILE__ << ":" << __LINE__ << ":"
              << __FUNCTION__ << ">";
    for (const auto &i : items) {
      std::cout << ' ' << i;
    }
    std::cout << "\n\n";
  }

  if (items.empty()) {
    throw std::runtime_error("Expressions must not be empty");
  }

  // Do non-quant operators
  for (const auto &op : order_of_operations) {
    std::list<ASTNode> next_items;

    // Unary prefix operations (just not, for now)
    if (op == "not") {
      for (auto rit = items.rbegin(); rit != items.rend();
           ++rit) {
        if (rit->text == "not" && rit->children.empty()) {
          if (next_items.empty()) {
            throw std::runtime_error(
                "Malformed expression: 'not' does not act on "
                "anything");
          }
          next_items.front() =
              ASTNode("not", {next_items.front()});
        } else {
          next_items.push_front(*rit);
        }
      }
    }

    // Unary suffix: Just prime, for now
    else if (op == "'") {
      for (const auto &item : items) {
        if (item == "'") {
          if (next_items.empty()) {
            throw std::runtime_error(
                "Malformed expression: 'prime' does not act on "
                "anything");
          }
          const auto upon = next_items.back();
          next_items.pop_back();
          next_items.push_back(ASTNode("prime", {upon}));
        } else {
          next_items.push_back(item);
        }
      }
    }

    // Binary operations
    else {
      bool pending = false;
      for (const auto &item : items) {
        if (pending) {
          const auto popped = next_items.back();
          next_items.pop_back();
          next_items.push_back(
              ASTNode(Token(op), {popped, item}));
          pending = false;
        } else if (item == op && item.children.empty()) {
          if (next_items.empty()) {
            throw std::runtime_error(
                "Malformed expression: " + op + " has no LHS");
          }
          pending = true;
        } else {
          next_items.push_back(item);
        }
      }
      if (pending) {
        throw std::runtime_error("Malformed expression: " + op +
                                 " has no RHS");
      }
    }
    items = next_items;

    if (debug) {
      std::cout << "After " << op << ": [";
      for (const auto &item : items) {
        std::cout << item << ' ';
      }
      std::cout << "]\n";
    }
  }

  // Quantification
  std::list<ASTNode> output_items;

  // Iterate BACKWARDS
  // When we see ".", the current HEAD is the body. 0 -> 1
  // When we are in 1, the current token is x. push and 1 -> 2
  // When we are in 2, the current token is quant. push and -> 0
  uint state = 0;
  for (auto rit = items.rbegin(); rit != items.rend(); ++rit) {
    const auto item = *rit;
    switch (state) {
    default: {
      if (item == ".") {
        // Cur head is body
        state = 1;
      } else {
        output_items.push_front(item);
      }
    } break;
    case 1: {
      // Push var to head
      output_items.push_front(item);
      state = 2;
    } break;
    case 2: {
      const auto quant = item;
      if (!quant.children.empty()) {
        throw std::runtime_error(
            "Illegal non-atomic quantifier");
      }

      const auto var = output_items.front();
      output_items.pop_front();
      const auto body = output_items.front();
      output_items.pop_front();

      // Push quantified body to front
      output_items.push_front(ASTNode(quant.text, {var, body}));
      state = 0;
    } break;
    }
  }
  if (state != 0) {
    throw std::runtime_error("Malformed quantifier");
  }

  if (output_items.size() != 1) {
    throw std::runtime_error(
        "Malformed expression: Failed to produce single tree. "
        "Instead, " +
        std::to_string(output_items.size()));
  }
  return *output_items.begin();
}
