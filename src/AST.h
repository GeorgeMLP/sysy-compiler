#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <map>
#include <variant>


static int symbol_num = 0;
static int if_else_num = 0;
static int while_num = 0;
enum class UnaryExpType { primary, unary, func_call };
enum class PrimaryExpType { exp, number, lval };
enum class StmtType { if_, ifelse, simple, while_ };
enum class SimpleStmtType { lval, exp, block, ret, break_, continue_ };
enum class DeclType { const_decl, var_decl };
enum class BlockItemType { decl, stmt };
static std::vector<std::map<std::string, std::variant<int, std::string>>>
    symbol_tables;
static std::map<std::string, int> var_num;
static std::vector<int> while_stack;
static std::map<std::string, std::string> function_table;
static std::map<std::string, std::string> function_ret_type;
static std::map<std::string, int> function_param_num;
static std::map<std::string, std::vector<std::string>> function_param_idents;
static std::map<std::string, std::vector<std::string>> function_param_names;
static std::string present_func_type;


static std::variant<int, std::string> look_up_symbol_tables(std::string l_val)
{
    for (auto it = symbol_tables.rbegin(); it != symbol_tables.rend(); it++)
        if (it->count(l_val))
            return (*it)[l_val];
    assert(false);
    return -1;
}


class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void dump() const = 0;
    virtual std::string dumpIR() const = 0;
    virtual int dumpExp() const { assert(false); return -1; }
    virtual std::string get_ident() const { assert(false); return ""; }
};


class CompUnitAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> func_def_list;
    std::vector<std::unique_ptr<BaseAST>> decl_list;
    void dump() const override
    {
        std::cout << "CompUnitAST { ";
        for (auto&& func_def : func_def_list)
            func_def->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        std::cout << "decl @getint(): i32" << std::endl;
        std::cout << "decl @getch(): i32" << std::endl;
        std::cout << "decl @getarray(*i32): i32" << std::endl;
        std::cout << "decl @putint(i32)" << std::endl;
        std::cout << "decl @putch(i32)" << std::endl;
        std::cout << "decl @putarray(i32, *i32)" << std::endl;
        std::cout << "decl @starttime()" << std::endl;
        std::cout << "decl @stoptime()" << std::endl << std::endl;
        function_table["getint"] = "@getint";
        function_table["getch"] = "@getch";
        function_table["getarray"] = "@getarray";
        function_table["putint"] = "@putint";
        function_table["putch"] = "@putch";
        function_table["putarray"] = "@putarray";
        function_table["starttime"] = "@starttime";
        function_table["stoptime"] = "@stoptime";
        function_ret_type["getint"] = "int";
        function_ret_type["getch"] = "int";
        function_ret_type["getarray"] = "int";
        function_ret_type["putint"] = "void";
        function_ret_type["putch"] = "void";
        function_ret_type["putarray"] = "void";
        function_ret_type["starttime"] = "void";
        function_ret_type["stoptime"] = "void";
        function_param_num["getint"] = 0;
        function_param_num["getch"] = 0;
        function_param_num["getarray"] = 1;
        function_param_num["putint"] = 1;
        function_param_num["putch"] = 1;
        function_param_num["putarray"] = 2;
        function_param_num["starttime"] = 0;
        function_param_num["stoptime"] = 0;
        std::map<std::string, std::variant<int, std::string>> global_syms;
        symbol_tables.push_back(global_syms);
        for (auto&& decl : decl_list)decl->dumpExp();
        std::cout << std::endl;
        for (auto&& func_def : func_def_list)func_def->dumpIR();
        symbol_tables.pop_back();
        return "";
    }
};


class FuncFParamAST : public BaseAST
{
public:
    std::string b_type;
    std::string ident;
    void dump() const override { std::cout << b_type << " " << ident; }
    std::string dumpIR() const override
    {
        assert(b_type == "int");
        std::string param_name = "@" + ident;
        std::string name = param_name + "_" +
            std::to_string(var_num[param_name]++);
        std::cout << name;
        return name;
    }
    std::string get_ident() const override { return ident; }
};


class FuncDefAST : public BaseAST
{
public:
    std::string func_type;
    std::string ident;
    std::vector<std::unique_ptr<BaseAST>> params;
    std::unique_ptr<BaseAST> block;
    void dump() const override
    {
        std::cout << "FuncDefAST { " << func_type << ", " << ident << ", ";
        for (int i = 0; i < params.size(); i++)
        {
            params[i]->dump();
            if (i != params.size() - 1)std::cout << ", ";
        }
        block->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        std::string name = "@" + ident;
        assert(!symbol_tables[0].count(ident));
        assert(!function_table.count(ident));
        function_table[ident] = name;
        function_ret_type[ident] = func_type;
        function_param_num[ident] = params.size();
        present_func_type = func_type;
        std::vector<std::string> idents, names;
        std::cout << "fun " << name << "(";
        for (int i = 0; i < params.size(); i++)
        {
            idents.push_back(params[i]->get_ident());
            names.push_back(params[i]->dumpIR());
            std::cout << ": i32";
            if (i != params.size() - 1)std::cout << ", ";
        }
        function_param_idents[ident] = move(idents);
        function_param_names[ident] = move(names);
        std::cout << ")";
        if (func_type == "int")std::cout << ": i32";
        else if (func_type != "void")assert(false);
        std::cout << " {" << std::endl << "\%entry_" << ident << ":" <<
            std::endl;
        std::string block_type = block->dumpIR();
        if (block_type != "ret")
        {
            if (func_type == "int")std::cout << "\tret 0" << std::endl;
            else if (func_type == "void")std::cout << "\tret" << std::endl;
            else assert(false);
        }
        std::cout << "}" << std::endl << std::endl;
        return block_type;
    }
};


class BlockAST : public BaseAST
{
public:
    std::vector<std::unique_ptr<BaseAST>> block_item_list;
    std::string func = "";
    void dump() const override
    {
        std::cout << "BlockAST { ";
        for (auto&& block_item : block_item_list)block_item->dump();
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        std::string block_type = "";
        std::map<std::string, std::variant<int, std::string>> symbol_table;
        if (func != "")
        {
            std::vector<std::string> idents = function_param_idents[func];
            std::vector<std::string> names = function_param_names[func];
            for (int i = 0; i < names.size(); i++)
            {
                std::string ident = idents[i];
                std::string name = names[i]; name[0] = '%';
                symbol_table[ident] = name;
                std::cout << '\t' << name << " = alloc i32" << std::endl;
                std::cout << "\tstore " << names[i] << ", " << name <<
                    std::endl;
            }
        }
        symbol_tables.push_back(symbol_table);
        for (auto&& block_item : block_item_list)
        {
            block_type = block_item->dumpIR();
            if (block_type == "ret" || block_type == "break" ||
                block_type == "cont")break;
        }
        symbol_tables.pop_back();
        return block_type;
    }
};


class StmtAST : public BaseAST
{
public:
    StmtType type;
    std::unique_ptr<BaseAST> exp_simple;
    std::unique_ptr<BaseAST> if_stmt;
    std::unique_ptr<BaseAST> else_stmt;
    std::unique_ptr<BaseAST> while_stmt;
    void dump() const override
    {
        if (type == StmtType::simple)
        {
            std::cout << "StmtAST { ";
            exp_simple->dump();
            std::cout << " } ";
        }
        else if (type == StmtType::if_)
        {
            std::cout << "IF { ";
            exp_simple->dump();
            std::cout << " } THEN { ";
            if_stmt->dump();
            std::cout << " } ";
        }
        else if (type == StmtType::ifelse)
        {
            std::cout << "IF { ";
            exp_simple->dump();
            std::cout << " } THEN { ";
            if_stmt->dump();
            std::cout << " } ELSE { ";
            else_stmt->dump();
            std::cout << " } ";
        }
        else if (type == StmtType::while_)
        {
            std::cout << "WHILE { ";
            exp_simple->dump();
            std::cout << " } DO { ";
            while_stmt->dump();
            std::cout << " } ";
        }
        else assert(false);
    }
    std::string dumpIR() const override
    {
        if (type == StmtType::simple)return exp_simple->dumpIR();
        else if (type == StmtType::if_)
        {
            std::string if_result = exp_simple->dumpIR();
            std::string then_label = "\%then_" + std::to_string(if_else_num);
            std::string end_label = "\%end_" + std::to_string(if_else_num++);
            std::cout << "\tbr " << if_result << ", " << then_label << ", " <<
                end_label << std::endl;
            std::cout << then_label << ":" << std::endl;
            std::string if_stmt_type = if_stmt->dumpIR();
            if (if_stmt_type != "ret" && if_stmt_type != "break" &&
                if_stmt_type != "cont")
                std::cout << "\tjump " << end_label << std::endl;
            std::cout << end_label << ":" << std::endl;
        }
        else if (type == StmtType::ifelse)
        {
            std::string if_result = exp_simple->dumpIR();
            std::string then_label = "\%then_" + std::to_string(if_else_num);
            std::string else_label = "\%else_" + std::to_string(if_else_num);
            std::string end_label = "\%end_" + std::to_string(if_else_num++);
            std::cout << "\tbr " << if_result << ", " << then_label << ", " <<
                else_label << std::endl;
            std::cout << then_label << ":" << std::endl;
            std::string if_stmt_type = if_stmt->dumpIR();
            if (if_stmt_type != "ret" && if_stmt_type != "break" &&
                if_stmt_type != "cont")
                std::cout << "\tjump " << end_label << std::endl;
            std::cout << else_label << ":" << std::endl;
            std::string else_stmt_type = else_stmt->dumpIR();
            if (else_stmt_type != "ret" && else_stmt_type != "break" &&
                else_stmt_type != "cont")
                std::cout << "\tjump " << end_label << std::endl;
            if ((if_stmt_type == "ret" || if_stmt_type == "break" ||
                if_stmt_type == "cont") && (else_stmt_type == "ret" ||
                else_stmt_type == "break" || else_stmt_type == "cont"))
                return "ret";
            else std::cout << end_label << ":" << std::endl;
        }
        else if (type == StmtType::while_)
        {
            std::string entry_label = "\%while_" + std::to_string(while_num);
            std::string body_label = "\%do_" + std::to_string(while_num);
            std::string end_label = "\%while_end_" + std::to_string(while_num);
            while_stack.push_back(while_num++);
            std::cout << "\tjump " << entry_label << std::endl;
            std::cout << entry_label << ":" << std::endl;
            std::string while_result = exp_simple->dumpIR();
            std::cout << "\tbr " << while_result << ", " << body_label << ", "
                << end_label << std::endl;
            std::cout << body_label << ":" << std::endl;
            std::string while_stmt_type = while_stmt->dumpIR();
            if (while_stmt_type != "ret" && while_stmt_type != "break" &&
                while_stmt_type != "cont")
                std::cout << "\tjump " << entry_label << std::endl;
            std::cout << end_label << ":" << std::endl;
            while_stack.pop_back();
        }
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
        else if (type == SimpleStmtType::lval)
        {
            std::cout << "LVAL { " << lval << " = ";
            block_exp->dump();
            std::cout << " } ";
        }
        else if (type == SimpleStmtType::exp)
        {
            if (block_exp != nullptr)
            {
                std::cout << "EXP { ";
                block_exp->dump();
                std::cout << " } ";
            }
        }
        else if (type == SimpleStmtType::block)
        {
            std::cout << "BLOCK { ";
            block_exp->dump();
            std::cout << " } ";
        }
        else if (type == SimpleStmtType::break_)std::cout << "BREAK ";
        else if (type == SimpleStmtType::continue_)std::cout << "CONTINUE ";
        else assert(false);
    }
    std::string dumpIR() const override
    {
        if (type == SimpleStmtType::ret)
        {
            if (block_exp == nullptr)
            {
                if (present_func_type == "int")
                    std::cout << "\tret 0" << std::endl;
                else std::cout << "\tret" << std::endl;
            }
            else
            {
                std::string result_var = block_exp->dumpIR();
                std::cout << "\tret " << result_var << std::endl;
            }
            return "ret";
        }
        else if (type == SimpleStmtType::lval)
        {
            std::string result_var = block_exp->dumpIR();
            std::variant<int, std::string> value = look_up_symbol_tables(lval);
            assert(value.index() == 1);
            std::cout << "\tstore " << result_var << ", " <<
                std::get<std::string>(value) << std::endl;
        }
        else if (type == SimpleStmtType::exp)
        {
            if (block_exp != nullptr)block_exp->dumpIR();
        }
        else if (type == SimpleStmtType::block)return block_exp->dumpIR();
        else if (type == SimpleStmtType::break_)
        {
            assert(!while_stack.empty());
            int while_no = while_stack.back();
            std::string end_label = "\%while_end_" + std::to_string(while_no);
            std::cout << "\tjump " << end_label << std::endl;
            return "break";
        }
        else if (type == SimpleStmtType::continue_)
        {
            assert(!while_stack.empty());
            int while_no = while_stack.back();
            std::string entry_label = "\%while_" + std::to_string(while_no);
            std::cout << "\tjump " << entry_label << std::endl;
            return "cont";
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
        std::cout << " } ";
    }
    std::string dumpIR() const override
    {
        return l_or_exp->dumpIR();
    }
    virtual int dumpExp() const override
    {
        return l_or_exp->dumpExp();
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
    virtual int dumpExp() const override
    {
        int result = 1;
        if (op == "")result = l_and_exp->dumpExp();
        else if (op == "||")
        {
            int left_result = l_or_exp->dumpExp();
            if (left_result)return 1;
            result = l_and_exp->dumpExp() != 0;
        }
        else assert(false);
        return result;
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
    virtual int dumpExp() const override
    {
        int result = 0;
        if (op == "")result = eq_exp->dumpExp();
        else if (op == "&&")
        {
            int left_result = l_and_exp->dumpExp();
            if (left_result == 0)return 0;
            result = eq_exp->dumpExp() != 0;
        }
        else assert(false);
        return result;
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
    virtual int dumpExp() const override
    {
        int result = 0;
        if (op == "")result = rel_exp->dumpExp();
        else
        {
            int left_result = eq_exp->dumpExp();
            int right_result = rel_exp->dumpExp();
            if (op == "==")result = left_result == right_result;
            else if (op == "!=")result = left_result != right_result;
            else assert(false);
        }
        return result;
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
    virtual int dumpExp() const override
    {
        int result = 0;
        if (op == "")result = add_exp->dumpExp();
        else
        {
            int left_result = rel_exp->dumpExp();
            int right_result = add_exp->dumpExp();
            if (op == ">")result = left_result > right_result;
            else if (op == ">=")result = left_result >= right_result;
            else if (op == "<")result = left_result < right_result;
            else if (op == "<=")result = left_result <= right_result;
            else assert(false);
        }
        return result;
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
    virtual int dumpExp() const override
    {
        int result = 0;
        if (op == "")result = mul_exp->dumpExp();
        else
        {
            int left_result = add_exp->dumpExp();
            int right_result = mul_exp->dumpExp();
            if (op == "+")result = left_result + right_result;
            else if (op == "-")result = left_result - right_result;
            else assert(false);
        }
        return result;
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
    virtual int dumpExp() const override
    {
        int result = 0;
        if (op == "")result = unary_exp->dumpExp();
        else
        {
            int left_result = mul_exp->dumpExp();
            int right_result = unary_exp->dumpExp();
            if (op == "*")result = left_result * right_result;
            else if (op == "/")result = left_result / right_result;
            else if (op == "%")result = left_result % right_result;
            else assert(false);
        }
        return result;
    }
};


class UnaryExpAST : public BaseAST
{
public:
    UnaryExpType type;
    std::string op;
    std::unique_ptr<BaseAST> exp;
    std::string ident;
    std::vector<std::unique_ptr<BaseAST>> params;
    void dump() const override
    {
        if (type == UnaryExpType::func_call)
        {
            std::cout << ident << "(";
            for (int i = 0; i < params.size(); i++)
            {
                params[i]->dump();
                if (i != params.size() - 1)std::cout << ", ";
            }
            return;
        }
        if (type == UnaryExpType::unary)std::cout << op;
        exp->dump();
    }
    std::string dumpIR() const override
    {
        if (type == UnaryExpType::primary)return exp->dumpIR();
        else if (type == UnaryExpType::unary)
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
        else if (type == UnaryExpType::func_call)
        {
            std::vector<std::string> param_vars;
            for (auto&& param : params)
                param_vars.push_back(param->dumpIR());
            assert(function_table.count(ident));
            assert(function_param_num[ident] == params.size());
            std::string result_var = "";
            if (function_ret_type[ident] == "int")
                result_var = "%" + std::to_string(symbol_num++);
            std::string name = function_table[ident];
            std::cout << '\t';
            if (function_ret_type[ident] == "int")
                std::cout << result_var << " = ";
            std::cout << "call " << name << "(";
            for (int i = 0; i < param_vars.size(); i++)
            {
                std::cout << param_vars[i];
                if (i != param_vars.size() - 1)std::cout << ", ";
            }
            std::cout << ")" << std::endl;
            return result_var;
        }
        else assert(false);
        return "";
    }
    virtual int dumpExp() const override
    {
        int result = 0;
        if (type == UnaryExpType::primary)result = exp->dumpExp();
        else if (type == UnaryExpType::unary)
        {
            int tmp = exp->dumpExp();
            if (op == "+")result = tmp;
            else if (op == "-")result = -tmp;
            else if (op == "!")result = !tmp;
            else assert(false);
        }
        else assert(false);
        return result;
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
        else if (type == PrimaryExpType::lval)std::cout << lval;
        else assert(false);
    }
    std::string dumpIR() const override
    {
        std::string result_var = "";
        if (type == PrimaryExpType::exp)result_var = exp->dumpIR();
        else if (type == PrimaryExpType::number)
            result_var = std::to_string(number);
        else if (type == PrimaryExpType::lval)
        {
            std::variant<int, std::string> value = look_up_symbol_tables(lval);
            if (value.index() == 0)
                result_var = std::to_string(std::get<int>(value));
            else
            {
                result_var = "%" + std::to_string(symbol_num++);
                std::cout << '\t' << result_var << " = load " <<
                    std::get<std::string>(value) << std::endl;
            }
        }
        else assert(false);
        return result_var;
    }
    virtual int dumpExp() const override
    {
        int result = 0;
        if (type == PrimaryExpType::exp)result = exp->dumpExp();
        else if (type == PrimaryExpType::number)result = number;
        else if (type == PrimaryExpType::lval)
        {
            std::variant<int, std::string> value = look_up_symbol_tables(lval);
            assert(value.index() == 0);
            result = std::get<int>(value);
        }
        else assert(false);
        return result;
    }
};


class DeclAST : public BaseAST
{
public:
    DeclType type;
    std::unique_ptr<BaseAST> decl;
    void dump() const override { decl->dump(); }
    std::string dumpIR() const override { return decl->dumpIR(); }
    int dumpExp() const override { return decl->dumpExp(); }
};


class ConstDeclAST : public BaseAST
{
public:
    std::string b_type;
    std::vector<std::unique_ptr<BaseAST>> const_def_list;
    void dump() const override
    {
        assert(b_type == "int");
        for (auto&& const_def : const_def_list)const_def->dump();
    }
    std::string dumpIR() const override
    {
        assert(b_type == "int");
        for (auto&& const_def : const_def_list)const_def->dumpIR();
        return "";
    }
    int dumpExp() const override { dumpIR(); return 0; }
};


class ConstDefAST : public BaseAST
{
public:
    std::string ident;
    std::unique_ptr<BaseAST> const_init_val;
    void dump() const override
    {
        std::cout << "ConstDefAST{" << ident << "=";
        const_init_val->dump();
        std::cout << "} ";
    }
    std::string dumpIR() const override
    {
        symbol_tables.back()[ident] = std::stoi(const_init_val->dumpIR());
        return "";
    }
};


class ConstInitValAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> const_exp;
    void dump() const override { std::cout << const_exp->dumpExp(); }
    std::string dumpIR() const override
    {
        return std::to_string(const_exp->dumpExp());
    }
};


class BlockItemAST : public BaseAST
{
public:
    BlockItemType type;
    std::unique_ptr<BaseAST> content;
    void dump() const override { content->dump(); }
    std::string dumpIR() const override { return content->dumpIR(); }
};


class ConstExpAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> exp;
    void dump() const override { std::cout << exp->dumpExp(); }
    std::string dumpIR() const override
    {
        return std::to_string(exp->dumpExp());
    }
    virtual int dumpExp() const override { return exp->dumpExp(); }
};


class VarDeclAST : public BaseAST
{
public:
    std::string b_type;
    std::vector<std::unique_ptr<BaseAST>> var_def_list;
    void dump() const override
    {
        assert(b_type == "int");
        for (auto&& var_def : var_def_list)var_def->dump();
    }
    std::string dumpIR() const override
    {
        assert(b_type == "int");
        for (auto&& var_def : var_def_list)var_def->dumpIR();
        return "";
    }
    int dumpExp() const override
    {
        assert(b_type == "int");
        for (auto&& var_def : var_def_list)var_def->dumpExp();
        return 0;
    }
};


class VarDefAST : public BaseAST
{
public:
    std::string ident;
    bool has_init_val;
    std::unique_ptr<BaseAST> init_val;
    void dump() const override
    {
        std::cout << "VarDefAST{" << ident;
        if (has_init_val)
        {
            std::cout << "=";
            init_val->dump();
        }
        std::cout << "} ";
    }
    std::string dumpIR() const override
    {
        std::string var_name = "@" + ident;
        std::string name = var_name + "_" +
            std::to_string(var_num[var_name]++);
        std::cout << '\t' << name << " = alloc i32" << std::endl;
        symbol_tables.back()[ident] = name;
        if (has_init_val)
        {
            std::string val_var = init_val->dumpIR();
            std::cout << "\tstore " << val_var << ", " << name << std::endl;
        }
        return "";
    }
    int dumpExp() const override
    {
        std::string var_name = "@" + ident;
        std::string name = var_name + "_" +
            std::to_string(var_num[var_name]++);
        symbol_tables.back()[ident] = name;
        if (has_init_val)
        {
            std::string val_var = init_val->dumpIR();
            std::cout << "global " << name << " = alloc i32, ";
            if (val_var[0] == '@' || val_var[0] == '%')assert(false);
            else if (val_var != "0")std::cout << val_var << std::endl;
            else std::cout << "zeroinit" << std::endl;
        }
        else
            std::cout << "global " << name << " = alloc i32, zeroinit" <<
                std::endl;
        return 0;
    }
};


class InitValAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> exp;
    void dump() const override { exp->dump(); }
    std::string dumpIR() const override { return exp->dumpIR(); }
};
