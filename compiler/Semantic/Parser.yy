%skeleton "lalr1.cc"
%require "3.0"
%defines
%define api.parser.class { Parser }
%define api.namespace { klang }
%define api.value.type variant
%define api.token.constructor
%define parse.assert

%code requires
{
#include <Semantic/AST.h>

namespace klang {
class Scanner;
} // namespace klang

klang::ASTModule* GetModule(void);
}

%code top
{
#include <Semantic/AST.h>
#include <Semantic/Scanner.h>
#include "location.hh"

static klang::Parser::symbol_type yylex(klang::Scanner& scanner) {
  return scanner.next_token();
}

using namespace klang;

ASTModule* gTheModule = nullptr; 
ASTModule* GetModule(void) {
        return gTheModule;
}

extern "C" int yylineno;

}

%lex-param { klang::Scanner &scanner }
%parse-param { klang::Scanner &scanner }

%locations
%define parse.trace
%define parse.error verbose
%define api.token.prefix {T_}

%token END 0 "end of file"
%token <std::string> STRING "string";
%token <std::string> ID  "identifier";
%token <int64_t> NUMBER "number";
%token COLON    "colon";
%token SEMICOLON "semicolon";
%token EQUAL    "equal";
%token ASSIGN   "assign"
%token LBRAC    "lbracket";
%token RBRAC    "rbracket";
%token COMMA    "comma";
%token LBLK     "lblock";
%token RBLK     "rblock";
%token LIDX     "lindex";
%token RIDX     "rindex";
%token WHILE    "while";
%token ARROW    "arrow";

// logic binops
%token LE   "le";
%token LT   "lt";
%token GT   "gt";
%token GE   "ge";
%token EQ   "eq";

// arithmetic
%token ADD  "add";
%token SUB  "sub";
%token MUL  "mul";
%token DIV  "div";

%left LT GT LE GE EQ
%left ADD SUB
%left MUL DIV

// keywords
%token INT      "int"
%token ARRAY    "array"
%token STRINGK   "stringk"
%token VOID     "void"
%token IF       "if"
%token ELSE     "else"
%token FUNCTION "function"
%token RETURN   "return"
%token DO       "do"

%type<class ASTExpression*> literals expression
%type<class ASTStatement*> statement single_statement
%type<class ASTFunction*> function 
%type<std::vector<class ASTStatement*>> statements block
%type<std::vector<class ASTFunction*>> functions
%type<int> type
%type<std::pair<std::string, int>> argument 
%type<std::vector<std::pair<std::string, int>>> argument_list vardefs
%type<std::vector<ASTExpression*>> expression_list

%start program

%% 

program 
        : functions { 
                gTheModule = new ASTModule($1);
        }
        ;

functions 
        :                       { $$ = std::vector<ASTFunction*>(); }
        | functions function    { 
                $$ = $1;
                $$.push_back($2);
        }
        ;

function 
        : FUNCTION ID LBRAC argument_list RBRAC COLON vardefs ARROW type block { 
                $$ = new ASTFunction($2.c_str(), $9, $4, $7, $10);
        }
        ;

argument_list
        :                              { $$ = std::vector<std::pair<std::string,int>>(); }
        | argument {
                $$ = std::vector<std::pair<std::string,int>>();
                $$.push_back($1);
        }
        | argument_list COMMA argument { 
                $$ = $1;
                $$.push_back($3);
        }
        ;

argument
        : type ID       { $$ = std::make_pair($2, $1); }
        ;

type
        : INT           { $$ = TY_INTEGER; }
        | ARRAY         { $$ = TY_ARRAY; }
        | STRING        { $$ = TY_STRING; }
        | VOID          { $$ = TY_VOID; }
        ;

vardefs
        : argument_list { $$ = $1; }
        ;

block
        : LBLK statements RBLK          { $$ = $2; }
        ;

statements
        :       { $$ = std::vector<ASTStatement*>(); }
        | statement {
                $$ = std::vector<ASTStatement*>();
                $$.push_back($1);
        }
        | statements statement {
                $$ = $1;
                $$.push_back($2);
        }
        ;

statement
        : single_statement SEMICOLON { $$ = $1; }
        ;

single_statement
        : ID ASSIGN expression    {
                $$ = new ASTStatementAssign(new ASTExpressionVariable($1), $3, yylineno);
        }
        | ID LIDX expression RIDX ASSIGN expression {
                $$ = new ASTStatementAssign(new ASTExpressionArrayAccess($1, $3), $6, yylineno);
        }
        | ID LBRAC expression_list RBRAC {
                $$ = new ASTStatementFunctionCall(new ASTExpressionFunctionCall($1, $3), yylineno);
        }
        | IF LBRAC expression RBRAC block ELSE block {
                $$ = new ASTStatementIfElse($3, $5, $7, yylineno);
        }
        | IF LBRAC expression RBRAC block {
                $$ = new ASTStatementIf($3, $5, yylineno);
        }
        | DO block WHILE LBRAC expression RBRAC {
                $$ = new ASTStatementWhile($5, $2, yylineno);
        }
        | RETURN expression {
                $$ = new ASTStatementReturn($2, yylineno);
        }
        | RETURN {
                $$ = new ASTStatementReturn(nullptr, yylineno);
        }
        ;

expression_list
        :       { $$ = std::vector<ASTExpression*>(); }
        | expression {
                $$ = std::vector<ASTExpression*>();
                $$.push_back($1);
        }
        | expression_list COMMA expression {
                $$ = $1;
                $$.push_back($3);
        }
        ;

expression
        : literals
        | expression ADD expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_ADD, $1, $3);
        }
        | expression SUB expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_SUB, $1, $3);
        }
        | expression MUL expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_MUL, $1, $3);
        }
        | expression DIV expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_DIV, $1, $3);
        }
        | expression LT expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_LT, $1, $3);
        }
        | expression LE expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_LTE, $1, $3);
        }
        | expression GT expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_GT, $1, $3);
        }
        | expression GE expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_GTE, $1, $3);
        }
        | expression EQ expression {
                $$ = new ASTExpressionBinary(ASTExpressionBinary::OP_EQ, $1, $3);
        }
        | ID LBRAC expression_list RBRAC {
                $$ = new ASTExpressionFunctionCall($1, $3);
        }
        | ID LIDX expression RIDX {
                $$ = new ASTExpressionArrayAccess($1, $3);
        } 
        | ID {
                $$ = new ASTExpressionVariable($1);
        }
        ;

literals
        : NUMBER        { 
                $$ = new ASTExpressionInteger($1);
        }
        | STRING        {
                $$ = new ASTExpressionString($1);
        }
        ;
%% 

void klang::Parser::error(const location& loc, const std::string& message) {
        std::cerr << message.c_str() << std::endl;
}