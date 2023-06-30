%code requires {
    #include <memory>
    #include <string>
    #include "AST.h"
}

%{

#include <iostream>
#include <memory>
#include <string>
#include <vector>
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
    std::vector<std::unique_ptr<BaseAST>> *vec_val;
}

%token INT VOID RETURN CONST IF ELSE WHILE BREAK CONTINUE
%token <str_val> IDENT
%token <int_val> INT_CONST
%token <str_val> LE GE EQ NE AND OR

%type <str_val> UNARYOP MULOP ADDOP RELOP EQOP ANDOP OROP
%type <ast_val> FuncDef Block Stmt Exp PrimaryExp UnaryExp AddExp
%type <ast_val> MulExp RelExp EqExp LAndExp LOrExp Decl ConstDecl ConstDef
%type <ast_val> ConstInitVal BlockItem ConstExp VarDecl VarDef InitVal
%type <ast_val> SimpleStmt OpenStmt ClosedStmt CompUnitList FuncFParam
%type <vec_val> BlockItemList ConstDefList VarDefList FuncFParams FuncRParams
%type <vec_val> ConstExpList ConstInitValList InitValList ExpList
%type <int_val> Number
%type <str_val> LVal Type

%%

CompUnit
    : CompUnitList {
        auto comp_unit = unique_ptr<BaseAST>($1);
        ast = move(comp_unit);
    }
    ;

CompUnitList
    : FuncDef {
        auto comp_unit = new CompUnitAST();
        auto func_def = unique_ptr<BaseAST>($1);
        comp_unit->func_def_list.push_back(move(func_def));
        $$ = comp_unit;
    }
    | Decl {
        auto comp_unit = new CompUnitAST();
        auto decl = unique_ptr<BaseAST>($1);
        comp_unit->decl_list.push_back(move(decl));
        $$ = comp_unit;
    }
    | CompUnitList FuncDef {
        auto comp_unit = (CompUnitAST*)($1);
        auto func_def = unique_ptr<BaseAST>($2);
        comp_unit->func_def_list.push_back(move(func_def));
        $$ = comp_unit;
    }
    | CompUnitList Decl {
        auto comp_unit = (CompUnitAST*)($1);
        auto decl = unique_ptr<BaseAST>($2);
        comp_unit->decl_list.push_back(move(decl));
        $$ = comp_unit;
    }
    ;

FuncDef
    : Type IDENT '(' ')' Block {
        auto func_def = new FuncDefAST();
        func_def->func_type = *unique_ptr<string>($1);
        func_def->ident = *unique_ptr<string>($2);
        func_def->block = unique_ptr<BaseAST>($5);
        $$ = func_def;
    }
    | Type IDENT '(' FuncFParams ')' Block {
        auto func_def = new FuncDefAST();
        func_def->func_type = *unique_ptr<string>($1);
        func_def->ident = *unique_ptr<string>($2);
        vector<unique_ptr<BaseAST>> *v_ptr = ($4);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            func_def->params.push_back(move(*it));
        func_def->block = unique_ptr<BaseAST>($6);
        ((BlockAST*)(func_def->block).get())->func = func_def->ident;
        $$ = func_def;
    }
    ;

FuncFParams
    : FuncFParam {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | FuncFParams ',' FuncFParam {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

FuncFParam
    : Type IDENT {
        auto param = new FuncFParamAST();
        param->type = FuncFParamType::var;
        param->b_type = *unique_ptr<string>($1);
        param->ident = *unique_ptr<string>($2);
        $$ = param;
    }
    | Type IDENT '[' ']' {
        auto param = new FuncFParamAST();
        param->type = FuncFParamType::list;
        param->b_type = *unique_ptr<string>($1);
        param->ident = *unique_ptr<string>($2);
        $$ = param;
    }
    | Type IDENT '[' ']' ConstExpList {
        auto param = new FuncFParamAST();
        param->type = FuncFParamType::list;
        param->b_type = *unique_ptr<string>($1);
        param->ident = *unique_ptr<string>($2);
        vector<unique_ptr<BaseAST>> *v_ptr = ($5);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            param->const_exp_list.push_back(move(*it));
        $$ = param;
    }
    ;

FuncRParams
    : Exp {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | FuncRParams ',' Exp {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

Block
    : '{' BlockItemList '}' {
        auto block = new BlockAST();
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            block->block_item_list.push_back(move(*it));
        $$ = block;
    }
    ;

Stmt
    : OpenStmt {
        auto stmt = ($1);
        $$ = stmt;
    }
    | ClosedStmt {
        auto stmt = ($1);
        $$ = stmt;
    }
    ;

ClosedStmt
    : SimpleStmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::simple;
        stmt->exp_simple = unique_ptr<BaseAST>($1);
        $$ = stmt;
    }
    | IF '(' Exp ')' ClosedStmt ELSE ClosedStmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::ifelse;
        stmt->exp_simple = unique_ptr<BaseAST>($3);
        stmt->if_stmt = unique_ptr<BaseAST>($5);
        stmt->else_stmt = unique_ptr<BaseAST>($7);
        $$ = stmt;
    }
    | WHILE '(' Exp ')' ClosedStmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::while_;
        stmt->exp_simple = unique_ptr<BaseAST>($3);
        stmt->while_stmt = unique_ptr<BaseAST>($5);
        $$ = stmt;
    }
    ;

OpenStmt
    : IF '(' Exp ')' Stmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::if_;
        stmt->exp_simple = unique_ptr<BaseAST>($3);
        stmt->if_stmt = unique_ptr<BaseAST>($5);
        $$ = stmt;
    }
    | IF '(' Exp ')' ClosedStmt ELSE OpenStmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::ifelse;
        stmt->exp_simple = unique_ptr<BaseAST>($3);
        stmt->if_stmt = unique_ptr<BaseAST>($5);
        stmt->else_stmt = unique_ptr<BaseAST>($7);
        $$ = stmt;
    }
    | WHILE '(' Exp ')' OpenStmt {
        auto stmt = new StmtAST();
        stmt->type = StmtType::while_;
        stmt->exp_simple = unique_ptr<BaseAST>($3);
        stmt->while_stmt = unique_ptr<BaseAST>($5);
        $$ = stmt;
    }
    ;

SimpleStmt
    : RETURN Exp ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::ret;
        stmt->block_exp = unique_ptr<BaseAST>($2);
        $$ = stmt;
    }
    | RETURN ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::ret;
        stmt->block_exp = nullptr;
        $$ = stmt;
    }
    | LVal '=' Exp ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::lval;
        stmt->lval = *unique_ptr<string>($1);
        stmt->block_exp = unique_ptr<BaseAST>($3);
        $$ = stmt;
    }
    | IDENT ExpList '=' Exp ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::list;
        stmt->lval = *unique_ptr<string>($1);
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            stmt->exp_list.push_back(move(*it));
        stmt->block_exp = unique_ptr<BaseAST>($4);
        $$ = stmt;
    }
    | Block {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::block;
        stmt->block_exp = unique_ptr<BaseAST>($1);
        $$ = stmt;
    }
    | Exp ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::exp;
        stmt->block_exp = unique_ptr<BaseAST>($1);
        $$ = stmt;
    }
    | ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::exp;
        stmt->block_exp = nullptr;
        $$ = stmt;
    }
    | BREAK ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::break_;
        $$ = stmt;
    }
    | CONTINUE ';' {
        auto stmt = new SimpleStmtAST();
        stmt->type = SimpleStmtType::continue_;
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
    | IDENT '(' ')' {
        auto unary_exp = new UnaryExpAST();
        unary_exp->type = UnaryExpType::func_call;
        unary_exp->ident = *unique_ptr<string>($1);
        $$ = unary_exp;
    }
    | IDENT '(' FuncRParams ')' {
        auto unary_exp = new UnaryExpAST();
        unary_exp->type = UnaryExpType::func_call;
        unary_exp->ident = *unique_ptr<string>($1);
        vector<unique_ptr<BaseAST>> *v_ptr = ($3);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            unary_exp->params.push_back(move(*it));
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
    | IDENT ExpList {
        auto primary_exp = new PrimaryExpAST();
        primary_exp->type = PrimaryExpType::list;
        primary_exp->lval = *unique_ptr<string>($1);
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            primary_exp->exp_list.push_back(move(*it));
        $$ = primary_exp;
    }
    ;

Decl
    : ConstDecl {
        auto decl = new DeclAST();
        decl->type = DeclType::const_decl;
        decl->decl = unique_ptr<BaseAST>($1);
        $$ = decl;
    }
    | VarDecl {
        auto decl = new DeclAST();
        decl->type = DeclType::var_decl;
        decl->decl = unique_ptr<BaseAST>($1);
        $$ = decl;
    }
    ;

ConstDecl
    : CONST Type ConstDefList ';' {
        auto const_decl = new ConstDeclAST();
        const_decl->b_type = *unique_ptr<string>($2);
        vector<unique_ptr<BaseAST>> *v_ptr = ($3);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            const_decl->const_def_list.push_back(move(*it));
        $$ = const_decl;
    }
    ;

ConstDef
    : IDENT '=' ConstInitVal {
        auto const_def = new ConstDefAST();
        const_def->ident = *unique_ptr<string>($1);
        const_def->const_init_val = unique_ptr<BaseAST>($3);
        $$ = const_def;
    }
    | IDENT ConstExpList '=' ConstInitVal {
        auto const_def = new ConstDefAST();
        const_def->ident = *unique_ptr<string>($1);
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            const_def->const_exp_list.push_back(move(*it));
        const_def->const_init_val = unique_ptr<BaseAST>($4);
        $$ = const_def;
    }
    ;

ConstInitVal
    : ConstExp {
        auto const_init_val = new ConstInitValAST();
        const_init_val->type = ConstInitValType::const_exp;
        const_init_val->const_exp = unique_ptr<BaseAST>($1);
        $$ = const_init_val;
    }
    | '{' '}' {
        auto const_init_val = new ConstInitValAST();
        const_init_val->type = ConstInitValType::list;
        $$ = const_init_val;
    }
    | '{' ConstInitValList '}' {
        auto const_init_val = new ConstInitValAST();
        const_init_val->type = ConstInitValType::list;
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            const_init_val->const_init_val_list.push_back(move(*it));
        $$ = const_init_val;
    }
    ;

BlockItem
    : Decl {
        auto block_item = new BlockItemAST();
        block_item->type = BlockItemType::decl;
        block_item->content = unique_ptr<BaseAST>($1);
        $$ = block_item;
    }
    | Stmt {
        auto block_item = new BlockItemAST();
        block_item->type = BlockItemType::stmt;
        block_item->content = unique_ptr<BaseAST>($1);
        $$ = block_item;
    }
    ;

ConstExp
    : Exp {
        auto const_exp = new ConstExpAST();
        const_exp->exp = unique_ptr<BaseAST>($1);
        $$ = const_exp;
    }
    ;

VarDecl
    : Type VarDefList ';' {
        auto var_decl = new VarDeclAST();
        var_decl->b_type = *unique_ptr<string>($1);
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            var_decl->var_def_list.push_back(move(*it));
        $$ = var_decl;
    }
    ;

VarDef
    : IDENT {
        auto var_def = new VarDefAST();
        var_def->ident = *unique_ptr<string>($1);
        var_def->has_init_val = false;
        $$ = var_def;
    }
    | IDENT '=' InitVal {
        auto var_def = new VarDefAST();
        var_def->ident = *unique_ptr<string>($1);
        var_def->has_init_val = true;
        var_def->init_val = unique_ptr<BaseAST>($3);
        $$ = var_def;
    }
    | IDENT ConstExpList {
        auto var_def = new VarDefAST();
        var_def->ident = *unique_ptr<string>($1);
        var_def->has_init_val = false;
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            var_def->exp_list.push_back(move(*it));
        $$ = var_def;
    }
    | IDENT ConstExpList '=' InitVal {
        auto var_def = new VarDefAST();
        var_def->ident = *unique_ptr<string>($1);
        var_def->has_init_val = true;
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            var_def->exp_list.push_back(move(*it));
        var_def->init_val = unique_ptr<BaseAST>($4);
        $$ = var_def;
    }
    ;

InitVal
    : Exp {
        auto init_val = new InitValAST();
        init_val->type = InitValType::exp;
        init_val->exp = unique_ptr<BaseAST>($1);
        $$ = init_val;
    }
    | '{' '}' {
        auto init_val = new InitValAST();
        init_val->type = InitValType::list;
        $$ = init_val;
    }
    | '{' InitValList '}' {
        auto init_val = new InitValAST();
        init_val->type = InitValType::list;
        vector<unique_ptr<BaseAST>> *v_ptr = ($2);
        for (auto it = v_ptr->begin(); it != v_ptr->end(); it++)
            init_val->init_val_list.push_back(move(*it));
        $$ = init_val;
    }
    ;

BlockItemList
    : {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        $$ = v;
    }
    | BlockItemList BlockItem {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($2));
        $$ = v;
    }
    ;

ConstDefList
    : ConstDef {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | ConstDefList ',' ConstDef {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

VarDefList
    : VarDef {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | VarDefList ',' VarDef {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

ConstExpList
    : '[' ConstExp ']' {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($2));
        $$ = v;
    }
    | ConstExpList '[' ConstExp ']' {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

ExpList
    : '[' Exp ']' {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($2));
        $$ = v;
    }
    | ExpList '[' Exp ']' {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

ConstInitValList
    : ConstInitVal {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | ConstInitValList ',' ConstInitVal {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
    }
    ;

InitValList
    : InitVal {
        vector<unique_ptr<BaseAST>> *v = new vector<unique_ptr<BaseAST>>;
        v->push_back(unique_ptr<BaseAST>($1));
        $$ = v;
    }
    | InitValList ',' InitVal {
        vector<unique_ptr<BaseAST>> *v = ($1);
        v->push_back(unique_ptr<BaseAST>($3));
        $$ = v;
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

Type
    : INT {
        string *type = new string("int");
        $$ = type;
    }
    | VOID {
        string *type = new string("void");
        $$ = type;
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
