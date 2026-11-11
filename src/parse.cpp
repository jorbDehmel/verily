#include "parse.hpp"
#include "ast.hpp"
#include <cassert>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <stdexcept>

// If there are two in a row, they should be two seperate tokens
const static std::set<char> singletons({'(', ')', '{', '}', '[',
                                        ']'});

// Encountering these from text (or vice versa) creates a new
// token
const static std::set<char> operator_chars({
    ';', ':', '\'', '.', ',', '^', '!', '`',
    '~', '@', '#',  '$', '%', '&', '*', '-',
    '+', '=', '\\', '|', '<', '>', '?', '/',
});

TokenStream lex_text(const std::string &text,
                     const std::filesystem::path &fp) {
  std::vector<Token> out;
  int line = 1;
  int col = 0;
  std::string cur;
  bool in_comment = false;
  bool in_string = false;
  bool in_operator = false;
  const auto add_tok = [&]() {
    if (!cur.empty()) {
      if (!in_comment) {
        out.push_back(Token(cur, fp, line, col - cur.size()));
      }
      cur.clear();
    }
  };
  for (const auto &c : text) {
    if (c == '"') {
      cur.push_back(c);
      if (in_string) {
        add_tok();
      }
      in_string = !in_string;
    } else if (in_string) {
      cur.push_back(c);
    }

    else if (c == '#') {
      in_comment = true;
    } else if ((c == '/' && cur == "/") ||
               (c == '-' && cur == "-")) {
      cur.clear();
      in_comment = true;
    } else if (c == '\n') {
      add_tok();
      ++line;
      col = 0;
      in_comment = false;
    } else if (c == ' ' || c == '\t') {
      add_tok();
    }

    else if (singletons.contains(c)) {
      in_operator = false;
      add_tok();
      cur = c;
      add_tok();
    }

    else if (operator_chars.contains(c)) {
      if (!in_operator) {
        in_operator = true;
        add_tok();
      }
      cur += c;
    } else {
      if (in_operator) {
        in_operator = false;
        add_tok();
      }
      cur += c;
    }

    ++col;
    if (col > 65) {
      throw std::runtime_error("Lines should not exceed 64 "
                               "characters (offender: line " +
                               std::to_string(line) + ")");
    }
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
  } else if (t == "{") {
    ASTNode out(Token(tok, "SCOPE"));
    while (!ts.done() && ts.cur().text != "}") {
      out.children.push_back(parse_statement());
    }
    ts.next();
    return out;
  } else if (t == "include" || t == "import") {
    const auto written = ts.cur_next();
    return ASTNode(
        Token(Token(tok, "INCLUDE")),
        {Token(written, written.text.substr(
                            1, written.text.size() - 2))});
  } else if (t == "setting" || t == "option") {
    const auto written = ts.cur_next();
    return ASTNode(
        Token(Token(tok, "SETTING")),
        {Token(written, written.text.substr(
                            1, written.text.size() - 2))});
  } else if (t == "notation") {
    std::string name = "NULL";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    auto pos = ts.expect({":"});
    ASTNode over_block(Token(pos, "OVER"));
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
    pos = ts.expect({"given"});
    const ASTNode given_block(Token(pos, "GIVEN"),
                              parse_expr().children);
    pos = ts.expect({"deduce"});
    const ASTNode deduce_block(Token(pos, "DEDUCE"),
                               {parse_expr()});

    ASTNode before(Token(pos, "BEFORE"));
    if (ts.cur().text == "before") {
      ts.next();
      before.children.push_back(ts.cur_next());
    }
    const auto out =
        ASTNode(Token(pos, "NOTATION"),
                {Token(pos, name), over_block, given_block,
                 deduce_block, before});
    return out;
  } else if (t == "axiom") {
    std::string name = "NULL";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    auto pos = ts.expect({":"});
    ASTNode over_block(Token(pos, "OVER"));
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
    ASTNode given_block(Token(pos, "GIVEN"));
    if (ts.cur().text == "given") {
      ts.next();
      while (!ts.done() && ts.cur().text != "deduce") {
        given_block.children.push_back(parse_expr());
        while (ts.cur().text == ",") {
          ts.next();
        }
      }
    }
    pos = ts.expect({"deduce"});
    ASTNode deduce_block(Token(pos, "DEDUCE"), {parse_expr()});
    return ASTNode(Token(Token(tok, "AXIOM")),
                   {over_block, given_block, deduce_block,
                    ASTNode(Token(pos, name))});
  } else if (t == "theorem") {
    std::string name = "NULL";
    if (ts.cur().text != ":") {
      name = ts.cur().text;
      ts.next();
    }
    auto pos = ts.expect({":"});
    ASTNode over_block(Token(pos, "OVER"));
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
    ASTNode given_block(Token(pos, "GIVEN"));
    if (ts.cur().text == "given") {
      ts.next();
      while (!ts.done() && ts.cur().text != "deduce") {
        given_block.children.push_back(parse_expr());
        while (ts.cur().text == ",") {
          ts.next();
        }
      }
    }
    pos = ts.expect({"deduce"});
    const ASTNode deduce_block(Token(pos, "DEDUCE"),
                               {parse_expr()});
    pos = ts.expect({"by"});
    const auto proof = parse_proof();
    return ASTNode(Token(Token(tok, "THEOREM")),
                   {over_block, given_block, deduce_block,
                    proof, ASTNode(Token(pos, name))});
  }

  else {
    throw std::runtime_error(
        "At " + tok.file.string() + ":" +
        std::to_string(tok.line) + "." +
        std::to_string(tok.col) +
        "- Unexpected statement start token '" + t + "'");
  }
}

ASTNode Parser::parse() {
  if (debug) {
    std::cout << "Parsing from token stream:\n";
    for (const auto &tok : ts.data) {
      std::cout << tok.text << ' ';
    }
    std::cout << '\n';
  }

  const auto pos = ts.cur();
  ASTNode out(Token(pos, "GLOBAL"));
  while (!ts.done()) {
    const ASTNode cur = parse_statement();
    if (cur.text.text != "NULL") {
      out.children.push_back(cur);
      if (debug) {
        std::cout << "Parsed: " << out.children.back()
                  << "\n\n";
      }
    }
  }
  return out;
}

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
    const std::list<ASTNode> &_input_items) {
  if (_input_items.empty()) {
    throw std::runtime_error(
        "Cannot parse expression from empty list!");
  }
  const auto pos = _input_items.front().text;

  std::vector<ASTNode> items;
  items.assign(_input_items.begin(), _input_items.end());

  if (debug) {
    std::cout << __FILE__ << ":" << __LINE__ << ":"
              << __FUNCTION__ << ">";
    for (const auto &i : items) {
      std::cout << ' ' << i;
    }
    std::cout << "\n\n";
  }

  for (auto rit = order_of_operations.begin();
       rit != order_of_operations.end(); ++rit) {
    const auto name = rit->first;
    const auto rule = rit->second;
    bool did_change = true;
    while (did_change) {
      did_change = false;
      ASTNode tmp(Token(pos, "_"), {items});
      const auto next = rule.apply_subexpr(tmp, did_change);
      if (did_change) {
        items.assign(next.children.begin(),
                     next.children.end());
      }
    }

    if (debug) {
      std::cout << "After " << name << ": [";
      for (const auto &item : items) {
        std::cout << item << ' ';
      }
      std::cout << "]\n";
    }
  }

  if (items.size() != 1) {
    ASTNode out(Token(pos, "_"));
    out.children.assign(items.begin(), items.end());
    return out;
  }
  return items.front();
}

ASTNode Parser::parse_proof() {
  const auto pos_tok = ts.cur();
  ASTNode pf(Token(pos_tok, "PROOF"));
  ts.expect({"{"});
  while (!ts.done() && ts.cur().text != "}") {
    const auto tok = ts.cur_next();
    const auto t = tok.text;
    if (t == "forward" || t == "fw") {
      const auto name = ts.cur_next();
      ASTNode on(Token(name, "ON"));
      ASTNode as(Token(name, "AS"));
      if (ts.cur().text == "on") {
        ts.next();
        do {
          while (!ts.done() && ts.cur().text == ",") {
            ts.next();
          }
          on.children.push_back(ts.cur_next());
        } while (!ts.done() && ts.cur().text == ",");
      }
      if (ts.cur().text == "as") {
        ts.expect({"as"});
        as.children.push_back(ts.cur_next());
      }
      if (ts.cur().text == "at") {
        ts.expect({"at"});
        as.children.push_back(ts.cur_next());
        on.children.push_back(as.children.front());
      }
      pf.children.push_back(
          ASTNode(Token(tok, "FORWARD"), {name, on, as}));
    } else if (t == "sweep") {
      const auto n = ts.cur_next();
      pf.children.push_back(ASTNode(Token(tok, "SWEEP"), {n}));
    } else if (t == "rewrite" || t == "rw") {
      const auto name = ts.cur_next();
      ASTNode on(Token(name, "ON"));
      ASTNode as(Token(name, "AS"));
      const auto choice = ts.expect({"on", "at"});
      on.children.push_back(ts.cur_next());
      if (choice.text == "on") {
        if (ts.cur().text == "as") {
          ts.expect({"as"});
          as.children.push_back(ts.cur_next());
        }
      } else {
        as.children.push_back(on.children.front());
      }
      pf.children.push_back(
          ASTNode(Token(tok, "REWRITE"), {name, on, as}));
    } else if (t == "backward" || t == "bw") {
      const auto name = ts.cur_next();
      pf.children.push_back(
          ASTNode(Token(tok, "BACKWARD"), {name}));
    } else if (t == "wts") {
      const auto goal = parse_expr();
      const auto proof = parse_proof();
      pf.children.push_back(
          ASTNode(Token(tok, "WTS"), {goal, proof}));
    } else if (t == "theorem" || t == "lemma") {
      std::string name = "NULL";
      if (ts.cur().text != ":") {
        name = ts.cur().text;
        ts.next();
      }
      auto pos = ts.expect({":"});
      ASTNode over_block(Token(pos, "OVER"));
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
      ASTNode given_block(Token(pos, "GIVEN"));
      if (ts.cur().text == "given") {
        ts.next();
        while (!ts.done() && ts.cur().text != "deduce") {
          given_block.children.push_back(parse_expr());
          while (ts.cur().text == ",") {
            ts.next();
          }
        }
      }
      pos = ts.expect({"deduce"});
      const ASTNode deduce_block(Token(pos, "DEDUCE"),
                                 {parse_expr()});
      pos = ts.expect({"by"});
      const auto proof = parse_proof();
      pf.children.push_back(
          ASTNode(Token(Token(tok, "THEOREM")),
                  {over_block, given_block, deduce_block, proof,
                   ASTNode(Token(pos, name))}));
    } else if (t == "have") {
      const auto thm = parse_expr();
      ts.expect({"as"});
      const auto name = ts.cur_next();
      pf.children.push_back(
          ASTNode(Token(tok, "HAVE"), {name, thm}));
    } else if (t == "use") {
      // use ... as ... on ... as ...
      ASTNode vk_pairs(Token(tok, "_"));
      while (!ts.done() && ts.cur().text != "on") {
        if (ts.cur().text == ",") {
          ts.next();
        }
        const auto value = parse_expr();
        ts.expect({"as"});
        const auto key = parse_expr();
      }
      ts.expect({"on"});
      const auto rule_name = ts.cur_next();
      ts.expect({"as"});
      const auto new_name = ts.cur_next();
      pf.children.push_back(ASTNode(
          Token(tok, "USE"), {vk_pairs, rule_name, new_name}));
    }

    else if (t == "ls") {
      pf.children.push_back(ASTNode(Token(tok, "LS")));
    }

    else if (t == "sorry") {
      pf.children.push_back(ASTNode(Token(tok, "SORRY")));
    }

    else if (t != ";") {
      throw std::runtime_error(
          "Invalid proof statement start token '" + t + "'");
    }
  }
  ts.expect({"}"});
  return pf;
}
