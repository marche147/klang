%{
#include <Semantic/Scanner.h>
#include <Util.h>
#include "location.hh"

#include "Parser.h"

using namespace klang;

#define yyterminate()   Parser::make_END(location());

// TODO: Fix line number counting
int yylineno = 0;
%}

%option nodefault
%option noyywrap
%option yylineno
%option c++
%option yyclass="Scanner"
%option prefix="klang_"

%x C_COMMENT
%x C_STRING

HEXPREFIX       "0x"
HEXDIGIT        [0-9a-zA-Z]
DIGIT           [0-9]
ID              [a-z_][a-zA-Z0-9_]*
STRCHAR         [^\"\n\\]
ESCAPE          (\\[^\n])

%%

[\t ]+    { /* ignore whitespace */ }
\n       { yylineno++; }    

"int"       { return Parser::make_INT(location()); }
"array"     { return Parser::make_ARRAY(location()); }
"string"    { return Parser::make_STRINGK(location()); }
"void"      { return Parser::make_VOID(location()); }

"if"        { return Parser::make_IF(location()); }
"else"      { return Parser::make_ELSE(location()); }
"while"     { return Parser::make_WHILE(location()); }
"return"    { return Parser::make_RETURN(location()); }
"function"  { return Parser::make_FUNCTION(location()); }
"do"        { return Parser::make_DO(location()); }

"=="                { return Parser::make_EQ(location()); }
"<="                { return Parser::make_LE(location()); }
"<"                 { return Parser::make_LT(location()); }
">="                { return Parser::make_GE(location()); }
">"                 { return Parser::make_GT(location()); }

"="                 { return Parser::make_EQUAL(location()); }
"("                 { return Parser::make_LBRAC(location()); }
")"                 { return Parser::make_RBRAC(location()); }
","                 { return Parser::make_COMMA(location()); }
"{"                 { return Parser::make_LBLK(location()); }
"}"                 { return Parser::make_RBLK(location()); }
"["                 { return Parser::make_LIDX(location()); }
"]"                 { return Parser::make_RIDX(location()); }

"+"                 { return Parser::make_ADD(location()); }
"-"                 { return Parser::make_SUB(location()); }
"*"                 { return Parser::make_MUL(location()); }
"/"                 { return Parser::make_DIV(location()); }

{DIGIT}+            { return Parser::make_NUMBER(std::strtoll(yytext, nullptr, 10), location()); }
({HEXPREFIX}{HEXDIGIT}+)    { return Parser::make_NUMBER(std::strtoll(yytext, nullptr, 16), location()); }
{ID}                { return Parser::make_ID(std::string(yytext), location()); }
":"                 { return Parser::make_COLON(location()); }
";"                 { return Parser::make_SEMICOLON(location()); }
"->"                { return Parser::make_ARROW(location()); }
":="                { return Parser::make_ASSIGN(location()); }

"/*"                { BEGIN(C_COMMENT); }
<C_COMMENT>"*/"     { BEGIN(INITIAL); }
<C_COMMENT><<EOF>>  { yyerror("unterminated block comment"); }
<C_COMMENT>.
<C_COMMENT>\n        { yylineno++; }

\"                 { BEGIN(C_STRING); }
<C_STRING>\n       { yyerror("string literal not terminated when hitting newline"); }
<C_STRING><<EOF>>  { yyerror("string literal not terminated when hitting EOF"); }
<C_STRING>({STRCHAR}|{ESCAPE})* { yymore(); }
<C_STRING>\"        { 
    std::string result(yytext);
    result.pop_back();  // remove the last "
    BEGIN(INITIAL);     // return to the normal state
    auto unescaped = Unescape(result);
    if(!unescaped.has_value()) {
        yyerror("invalid escape sequence in string literal");
    }
    return Parser::make_STRING(unescaped.value(), location());
}
<C_STRING>.        { yyerror("invalid character in string literal"); }

<<EOF>>     { return yyterminate(); }
%% 