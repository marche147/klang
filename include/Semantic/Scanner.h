#ifndef _SCANNER_H
#define _SCANNER_H

#if ! defined(yyFlexLexerOnce)
#undef yyFlexLexer
#define yyFlexLexer klang_FlexLexer // the trick with prefix; no namespace here :(
#include <FlexLexer.h>
#endif

#undef YY_DECL
#define YY_DECL klang::Parser::symbol_type klang::Scanner::next_token()

#include <iostream>
#include <string>

#include "Parser.h"

namespace klang {

class Scanner : public yyFlexLexer {
public:
  Scanner(std::istream *in) : yyFlexLexer(in) {}
  virtual ~Scanner() {}

  virtual int yylex() {
    yyerror("yylex() should not be called directly");
    return 0;
  }

  virtual Parser::symbol_type next_token();

private:
  void yyerror(const char *msg) {
    throw std::runtime_error(msg);
  }
};

} // namespace klang

#endif 