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
%token <str_val> LE GE EQ NE AND OR

%type <str_val> UNARYOP MULOP ADDOP RELOP EQOP ANDOP OROP
%type <ast_val> FuncDef FuncType Block Stmt Exp PrimaryExp UnaryExp AddExp
%type <ast_val> MulExp RelExp EqExp LAndExp LOrExp
%type <int_val> Number
%type <str_val> LVal

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
    : RETURN Exp ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::ret;
        stmt->block_exp = unique_ptr<BaseAST>($2);
        $$ = stmt;
    }
    ;

Exp
    : LOrExp {
        auto exp = new ExpAST();
        exp->l_or_exp = unique_ptr<BaseAST>($1);
        $$ = exp;
    }
    ;

LOrExp
    : LAndExp {
        auto l_or_exp = new LOrExpAST();
        l_or_exp->op = "";
        l_or_exp->l_and_exp = unique_ptr<BaseAST>($1);
        $$ = l_or_exp;
    }
    | LOrExp OROP LAndExp {
        auto l_or_exp = new LOrExpAST();
        l_or_exp->l_or_exp = unique_ptr<BaseAST>($1);
        l_or_exp->op = *unique_ptr<string>($2);
        l_or_exp->l_and_exp = unique_ptr<BaseAST>($3);
        $$ = l_or_exp;
    }
    ;

LAndExp
    : EqExp {
        auto l_and_exp = new LAndExpAST();
        l_and_exp->op = "";
        l_and_exp->eq_exp = unique_ptr<BaseAST>($1);
        $$ = l_and_exp;
    }
    | LAndExp ANDOP EqExp {
        auto l_and_exp = new LAndExpAST();
        l_and_exp->l_and_exp = unique_ptr<BaseAST>($1);
        l_and_exp->op = *unique_ptr<string>($2);
        l_and_exp->eq_exp = unique_ptr<BaseAST>($3);
        $$ = l_and_exp;
    }
    ;

EqExp
    : RelExp {
        auto eq_exp = new EqExpAST();
        eq_exp->op = "";
        eq_exp->rel_exp = unique_ptr<BaseAST>($1);
        $$ = eq_exp;
    }
    | EqExp EQOP RelExp {
        auto eq_exp = new EqExpAST();
        eq_exp->eq_exp = unique_ptr<BaseAST>($1);
        eq_exp->op = *unique_ptr<string>($2);
        eq_exp->rel_exp = unique_ptr<BaseAST>($3);
        $$ = eq_exp;
    }
    ;

RelExp
    : AddExp {
        auto rel_exp = new RelExpAST();
        rel_exp->op = "";
        rel_exp->add_exp = unique_ptr<BaseAST>($1);
        $$ = rel_exp;
    }
    | RelExp RELOP AddExp {
        auto rel_exp = new RelExpAST();
        rel_exp->rel_exp = unique_ptr<BaseAST>($1);
        rel_exp->op = *unique_ptr<string>($2);
        rel_exp->add_exp = unique_ptr<BaseAST>($3);
        $$ = rel_exp;
    }
    ;

AddExp
    : MulExp {
        auto add_exp = new AddExpAST();
        add_exp->op = "";
        add_exp->mul_exp = unique_ptr<BaseAST>($1);
        $$ = add_exp;
    }
    | AddExp ADDOP MulExp {
        auto add_exp = new AddExpAST();
        add_exp->add_exp = unique_ptr<BaseAST>($1);
        add_exp->op = *unique_ptr<string>($2);
        add_exp->mul_exp = unique_ptr<BaseAST>($3);
        $$ = add_exp;
    }
    ;

MulExp
    : UnaryExp {
        auto mul_exp = new MulExpAST();
        mul_exp->op = "";
        mul_exp->unary_exp = unique_ptr<BaseAST>($1);
        $$ = mul_exp;
    }
    | MulExp MULOP UnaryExp {
        auto mul_exp = new MulExpAST();
        mul_exp->mul_exp = unique_ptr<BaseAST>($1);
        mul_exp->op = *unique_ptr<string>($2);
        mul_exp->unary_exp = unique_ptr<BaseAST>($3);
        $$ = mul_exp;
    }
    ;

UnaryExp
    : PrimaryExp {
        auto unary_exp = new UnaryExpAST();
        unary_exp->type = UnaryExpType::primary;
        unary_exp->exp = unique_ptr<BaseAST>($1);
        $$ = unary_exp;
    }
    | UNARYOP UnaryExp {
        auto unary_exp = new UnaryExpAST();
        unary_exp->type = UnaryExpType::unary;
        unary_exp->op = *unique_ptr<string>($1);
        unary_exp->exp = unique_ptr<BaseAST>($2);
        $$ = unary_exp;
    }
    ;

PrimaryExp
    : '(' Exp ')' {
        auto primary_exp = new PrimaryExpAST();
        primary_exp->type = PrimaryExpType::exp;
        primary_exp->exp = unique_ptr<BaseAST>($2);
        $$ = primary_exp;
    }
    | Number {
        auto primary_exp = new PrimaryExpAST();
        primary_exp->type = PrimaryExpType::number;
        primary_exp->number = ($1);
        $$ = primary_exp;
    }
    | LVal {
        auto primary_exp = new PrimaryExpAST();
        primary_exp->type = PrimaryExpType::lval;
        primary_exp->lval = *unique_ptr<string>($1);
        $$ = primary_exp;
    }
    ;

Number
    : INT_CONST {
        $$ = ($1);
    }
    ;

LVal
    : IDENT {
        string *lval = new string(*unique_ptr<string>($1));
        $$ = lval;
    }
    ;

UNARYOP
    : '+' {
        string *op = new string("+");
        $$ = op;
    }
    | '-' {
        string *op = new string("-");
        $$ = op;
    }
    | '!' {
        string *op = new string("!");
        $$ = op;
    }
    ;

MULOP
    : '*' {
        string *op = new string("*");
        $$ = op;
    }
    | '/' {
        string *op = new string("/");
        $$ = op;
    }
    | '%' {
        string *op = new string("%");
        $$ = op;
    }
    ;

ADDOP
    : '+' {
        string *op = new string("+");
        $$ = op;
    }
    | '-' {
        string *op = new string("-");
        $$ = op;
    }
    ;

RELOP
    : LE {
        string *op = new string("<=");
        $$ = op;
    }
    | GE {
        string *op = new string(">=");
        $$ = op;
    }
    | '<' {
        string *op = new string("<");
        $$ = op;
    }
    | '>' {
        string *op = new string(">");
        $$ = op;
    }
    ;

EQOP
    : EQ {
        string *op = new string("==");
        $$ = op;
    }
    | NE {
        string *op = new string("!=");
        $$ = op;
    }
    ;

ANDOP
    : AND {
        string *op = new string("&&");
        $$ = op;
    }
    ;

OROP
    : OR {
        string *op = new string("||");
        $$ = op;
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
