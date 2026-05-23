#include "parse.hpp"
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>

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

    // Singletons
    else if (std::set<char>({';', '(', ')', '{', '}', '.', ',',
                             '[', ']', '\'', '^'})
                 .contains(c)) {
      add_tok();
      cur = c;
      add_tok();
    }

    // typing or member access
    else if (c == ':') {
      if (cur == ":") {
        cur.push_back(c);
        add_tok();
      } else {
        add_tok();
        cur = c;
      }
    }

    else if (c == '#') {
      in_comment = true;
    } else if ((c == '/' && cur == "/") ||
               (c == '-' && cur == "-")) {
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
      if (cur == ":") {
        add_tok();
      }

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
    text += line + "\n";
  }
  return lex_text(text, fp);
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
  } else if (t == "ls") {
    return ASTNode("LS");
  } else if (t == "quit") {
    return ASTNode("QUIT");
  } else if (t == "help") {
    return ASTNode("HELP");
  }

  else if (t == "{") {
    ASTNode out("SCOPE");
    while (!ts.done() && ts.cur().text != "}") {
      out.children.push_back(parse_statement());
    }
    ts.next();
    return out;
  }

  else if (t == "wts") {
    return ASTNode(Token("WTS"), {parse_expr()});
  }

  else if (t == "include" || t == "import") {
    const auto written = ts.cur_next().text;
    return ASTNode(
        Token("INCLUDE"),
        {Token(written.substr(1, written.size() - 2))});
  } else if (t == "setting" || t == "option") {
    const auto written = ts.cur_next().text;
    return ASTNode(
        Token("SETTING"),
        {Token(written.substr(1, written.size() - 2))});
  }

  else if (t == "prove_forward") {
    std::string name = "";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_FORWARD"),
                   {ASTNode(name), parse_expr()});
  } else if (t == "prove_backward") {
    std::string name = "";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_BACKWARD"),
                   {ASTNode(name), parse_expr()});
  } else if (t == "prove_smt") {
    std::string name = "";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("PROVE_SMT"),
                   {ASTNode(name), parse_expr()});
  } else if (t == "theorem" || t == "lemma" || t == "deduce" ||
             t == "prove" || t == "assert") {
    std::string name = "";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("THEOREM"),
                   {ASTNode(name), parse_expr()});
  }

  else if (t == "apply") {
    // apply X;
    // apply X to Y, Z, A, B;
    const auto rule_name = ts.cur_next();
    ASTNode arguments("_");
    if (ts.cur().text == "to") {
      ts.next();
      arguments.children.push_back(ts.cur_next());
      while (ts.cur().text == ",") {
        ts.next();
        arguments.children.push_back(ts.cur_next());
      }
    }
    std::string result_name = "";
    if (ts.cur().text == "as") {
      ts.next();
      result_name = ts.cur_next().text;
    }
    return ASTNode(Token("APPLY"), {rule_name, arguments,
                                    ASTNode(result_name)});
  }

  else if (t == "axiom" || t == "assume") {
    std::string name = "";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    ts.expect({":"});
    return ASTNode(Token("AXIOM"),
                   {ASTNode(name), parse_expr()});
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
  } else if (t == "method") {
    return parse_method();
  } else {
    throw std::runtime_error(
        "At " + tok.file.string() + ":" +
        std::to_string(tok.line) + "." +
        std::to_string(tok.col) +
        "- Unexpected statement start token '" + t + "'");
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

const static std::initializer_list<std::string>
    order_of_operations = {
        "'",     "^",       "*",       "/",      "%",  "+",
        "-",     "<",       ">",       "<=",     ">=", "==",
        "cross", "to",      "in",      "not",    "or", "and",
        "iff",   "implies", "derives", "models",
};
const static std::set<std::string> keywords =
    order_of_operations;

const static std::set<std::string> expression_terminators = {
    ",",      ";", "requires", "ensures", "given", "returns",
    "deduce", "{", "}",        "=",       "]"};

ASTNode Parser::parse_expr() {
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

          ASTNode call("@", {items.back()});
          items.pop_back();
          while (!ts.done() && ts.cur().text != ")") {
            call.children.push_back(parse_expr());
            while (ts.cur().text == ",") {
              ts.next();
            }
          }
          ts.expect({")"});
          items.push_back(call);
        } else {
          // Special case: Unit tuple ()
          if (ts.cur().text == ")") {
            items.push_back(ASTNode("_", {}));
            ts.next();
          } else {
            // Parse parentheses
            items.push_back(parse_expr());

            if (ts.cur().text == ",") {
              // Actually a tuple / list
              ASTNode tuple = ASTNode("_", {items.back()});
              items.pop_back();

              // Parse any number of statements, then close
              while (ts.cur().text != ")") {
                while (ts.cur().text == ",") {
                  ts.next();
                }
                if (ts.cur().text == ")") {
                  break;
                }

                tuple.children.push_back(parse_expr());
              }

              // Append tuple to items
              items.push_back(tuple);
            }

            ts.expect({")"});
          }
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
        items.push_back(ASTNode("REPLACE", {A, x, B}));
      } else if (cur.text == "::") {
        // Member access
        if (items.empty()) {
          throw std::runtime_error(
              "Malformed expression: access operator '::' "
              "must act upon an expression");
        }

        // Pop A
        const auto A = items.back();
        items.pop_back();

        // Get B (NOT parsing an expression)
        const auto B = ts.cur_next();

        // Push replacement expression
        items.push_back(ASTNode("::", {A, B}));
      }

      // Non-parentheses case
      else {
        if (cur == ":") {
          items.push_back(ASTNode("in"));
        } else {
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

      if (output_items.size() < 2) {
        throw std::runtime_error(
            "Quantifier has no body and/or variable");
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

ASTNode Parser::parse_method() {
  const auto name = ts.cur_next();
  ts.expect({"("});
  ASTNode args("_");
  while (ts.cur() != ")") {
    args.children.push_back(ts.cur_next());
    while (ts.cur() == ",") {
      ts.next();
    }
  }
  ts.expect({")"});

  ts.expect({"requires"});
  const auto requirement = parse_expr();
  ts.expect({"ensures"});
  const auto ensurement = parse_expr();
  ts.expect({"returns"});
  const auto return_val = ts.cur_next();

  const auto body = parse_method_statement();

  return ASTNode("METHOD", {name, args, requirement, ensurement,
                            return_val, body});
}

ASTNode Parser::parse_method_statement() {
  const auto t = ts.cur_next();
  if (t == "{") {
    ASTNode out("_");
    while (ts.cur() != "}") {
      out.children.push_back(parse_method_statement());
    }
    ts.expect({"}"});
    return out;
  } else if (t == "if") {
    ASTNode out("ITE");
    out.children.push_back(parse_expr());
    out.children.push_back(parse_method_statement());
    ts.expect({"else"});
    out.children.push_back(parse_method_statement());
    return out;
  } else if (t == "while") {
    ASTNode out("WHILE");
    out.children.push_back(parse_expr());
    out.children.push_back(parse_method_statement());
    return out;
  } else if (t == ";") {
    return ASTNode("_");
  } else {
    ASTNode out("SET", {t});
    ts.expect({"="});
    out.children.push_back(parse_expr());
    return out;
  }
}
