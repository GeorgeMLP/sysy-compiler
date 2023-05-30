#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <memory>


class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void dump() const = 0;
    virtual void dumpIR() const = 0;
};


class CompUnitAST : public BaseAST
{
public:
    std::unique_ptr<BaseAST> func_def;
    void dump() const override
    {
        std::cout << "CompUnitAST { ";
        func_def->dump();
        std::cout << " }";
    }
    void dumpIR() const override
    {
        func_def->dumpIR();
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
        std::cout << " }";
    }
    void dumpIR() const override
    {
        std::cout << "fun @" << ident << "(): ";
        func_type->dumpIR();
        std::cout << " {" << std::endl << "\%entry:" << std::endl;
        block->dumpIR();
        std::cout << "}";
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
        std::cout << "}";
    }
    void dumpIR() const override
    {
        if (func_type_name == "int")std::cout << "i32";
        else std::cout << "ERROR";
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
        std::cout << " }";
    }
    void dumpIR() const override
    {
        stmt->dumpIR();
    }
};


class StmtAST: public BaseAST
{
public:
    std::string stmt;
    void dump() const override
    {
        std::cout << "StmtAST { ";
        std::cout << stmt;
        std::cout << " }";
    }
    void dumpIR() const override
    {
        std::cout << "\tret 0" << std::endl;
    }
};
