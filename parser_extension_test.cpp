/**
 * @brief Staging for a potential customizable grammar extension
 * to the parser. I'm writing this here rather than in
 * parser.hpp so as not to get that file any dirtier.
 */

#include "src/ast.hpp"
#include "src/parse.hpp"
#include <cassert>
#include <iostream>
#include <map>

/// Manages grammars, but only LR(1) recursive-descent ones. No
/// shift-reduce allowed!
class GrammarController {
public:
  ASTNode parse(TokenStream &_ts,
                const ASTNode &_grammar_fragment) {
    if (_grammar_fragment.text == "TOKEN") {
      return _ts.cur_next();
    } else if (_grammar_fragment.text == "LITERAL") {
      // (LITERAL raw_expected_text)
      _ts.expect({_grammar_fragment.children.at(0).text.text});
      return _grammar_fragment.children.at(0);
    } else if (_grammar_fragment.text == "GRAMMAR") {
      // (GRAMMAR grammar_name)
      const auto grammar_name =
          _grammar_fragment.children.at(0).text;
      const auto g = grammars.at(grammar_name.text);
      ASTNode out(grammar_name, {parse(_ts, g)});
      if (out.children.size() == 1 &&
          out.children.at(0).text == "_") {
        out.children = out.children.at(0).children;
      }
      return out;
    } else if (_grammar_fragment.text == "_") {
      // (_ any number of arguments ...)
      ASTNode out(_grammar_fragment.text);
      for (const auto &arg : _grammar_fragment.children) {
        const auto to_add = parse(_ts, arg);
        if (to_add != "NULL") {
          out.children.push_back(to_add);
        }
      }
      return out;
    } else if (_grammar_fragment.text == "OR") {
      std::map<std::string, size_t> starting_toks;
      for (size_t i = 0;
           i + 1 < _grammar_fragment.children.size(); ++i) {
        const auto meta_arg = _grammar_fragment.children.at(i);
        assert(meta_arg.text == "_");
        const auto arg = meta_arg.children.at(0);
        assert(arg.text == "LITERAL");
        const auto starting_tok = arg.children.at(0).text.text;
        assert(!starting_toks.contains(starting_tok));
        starting_toks[starting_tok] = i;
      }

      const auto t = _ts.cur().text;
      if (starting_toks.contains(t)) {
        return parse(_ts, _grammar_fragment.children.at(
                              starting_toks.at(t)));
      } else {
        return parse(_ts, _grammar_fragment.children.back());
      }
    }

    return ASTNode("PARSE_FAILURE");
  }

  std::map<std::string, ASTNode> grammars;
};

int main() {
  GrammarController gc;
  gc.grammars["FORMULA"] = ASTNode(
      "OR",
      {
          ASTNode("_",
                  {
                      ASTNode("LITERAL", {ASTNode("not")}),
                      ASTNode("GRAMMAR", {ASTNode("FORMULA")}),
                  }),
          ASTNode("_",
                  {
                      ASTNode("LITERAL", {ASTNode("or")}),
                      ASTNode("GRAMMAR", {ASTNode("FORMULA")}),
                      ASTNode("GRAMMAR", {ASTNode("FORMULA")}),
                  }),
          ASTNode("_",
                  {
                      ASTNode("LITERAL", {ASTNode("and")}),
                      ASTNode("GRAMMAR", {ASTNode("FORMULA")}),
                      ASTNode("GRAMMAR", {ASTNode("FORMULA")}),
                  }),
          ASTNode("_", {ASTNode("TOKEN")}),
      });

  TokenStream ts(lex_text("not or a and b and or c not d not a",
                          "NULL_FP"));

  const auto parsed =
      gc.parse(ts, ASTNode("GRAMMAR", {ASTNode("FORMULA")}));

  std::cout << "Parsed " << parsed << "\n";

  return 0;
}
