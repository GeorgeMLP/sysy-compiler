%code requires {
    #include <memory>
    #include <string>
    #include "AST.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include "AST.h"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);

using namespace std;

%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
    std::string *str_val;
    int int_val;
    BaseAST *ast_val;
}

%token INT RETURN
%token <str_val> IDENT
%token <int_val> INT_CONST

%type <ast_val> FuncDef FuncType Block Stmt
%type <int_val> Number

%%

CompUnit
    : FuncDef {
        auto comp_unit = make_unique<CompUnitAST>();
        comp_unit->func_def = unique_ptr<BaseAST>($1);
        ast = move(comp_unit);
    }
    ;

FuncDef
    : FuncType IDENT '(' ')' Block {
        auto func_def = new FuncDefAST();
        func_def->func_type = unique_ptr<BaseAST>($1);
        func_def->ident = *unique_ptr<string>($2);
        func_def->block = unique_ptr<BaseAST>($5);
        $$ = func_def;
    }
    ;

FuncType
    : INT {
        auto func_type = new FuncTypeAST();
        func_type->func_type_name = "int";
        $$ = func_type;
    }
    ;

Block
    : '{' Stmt '}' {
        auto block = new BlockAST();
        block->stmt = unique_ptr<BaseAST>($2);
        $$ = block;
    }
    ;

Stmt
    : RETURN Number ';' {
        auto stmt = new StmtAST();
        auto number = new string(to_string($2));
        stmt->stmt = "return " + *number + ";";
        $$ = stmt;
    }
    ;

Number
    : INT_CONST {
        $$ = ($1);
    }
    ;

%%

void yyerror(unique_ptr<BaseAST> &ast, const char *s)
{
    extern int yylineno;
    extern char *yytext;
    cerr << "ERROR: " << s << " at symbol '" << yytext << "' on line "
        << yylineno << endl;
}
