#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>


static int symbol_num = 0;
static int if_else_num = 0;
static int other_num = 0;
enum class UnaryExpType { primary, unary };
enum class PrimaryExpType { exp, number, lval };
enum class StmtType { if_, ifelse, simple };
enum class SimpleStmtType { lval, exp, block, ret };


class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void dump() const = 0;
    virtual std::string dumpIR() const = 0;
};


class CompUnitAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_def;
    void dump() const override
    {
        std::cout << "CompUnitAST { ";
        func_def->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        return func_def->dumpIR();
    }
};


class FuncDefAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_type;
    std::string ident;
    std::unique_ptr<BaseAST> block;
    void dump() const override
    {
        std::cout << "FuncDefAST { ";
        func_type->dump();
        std::cout << ", " << ident << ", ";
        block->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        std::cout << "fun @" << ident << "(): ";
        func_type->dumpIR();
        std::cout << " {" << std::endl << "\%entry:" << std::endl;
        block->dumpIR();
        std::cout << "}";
        return "";
    }
};


class FuncTypeAST : public BaseAST
{
public:
    std::string func_type_name;
    void dump() const override
    {
        std::cout << "FuncTypeAST{";
        std::cout << func_type_name;
        std::cout << "} ";
    }
    std::string dumpIR() const override
    {
        if (func_type_name == "int")std::cout << "i32";
        else std::cout << "ERROR";
        return "";
    }
};


class BlockAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> stmt;
    void dump() const override
    {
        std::cout << "BlockAST { ";
        stmt->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        return stmt->dumpIR();
    }
};


class StmtAST : public BaseAST
{
public:
    StmtType type;
    std::unique_ptr<BaseAST> exp_simple;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    void dump() const override
    {
        if (type == StmtType::simple)
        {
            std::cout << "StmtAST { ";
            exp_simple->dump();
            std::cout << " } ";
        }
        else assert(false);
    }
    std::string dumpIR() const override
    {
        if (type == StmtType::simple)return exp_simple->dumpIR();
        else assert(false);
        return "";
    }
};


class SimpleStmtAST : public BaseAST
{
public:
    SimpleStmtType type;
    std::string lval;
    std::unique_ptr<BaseAST> block_exp;
    void dump() const override
    {
        if (type == SimpleStmtType::ret)
        {
            std::cout << "RETURN { ";
            block_exp->dump();
            std::cout << " } ";
        }
        else assert(false);
    }
    std::string dumpIR() const override
    {
        if (type == SimpleStmtType::ret)
        {
            if (block_exp == nullptr)std::cout << '\t' << "ret" << std::endl;
            else
            {
                std::string result_var = block_exp->dumpIR();
                std::cout << '\t' << "ret " << result_var << std::endl;
            }
        }
        else assert(false);
        return "";
    }
};


class ExpAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> l_or_exp;
    void dump() const override
    {
        std::cout << "ExpAST { ";
        l_or_exp->dump();
        std::cout << " }";
    }
    std::string dumpIR() const override
    {
        return l_or_exp->dumpIR();
    }
};


class LOrExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> l_and_exp;
    std::unique_ptr<BaseAST> l_or_exp;
    void dump() const override
    {
        if (op == "")l_and_exp->dump();
        else
        {
            l_or_exp->dump();
            std::cout << op;
            l_and_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = l_and_exp->dumpIR();
        else if (op == "||")
        {
            std::string left_result = l_or_exp->dumpIR();
            std::string then_label = "\%then_" + std::to_string(if_else_num);
            std::string else_label = "\%else_" + std::to_string(if_else_num);
            std::string end_label = "\%end_" + std::to_string(if_else_num++);
            std::string result_var_ptr = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << result_var_ptr << " = alloc i32" << std::endl;
            std::cout << "\tbr " << left_result << ", " << then_label << ", "
                << else_label << std::endl;
            std::cout << then_label << ":" << std::endl;
            std::cout << "\tstore 1, " << result_var_ptr << std::endl;
            std::cout << "\tjump " << end_label << std::endl;
            std::cout << else_label << ":" << std::endl;
            std::string tmp_result_var = "%" + std::to_string(symbol_num++);
            std::string right_result = l_and_exp->dumpIR();
            std::cout << '\t' << tmp_result_var << " = ne " << right_result
                << ", 0" << std::endl;
            std::cout << "\tstore " << tmp_result_var << ", " << result_var_ptr
                << std::endl;
            std::cout << "\tjump " << end_label << std::endl;
            std::cout << end_label << ":" << std::endl;
            result_var = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << result_var << " = load " << result_var_ptr
                << std::endl;
        }
        else assert(false);
        return result_var;
    }
};


class LAndExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> eq_exp;
    std::unique_ptr<BaseAST> l_and_exp;
    void dump() const override
    {
        if (op == "")eq_exp->dump();
        else
        {
            l_and_exp->dump();
            std::cout << op;
            eq_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = eq_exp->dumpIR();
        else if (op == "&&")
        {
            std::string left_result = l_and_exp->dumpIR();
            std::string then_label = "\%then_" + std::to_string(if_else_num);
            std::string else_label = "\%else_" + std::to_string(if_else_num);
            std::string end_label = "\%end_" + std::to_string(if_else_num++);
            std::string result_var_ptr = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << result_var_ptr << " = alloc i32" << std::endl;
            std::cout << "\tbr " << left_result << ", " << then_label << ", "
                << else_label << std::endl;
            std::cout << then_label << ":" << std::endl;
            std::string tmp_result_var = "%" + std::to_string(symbol_num++);
            std::string right_result = eq_exp->dumpIR();
            std::cout << '\t' << tmp_result_var << " = ne " << right_result
                << ", 0" << std::endl;
            std::cout << "\tstore " << tmp_result_var << ", " << result_var_ptr
                << std::endl;
            std::cout << "\tjump " << end_label << std::endl;
            std::cout << else_label << ":" << std::endl;
            std::cout << "\tstore 0, " << result_var_ptr << std::endl;
            std::cout << "\tjump " << end_label << std::endl;
            std::cout << end_label << ":" << std::endl;
            result_var = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << result_var << " = load " << result_var_ptr
                << std::endl;
        }
        else assert(false);
        return result_var;
    }
};


class EqExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> rel_exp;
    std::unique_ptr<BaseAST> eq_exp;
    void dump() const override
    {
        if (op == "")rel_exp->dump();
        else
        {
            eq_exp->dump();
            std::cout << op;
            rel_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = rel_exp->dumpIR();
        else
        {
            std::string left_result = eq_exp->dumpIR();
            std::string right_result = rel_exp->dumpIR();
            result_var = "%" + std::to_string(symbol_num++);
            if (op == "==")
                std::cout << '\t' << result_var << " = eq " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == "!=")
                std::cout << '\t' << result_var << " = ne " << left_result <<
                    ", " << right_result << std::endl;
            else assert(false);
        }
        return result_var;
    }
};


class RelExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> add_exp;
    std::unique_ptr<BaseAST> rel_exp;
    void dump() const override
    {
        if (op == "")add_exp->dump();
        else
        {
            rel_exp->dump();
            std::cout << op;
            add_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = add_exp->dumpIR();
        else
        {
            std::string left_result = rel_exp->dumpIR();
            std::string right_result = add_exp->dumpIR();
            result_var = "%" + std::to_string(symbol_num++);
            if (op == "<")
                std::cout << '\t' << result_var << " = lt " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == ">")
                std::cout << '\t' << result_var << " = gt " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == "<=")
                std::cout << '\t' << result_var << " = le " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == ">=")
                std::cout << '\t' << result_var << " = ge " << left_result <<
                    ", " << right_result << std::endl;
            else assert(false);
        }
        return result_var;
    }
};


class AddExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> mul_exp;
    std::unique_ptr<BaseAST> add_exp;
    void dump() const override
    {
        if (op == "")mul_exp->dump();
        else
        {
            add_exp->dump();
            std::cout << op;
            mul_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = mul_exp->dumpIR();
        else
        {
            std::string left_result = add_exp->dumpIR();
            std::string right_result = mul_exp->dumpIR();
            result_var = "%" + std::to_string(symbol_num++);
            if (op == "+")
                std::cout << '\t' << result_var << " = add " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == "-")
                std::cout << '\t' << result_var << " = sub " << left_result <<
                    ", " << right_result << std::endl;
            else assert(false);
        }
        return result_var;
    }
};


class MulExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> unary_exp;
    std::unique_ptr<BaseAST> mul_exp;
    void dump() const override
    {
        if (op == "")unary_exp->dump();
        else
        {
            mul_exp->dump();
            std::cout << op;
            unary_exp->dump();
        }
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (op == "")result_var = unary_exp->dumpIR();
        else
        {
            std::string left_result = mul_exp->dumpIR();
            std::string right_result = unary_exp->dumpIR();
            result_var = "%" + std::to_string(symbol_num++);
            if (op == "*")
                std::cout << '\t' << result_var << " = mul " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == "/")
                std::cout << '\t' << result_var << " = div " << left_result <<
                    ", " << right_result << std::endl;
            else if (op == "%")
                std::cout << '\t' << result_var << " = mod " << left_result <<
                    ", " << right_result << std::endl;
            else assert(false);
        }
        return result_var;
    }
};


class UnaryExpAST : public BaseAST
{
public:
    UnaryExpType type;
    std::string op;
    std::unique_ptr<BaseAST> exp;
    void dump() const override
    {
        if (type == UnaryExpType::unary)std::cout << op;
        exp->dump();
    }
    std::string dumpIR() const override
    {
        if (type == UnaryExpType::primary)return exp->dumpIR();
        else
        {
            std::string result_var = exp->dumpIR();
            std::string next_var = "%" + std::to_string(symbol_num);
            if (op == "+")return result_var;
            else if (op == "-")std::cout << '\t' << next_var << " = sub 0, " <<
                result_var << std::endl;
            else if (op == "!")std::cout << '\t' << next_var << " = eq " <<
                result_var << ", 0" << std::endl;
            else assert(false);
            symbol_num++;
            return next_var;
        }
    }
};


class PrimaryExpAST : public BaseAST
{
public:
    PrimaryExpType type;
    std::unique_ptr<BaseAST> exp;
    std::string lval;
    int number;
    void dump() const override
    {
        if (type == PrimaryExpType::exp)exp->dump();
        else if (type == PrimaryExpType::number)std::cout << number;
        else if (type == PrimaryExpType::lval)assert(false);
        else assert(false);
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (type == PrimaryExpType::exp)result_var = exp->dumpIR();
        else if (type == PrimaryExpType::number)
            result_var = std::to_string(number);
        else if (type == PrimaryExpType::lval)assert(false);
        else assert(false);
        return result_var;
    }
};
