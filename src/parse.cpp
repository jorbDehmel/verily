#include "parse.hpp"
#include "ast.hpp"
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
                             '[', ']', '\'', '^', ':'})
                 .contains(c)) {
      add_tok();
      cur = c;
      add_tok();
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

  if (debug) {
    std::cout << "parse_statement at " << t << '\n';
  }

  if (t == ";") {
    return ASTNode(Token(tok, "NULL"));
  } else if (t == "ls") {
    return ASTNode(Token(tok, "LS"));
  } else if (t == "quit") {
    return ASTNode(Token(tok, "QUIT"));
  } else if (t == "help") {
    return ASTNode(Token(tok, "HELP"));
  }

  else if (t == "{") {
    ASTNode out(Token(tok, "SCOPE"));
    while (!ts.done() && ts.cur().text != "}") {
      out.children.push_back(parse_statement());
    }
    ts.next();
    return out;
  }

  else if (t == "include" || t == "import") {
    const auto written = ts.cur_next().text;
    return ASTNode(
        Token(Token(tok, "INCLUDE")),
        {Token(written.substr(1, written.size() - 2))});
  } else if (t == "setting" || t == "option") {
    const auto written = ts.cur_next().text;
    return ASTNode(
        Token(Token(tok, "SETTING")),
        {Token(written.substr(1, written.size() - 2))});
  }

  else if (t == "notation") {
    const auto name = ts.cur_next();
    ts.expect({":"});
    const auto fixedness =
        ts.expect({"prefix", "infix", "postfix"});
    const auto tok = ts.cur_next();
    ASTNode before("BEFORE");
    if (ts.cur() == "before") {
      ts.next();
      before.children.push_back(ts.cur_next());
    }
    return ASTNode(
        "NOTATION",
        {name, fixedness, tok, before,
         (fixedness == "infix" ? Token("2") : Token("1"))});
  }

  else if (t == "axiom") {
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
        Token(Token(tok, "AXIOM")),
        {over_block, given_block, deduce_block, ASTNode(name)});
  }

  else if (t == "theorem") {
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
    const ASTNode deduce_block(Token("DEDUCE"), {parse_expr()});

    ts.expect({"by"});
    const auto proof = parse_proof();

    return ASTNode(Token(Token(tok, "THEOREM")),
                   {over_block, given_block, deduce_block,
                    proof, ASTNode(name)});
  }

  else {
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

const static std::set<std::string> expression_terminators = {
    ",", ";", "given", "deduce", "{", "}", "by", "wts"};

ASTNode Parser::parse_expr() {
  if (debug) {
    std::cout << "parse_expr at " << ts.cur() << '\n';
  }

  std::list<ASTNode> items;
  try {
    while (!ts.done() &&
           !expression_terminators.contains(ts.cur().text)) {
      if (ts.cur().text == ")") {
        break;
      }

      const auto cur = ts.cur_next();

      if (cur.text == "(") {
        std::list<ASTNode> l;
        while (!ts.done() && ts.cur().text != ")") {
          l.push_back(parse_expr());
        }
        ts.expect({")"});
        items.push_back(parse_expr_from_list(l));
      }

      // Non-parentheses case
      else {
        items.push_back(cur);
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
  for (const auto &op_info : order_of_operations) {
    const auto op = op_info.tok;
    const auto label = op_info.label;
    const auto arity = op_info.arity;
    const auto fixedness = op_info.mode;

    std::list<ASTNode> next_items;

    // Unary prefix operations
    if (fixedness == OperationInfo::PREFIX) {
      if (arity != 1) {
        throw std::runtime_error(
            "Non-unary prefix operators are not yet supported "
            "(offender: '" +
            label + "')");
      }

      for (auto rit = items.rbegin(); rit != items.rend();
           ++rit) {
        if (rit->text == op && rit->children.empty()) {
          if (next_items.empty()) {
            throw std::runtime_error("Malformed expression: '" +
                                     label +
                                     "' does not act on "
                                     "anything");
          }
          next_items.front() =
              ASTNode(label, {next_items.front()});
        } else {
          next_items.push_front(*rit);
        }
      }
    }

    // Unary suffix
    else if (fixedness == OperationInfo::POSTFIX) {
      if (arity != 1) {
        throw std::runtime_error(
            "Non-unary postfix operators are not yet supported "
            "(offender: '" +
            label + "')");
      }

      for (const auto &item : items) {
        if (item == op) {
          if (next_items.empty()) {
            throw std::runtime_error("Malformed expression: '" +
                                     label +
                                     "' does not act on "
                                     "anything");
          }
          const auto upon = next_items.back();
          next_items.pop_back();
          next_items.push_back(ASTNode(label, {upon}));
        } else {
          next_items.push_back(item);
        }
      }
    }

    // Binary operations
    else {
      if (arity != 2) {
        throw std::runtime_error(
            "Non-binary infix operators are not yet supported "
            "(offender: '" +
            label + "')");
      }

      bool pending = false;
      for (const auto &item : items) {
        if (pending) {
          const auto popped = next_items.back();
          next_items.pop_back();
          next_items.push_back(
              ASTNode(Token(label), {popped, item}));
          pending = false;
        } else if (item == op && item.children.empty()) {
          if (next_items.empty()) {
            throw std::runtime_error("Malformed expression: " +
                                     label + " has no LHS");
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
      std::cout << "After " << label << ": [";
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
    std::vector<ASTNode> items;
    items.assign(output_items.begin(), output_items.end());
    return ASTNode("_", items);
  }
  return *output_items.begin();
}

ASTNode Parser::parse_proof() {
  ASTNode pf("PROOF");
  ts.expect({"{"});
  while (!ts.done() && ts.cur() != "}") {
    const auto tok = ts.cur_next();
    const auto t = tok.text;

    if (t == "forward" || t == "fw") {
      const auto name = ts.cur_next();

      ASTNode on("ON");
      ASTNode as("AS");
      if (ts.cur() == "on") {
        ts.next();
        do {
          while (!ts.done() && ts.cur() == ",") {
            ts.next();
          }
          on.children.push_back(ts.cur_next());
        } while (!ts.done() && ts.cur() == ",");
      }
      if (ts.cur() == "as") {
        ts.expect({"as"});
        as.children.push_back(ts.cur_next());
      }

      pf.children.push_back(
          ASTNode(Token(tok, "FORWARD"), {name, on, as}));
    } else if (t == "backward" || t == "bw") {
      const auto name = ts.cur_next();
      pf.children.push_back(
          ASTNode(Token(tok, "BACKWARD"), {name}));
    } else if (t == "wts") {
      const auto goal = parse_expr();
      const auto proof = parse_proof();
      pf.children.push_back(
          ASTNode(Token(tok, "WTS"), {goal, proof}));
    }

    else if (t == "ls") {
      pf.children.push_back(ASTNode(Token(tok, "LS")));
    }

    else {
      throw std::runtime_error(
          "Invalid proof statement start token '" + t + "'");
    }
  }
  ts.expect({"}"});
  return pf;
}
