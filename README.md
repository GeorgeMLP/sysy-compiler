# SysY Compiler

Compiles SysY into RISC-V. Below is my Chinese version lab report.

PKU Compiler Principles course documents: [PKU Compiler Principles](https://pku-minic.github.io/online-doc/#/).

PKU Compiler Principles development environment: [compiler-dev](https://github.com/pku-minic/compiler-dev).

## 一、编译器概述

### 1.1 基本功能

本编译器基本具备如下功能：

1. 将 SysY 语言程序编译为 Koopa IR。
2. 将 Koopa IR 编译为 RISC-V 指令。
3. 实现 RISC-V 程序的寄存器分配以优化性能。

### 1.2 主要特点

我开发的编译器的主要特点是实现简单、正确性高，并且进行了部分性能优化。编译过程完全利用嵌套的 AST 数据结构递归实现，只需一次遍历，代码简洁；通过了性能测试的所有测试用例以及 Koopa IR 和 RISC-V 的几乎全部测试用例，正确性较高；使用分级的寄存器分配策略，提高了性能。

## 二、编译器设计

### 2.1 主要模块组成

编译器由 3 个主要模块组成：```sysy.l``` 和 ```sysy.y``` 负责词法和语法分析，```AST.h``` 负责将 SysY 源代码编译成 Koopa IR，```RISCV.h``` 负责将 Koopa IR 编译成 RISC-V 指令。

### 2.2 主要数据结构

本编译器最核心的数据结构是 AST 树。如果将一个 SysY 程序视作一棵树，那么一个 ```class CompUnitAST``` 的实例就是这棵树的根，其数据成员包括所有函数定义和全局变量定义。函数定义用数据结构 ```class FuncDefAST``` 表示，数据成员包括其返回类型、标识符、参数指针和函数块指针；常量声明和定义分别用 ```class ConstDeclAST``` 和 ```class ConstDefAST``` 表示，变量声明和定义分别用 ```class VarDeclAST``` 和 ```class VarDefAST``` 表示。表示语句块、语句和表达式的数据结构分别为 ```class BlockAST```、```class StmtAST``` 和 ```class ExpAST```，而它们又可以根据语法规则衍生出很多其他的 AST 数据结构。

所有 AST 类都是基类 ```class BaseAST``` 的衍生类，其定义为：

```cpp
class BaseAST
{
public:
    virtual ~BaseAST() = default;
    virtual void dump() const = 0;
    virtual std::string dumpIR() const = 0;
    virtual int dumpExp() const { assert(false); return -1; }
    virtual std::string get_ident() const { assert(false); return ""; }
    virtual std::string get_type() const { assert(false); return ""; }
    virtual int get_dim() const { assert(false); return -1; }
    virtual std::vector<int> dumpList(std::vector<int>) const { exit(1); }
};
```

其中 ```dump()``` 递归地输出语法树结构，可以用来 debug；```dumpIR()``` 递归地生成中间代码；```dumpExp()``` 用于编译期生成代码，主要包括常量表达式的计算以及全局数组初始化等；```dumpList()``` 用于计算数组的初始值，将不完整的初始化列表补全；以及其他的一些辅助函数。

我们用一个 ```vector``` 来表示当前活跃的符号表，它是由若干符号表构成的 ```vector```：

```cpp
static std::vector<std::map<std::string, std::variant<int, std::string>>>
    symbol_tables;
```

每一个符号表都将当前语句块的一个变量（用字符串表示）映射到它的名字或数值。第一个符号表为全局符号表，它永远都是活跃的。此外我们维护一些额外信息：

```cpp
static std::map<std::string, int> var_num;
static std::map<std::string, int> is_list;
static std::map<std::string, int> is_func_param;
static std::map<std::string, int> list_dim;
```

以便于中间代码的生成。

对于函数，我们维护一个函数列表，并记录关于这些函数的额外信息，以便于语义分析：

```cpp
static std::map<std::string, std::string> function_table;
static std::map<std::string, std::string> function_ret_type;
static std::map<std::string, int> function_param_num;
static std::map<std::string, std::vector<std::string>> function_param_idents;
static std::map<std::string, std::vector<std::string>> function_param_names;
static std::map<std::string, std::vector<std::string>> function_param_types;
```

这些数据结构所记录的信息就如它们的名字所示。

### 2.3 主要设计考虑及算法选择

#### 2.3.1 符号表的设计考虑

我的符号表数据结构是用一个 ```vector``` 表示的，每个元素都是一个符号表，对应于语句块（```class BlockAST```）的嵌套关系，第一个元素为全局符号表，最后一个元素为当前所在块的符号表。这样当我们退出一个语句块时，只需要从 ```vector``` 中弹出一个元素；而进入一个语句块时，只需要向 ```vector``` 中加入一个元素。

#### 2.3.2 寄存器分配策略

我采用的是较为朴素的寄存器分配策略。每个寄存器有一个 ```reg_stats``` 值，可能为 ```0```、```1``` 或 ```2```。```0``` 表示当前寄存器中没有值或这个值不重要（即不用被保存回栈），```1``` 表示当前值在之后还可能被用到；```2``` 表示当前值不能被替换。搜索寄存器时，我们首先看有没有 ```reg_stats``` 为 ```0``` 的寄存器，若有则直接返回该寄存器；否则我们就找到一个 ```reg_stats``` 为 ```1``` 的寄存器，将其原来的值保存回栈，并作为分配的寄存器。```reg_stats``` 为 ```2``` 的寄存器是被保护的，它不能被换出，说明它在当前语句的处理中还会用到；也正因如此我们需要在 ```reg_stats``` 为 ```2``` 的寄存器被使用后将它的 ```reg_stats``` 还原为 ```1```，以避免无寄存器可用的情况。

#### 2.3.3 采用的优化策略

我进行了一些简单的优化，如维护一个活跃值表，当加载一个值时，如果它是活跃的，那么就不用去栈上加载，直接返回它所在的寄存器；控制流无法到达的语句不用生成（如：```ret``` 后的语句；基本块内 ```break``` 或 ```continue``` 后的语句；以及形如 ```if (...)ret; else ret;``` 或 ```if (...)break; else ret;``` 之类的语句后的语句）；和 0 相加或相乘的代码不用生成，直接计算出结果；没有作用的代码不用生成（如将一个寄存器的值保存或移动到它自己）；用一个特殊寄存器（```s11```）作为临时值寄存器，以避免频繁的临时寄存器调度等。

#### 2.3.4 其它补充设计考虑

我将相似的 AST 数据结构用一个类实现，用一个枚举类区分其类型，使代码更简洁；各个语法树元素之间尽可能通过递归的方式互相传递信息，一些关键信息如符号表或函数表则用全局变量记录；所有 AST 类都继承至 ```BaseAST```，共用同名的成员函数以实现多态。

## 三、编译器实现

### 3.1 各阶段编码细节

#### Lv1. main 函数和 Lv2. 初试目标代码生成

这部分比较简单，中间代码生成部分就按照 AST 树的结构递归处理即可；目标代码生成部分也只需要处理两种 ```value```：```ret``` 和 ```integer```。

```cpp
void visit(const koopa_raw_return_t &ret)
{
    koopa_raw_value_t ret_value = ret.value;
    visit(ret_value);
    std::cout << "\tret" << std::endl;
}


void visit(const koopa_raw_integer_t &integer)
{
    int32_t int_val = integer.value;
    std::cout << "\tli a0," << int_val << std::endl;
    return;
}
```

#### Lv3. 表达式

这部分的主要内容是利用 AST 的层次结构处理优先级，优先级较低的运算在语法树中层次较高。语法树从上到下依次为：

```cpp
class ExpAST : public BaseAST;
class LOrExpAST : public BaseAST;
class LAndExpAST : public BaseAST;
class EqExpAST : public BaseAST;
class RelExpAST : public BaseAST;
class AddExpAST : public BaseAST;
class MulExpAST : public BaseAST;
class UnaryExpAST : public BaseAST;
class PrimaryExpAST : public BaseAST;
```

每一级 AST 都有下一级 AST 作为其数据成员，这样对语法树递归处理时便能正确实现优先级。

#### Lv4. 常量和变量

本章添加 ```class DeclAST``` 用于表示声明，```class ConstDeclAST```、```class ConstDefAST``` 和 ```class ConstInitValAST``` 用于表示常量声明、定义和初始值，```class VarDeclAST```、```class VarDefAST``` 和 ```class InitValAST``` 用于表示变量声明、定义和初始值。```block``` 可以包含多个语句，我们用一个 ```vector``` 来表示；并且 ```SimpleStmt``` 和 ```PrimaryStmt``` 中也可以包含 ```LVal``` 了，为此我们需要实现一个符号表。如前所述，符号表被组织成一个栈，在进入 ```block``` 时加入一个符号表、在退出 ```block``` 时弹出一个符号表。

```cpp
class VarDefAST : public BaseAST
{
public:
    std::string ident;
    // ...
    std::string dumpIR() const override
    {
        std::string var_name = "@" + ident;
        std::string name = var_name + "_" +
            std::to_string(var_num[var_name]++);
        std::cout << '\t' << name << " = alloc i32" << std::endl;
        symbol_tables.back()[ident] = name;
        // ...
    }
};
```

常量并不需要保存至符号表，我们需要在编译器将其值求出来，为此我们实现 ```dumpExp()``` 函数来编译器对表达式求值。实际上我的代码中常量还是在 ```symbol_table``` 中的，只不过它的值不是字符串而是数字（用 ```std::variant<int, std::string>``` 实现）。

在生成目标代码时维护一个 ```value_map```，若所求值已经存在则不需要再次求值，直接加载即可。

```cpp
Reg Visit(const koopa_raw_value_t &value)
{
    koopa_raw_value_t old_value = present_value;
    present_value = value;
    if (value_map.count(value))
    {
        if (value_map[value].reg_name == -1)
        {
            int reg_name = find_reg(1);
            value_map[value].reg_name = reg_name;
            std::cout << "\tlw    " << reg_names[reg_name] << ", " <<
                value_map[value].reg_offset << "(sp)" << std::endl;
        }
        present_value = old_value;
        return value_map[value];
    }
}
```

#### Lv5. 语句块和作用域

本章的改动在于 ```block``` 也可以作为 ```stmt```，并需要正确维护符号表。这体现在代码上主要是 ```SimpleStmtAST``` 的数据成员有可能是 ```block``` 也有可能是 ```exp```，但由于多态性，这并没有什么影响。

```cpp
class SimpleStmtAST : public BaseAST
{
public:
    SimpleStmtType type;
    std::string lval;
    std::unique_ptr<BaseAST> block_exp;
    // ...
    std::string dumpIR() const override
    {
        if (type == SimpleStmtType::ret)
        {
            // ...
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
        else assert(false);
        return "";
    }
};
```

#### Lv6. if 语句

我们利用下述文法处理 if/else 的二义性：

```bison
Stmt : OpenStmt
     | ClosedStmt;
ClosedStmt : SimpleStmt
           | IF '(' Exp ')' ClosedStmt ELSE ClosedStmt;
OpenStmt : IF '(' Exp ')' Stmt
         | IF '(' Exp ')' ClosedStmt ELSE OpenStmt;
```

为了实现短路求值，我们需要更改 ```LOrExpAST``` 和 ```LAndExpAST``` 的代码。这里以 ```LOrExpAST``` 为例：

```cpp
class LOrExpAST : public BaseAST
{
public:
    std::string op;
    std::unique_ptr<BaseAST> l_and_exp;
    std::unique_ptr<BaseAST> l_or_exp;
    // ...
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
```

目标代码生成只需要额外考虑 ```br``` 和 ```jump``` 指令，以 ```br``` 为例：

```cpp
void Visit(const koopa_raw_branch_t &branch)
{
    std::string true_label = branch.true_bb->name + 1;
    std::string false_label = branch.false_bb->name + 1;
    int cond_reg = Visit(branch.cond).reg_name;
    std::cout << "\tbnez  " << reg_names[cond_reg] << ", " << true_label
        << std::endl;
    std::cout << "\tj     " << false_label << std::endl;
}
```

#### Lv7. while 语句

本章一个是实现 ```while```，一个是实现 ```break``` 和 ```continue```。实现 ```while``` 只需在 ```StmtAST``` 结构中新加一种 ```type``` 即可，代码如下：

```cpp
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
```

实现 ```while``` 和 ```break``` 也只需在 ```SimpleStmtAST``` 中新加入两种 ```type``` 即可。我们维护一个 ```while_stack``` 用于保存 ```while``` 语句块的 ```label```，这样在生成 ```break``` 和 ```continue``` 时就可以得到跳转地址了。以 ```break``` 为例：

```cpp
else if (type == SimpleStmtType::break_)
{
    assert(!while_stack.empty());
    int while_no = while_stack.back();
    std::string end_label = "\%while_end_" + std::to_string(while_no);
    std::cout << "\tjump " << end_label << std::endl;
    return "break";
}
```

需要注意的一点是同一个基本块内 ```ret```、```break``` 以及 ```continue``` 后的语句都不会被到达，不必为它们生成代码；形如 ```if (...)ret; else ret;``` 或者 ```if (...)break; else break;``` 之类的语句也是如此。

#### Lv8. 函数和全局变量

首先是函数。函数我们要处理两个事情，一个是函数的调用，一个是函数参数的保存。中间代码生成时，函数调用只需在 ```UnaryExpAST``` 中加一种 ```type```：

```cpp
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
```

而函数开头要把函数的相关信息保存在相关数据结构中，在 ```block``` 生成中间代码时先将函数参数保存在栈上。目标代码生成时，函数调用前要把参数移至相应位置：

```cpp
Reg Visit(const koopa_raw_call_t &call)
{
    struct Reg result_var = { 7, -1 };
    clear_registers();
    for (size_t i = 0; i < call.args.len; i++)
    {
        auto ptr = call.args.buffer[i];
        koopa_raw_value_t arg = reinterpret_cast<koopa_raw_value_t>(ptr);
        struct Reg arg_var = Visit(arg);
        assert(arg_var.reg_name >= 0);
        if (i < 8)
        {
            if (arg_var.reg_name != i + 7)
                std::cout << "\tmv    " << reg_names[i + 7] << ", " <<
                    reg_names[arg_var.reg_name] << std::endl;
        }
        else
            std::cout << "\tsw    " << reg_names[arg_var.reg_name] << ", " <<
                (i - 8) * 4 << "(sp)" << std::endl;
    }
    std::cout << "\tcall  " << call.callee->name + 1 << std::endl;
    return result_var;
}
```

而函数开头要计算栈的偏移量，并视情况减少栈指针和保存 ```ra``` 寄存器。函数调用前要保存所有 ```reg_stats``` 大于 ```0``` 的寄存器，函数调用后要假设所有调用者保存的寄存器的值都已经被重写。

SysY 库函数比较简单，在代码开头先定义好这些函数就行了。

至于全局变量，它如果有初始值的话一定是编译期可求的，我们不能仍然像局部变量一样用 ```dumpIR()``` 生成代码，而是另外实现 ```dumpExp()``` 以在全局分配变量和指定其初始值。在目标代码生成则需额外考虑一种 ```global``` 类型。

```cpp
std::string Visit(const koopa_raw_global_alloc_t &global)
{
    std::string name = "var_" + std::to_string(global_num++);
    std::cout << "\t.data" << std::endl;
    std::cout << "\t.globl " << name << std::endl;
    std::cout << name << ":" << std::endl;
    switch (global.init->kind.tag)
    {
    case KOOPA_RVT_ZERO_INIT:
        std::cout << "\t.zero 4" << std::endl << std::endl;
        break;
    case KOOPA_RVT_INTEGER:
        std::cout << "\t.word " << global.init->kind.data.integer.value <<
            std::endl << std::endl;
        break;
    default:
        assert(false);
    }
    return name;
}
```

#### Lv9. 数组

数组的一大难点在于多维数组的初始化。为此我实现了一系列的成员函数来递归地实现多维数组的处理。首先是 ```dumpListType```，递归地打印出数组类型：

```cpp
void dumpListType(std::vector<int> widths) const
{
    if (widths.size() == 1)
    {
        std::cout << "[i32, " << widths[0] << "]"; return;
    }
    std::vector<int> rec = std::vector<int>(widths.begin() + 1,
        widths.end());
    std::cout << "["; dumpListType(rec);
    std::cout << ", " << widths.front() << "]";
}
```

函数 ```dumpList()``` 递归地将数组的初始值补全：

```cpp
std::vector<int> dumpList(std::vector<int> widths) const override
{
    std::vector<int> ret;
    if (widths.size() == 1)
    {
        for (auto&& const_init_val : const_init_val_list)
        {
            assert(const_init_val->get_ident() == "const_exp");
            ret.push_back(std::stoi(const_init_val->dumpIR()));
            const_list_num++;
        }
        int num_zeros = widths[0] - ret.size();
        for (int i = 0; i < num_zeros; i++)
        {
            ret.push_back(0); const_list_num++;
        }
        return ret;
    }
    std::vector<int> products = widths;
    for (int i = products.size() - 2; i >= 0; i--)
        products[i] *= products[i + 1];
    int total_size = products[0];
    for (auto&& const_init_val : const_init_val_list)
        if (const_init_val->get_ident() == "const_exp")
        {
            ret.push_back(std::stoi(const_init_val->dumpIR()));
            const_list_num++; continue;
        }
        else if (const_init_val->get_ident() == "list")
        {
            int init_num;
            for (init_num = 1; init_num < widths.size(); init_num++)
                if (const_list_num % products[init_num] == 0)break;
                else if (init_num == widths.size() - 1)assert(false);
            std::vector<int> rec = std::vector<int>(widths.begin() +
                init_num, widths.end());
            std::vector<int> tmp = const_init_val->dumpList(rec);
            ret.insert(ret.end(), tmp.begin(), tmp.end());
        }
        else assert(false);
    int num_zeros = total_size - ret.size();
    for (int i = 0; i < num_zeros; i++)
    {
        ret.push_back(0); const_list_num++;
    }
    return ret;
}
```

函数 ```dumpListInit()``` 用于递归地实现局部数组的初始化：

```cpp
void dumpListInit(std::string prev, std::vector<int> widths, int depth,
    std::vector<int> init_list) const
{
    if (depth >= widths.size())
    {
        std::cout << "\tstore " << init_list[const_list_num++] << ", " <<
            prev << std::endl;
        return;
    }
    for (int i = 0; i < widths[depth]; i++)
    {
        std::string result_var = "%" + std::to_string(symbol_num++);
        std::cout << '\t' << result_var << " = getelemptr " << prev << ", "
            << i << std::endl;
        dumpListInit(result_var, widths, depth + 1, init_list);
    }
}
```

函数 ```printInitList()``` 递归地打印出数组的初始值：

```cpp
void printInitList(std::vector<int> widths, int depth,
    std::vector<int> init_list) const
{
    if (depth >= widths.size())
    {
        std::cout << init_list[const_list_num++]; return;
    }
    std::cout << "{";
    for (int i = 0; i < widths[depth]; i++)
    {
        printInitList(widths, depth + 1, init_list);
        if (i != widths[depth] - 1)std::cout << ", ";
    }
    std::cout << "}";
}
```

当数组作为左值的时候，我们要根据其是否是函数参数来选择使用 ```getptr``` 还是 ```getelemptr```：

```cpp
else if (type == SimpleStmtType::list)
{
    std::variant<int, std::string> value = look_up_symbol_tables(lval);
    assert(value.index() == 1);
    std::string result_var, name, prev = std::get<std::string>(value);
    assert(list_dim[prev] == exp_list.size());
    for (auto&& exp : exp_list)
    {
        result_var = exp->dumpIR();
        name = "%" + std::to_string(symbol_num++);
        if (is_func_param[prev])
        {
            std::cout << '\t' << name << " = load " << prev <<
                std::endl;
            std::string tmp = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << tmp << " = getptr " << name << ", "
                << result_var << std::endl;
            name = tmp;
        }
        else
            std::cout << '\t' << name << " = getelemptr " << prev <<
                ", " << result_var << std::endl;
        prev = name;
    }
    result_var = block_exp->dumpIR();
    std::cout << "\tstore " << result_var << ", " << prev << std::endl;
}
```

当数组作为右值的时候，我们也要根据其是否是函数参数、是指针还是整数（例如，对于数组 ```arr[2][2]```，```arr[1]``` 是指针，```arr[1][1]``` 是整数），选择适当的指令：

```cpp
else if (type == PrimaryExpType::list)
{
    std::variant<int, std::string> value = look_up_symbol_tables(lval);
    assert(value.index() == 1);
    std::string name, prev = std::get<std::string>(value);
    int dim = list_dim[prev];
    bool list = is_list[prev], func_param = is_func_param[prev];
    for (auto&& exp : exp_list)
    {
        result_var = exp->dumpIR();
        name = "%" + std::to_string(symbol_num++);
        if (is_func_param[prev])
        {
            std::cout << '\t' << name << " = load " << prev <<
                std::endl;
            std::string tmp = "%" + std::to_string(symbol_num++);
            std::cout << '\t' << tmp << " = getptr " << name <<
                ", " << result_var << std::endl;
            name = tmp;
        }
        else
            std::cout << '\t' << name << " = getelemptr " << prev <<
                ", " << result_var << std::endl;
        prev = name;
    }
    if (exp_list.size() == dim)
    {
        result_var = "%" + std::to_string(symbol_num++);
        std::cout << '\t' << result_var << " = load " << prev <<
            std::endl;
    }
    else if (list || func_param)
    {
        result_var = "%" + std::to_string(symbol_num++);
        std::cout << '\t' << result_var << " = getelemptr " << prev <<
            ", 0" << std::endl;
    }
    else result_var = name;
}
```

此外需要注意的是，数组名字也是可以被作为参数被单独传入函数的（例如 ```call f(arr)```），因此对 ```lval``` 的处理也要适当修改，判断其是否是数组（维护一个 ```is_list[]```）：

```cpp
else if (type == PrimaryExpType::lval)
{
    std::variant<int, std::string> value = look_up_symbol_tables(lval);
    if (value.index() == 0)
        result_var = std::to_string(std::get<int>(value));
    else if (is_list[std::get<std::string>(value)])
    {
        result_var = "%" + std::to_string(symbol_num++);
        std::cout << '\t' << result_var << " = getelemptr " <<
            std::get<std::string>(value) << ", 0" << std::endl;
    }
    else
    {
        result_var = "%" + std::to_string(symbol_num++);
        std::cout << '\t' << result_var << " = load " <<
            std::get<std::string>(value) << std::endl;
    }
}
```

目标代码生成部分，我们首先实现 ```cal_size()``` 函数来计算数组类型的大小，以便计算函数所需要的栈空间（注意数组作为函数参数的话只需要 4 字节的空间就够了，毕竟传进来的只是个指针）：

```cpp
int cal_size(const koopa_raw_type_t &ty)
{
    assert(ty->tag != KOOPA_RTT_UNIT);
    if (ty->tag == KOOPA_RTT_ARRAY)
    {
        int prev = cal_size(ty->data.array.base);
        int len = ty->data.array.len;
        return len * prev;
    }
    return 4;
}
```

然后我们分别实现对 ```getelemptr``` 和 ```getptr``` 的处理：

```cpp
Reg Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr)
{
    if (get_elem_ptr.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
    {
        assert(global_values.count(get_elem_ptr.src));
        struct Reg result_var = {find_reg(2), -1};
        koopa_raw_type_t arr = get_elem_ptr.src->ty->data.pointer.base;
        int total_size = cal_size(arr), len = arr->data.array.len;
        assert(total_size % len == 0);
        int elem_size = total_size / len;
        struct Reg ind_var = Visit(get_elem_ptr.index);
        int ind_reg = ind_var.reg_name;
        reg_stats[result_var.reg_name] = 1;
        std::cout << "\tla    " << reg_names[result_var.reg_name] << ", " <<
            global_values[get_elem_ptr.src] << std::endl;
        std::cout << "\tli    s11, " << elem_size << std::endl;
        std::cout << "\tmul   s11, s11, " << reg_names[ind_reg] << std::endl;
        std::cout << "\tadd   " << reg_names[result_var.reg_name] << ", " <<
            reg_names[result_var.reg_name] << ", s11" << std::endl;
        return result_var;
    }
    struct Reg src_var = value_map[get_elem_ptr.src];
    koopa_raw_type_t arr = get_elem_ptr.src->ty->data.pointer.base;
    struct Reg result_var = {find_reg(2), -1};
    int src_reg, src_old_stat;
    if (get_elem_ptr.src->name && get_elem_ptr.src->name[0] == '@')
    {
        int offset = src_var.reg_offset;
        assert(offset >= 0);  // variables have positive offset
        if (offset >= -2048 && offset <= 2047)
            std::cout << "\taddi  " << reg_names[result_var.reg_name] <<
                ", sp, " << offset << std::endl;
        else
        {
            std::cout << "\tli    s11, " << offset << std::endl;
            std::cout << "\tadd   " << reg_names[result_var.reg_name] <<
                ", sp, s11" << std::endl;
        }
    }
    else
    {
        src_var = Visit(get_elem_ptr.src);
        src_reg = src_var.reg_name;
        assert(src_reg >= 0);
        src_old_stat = reg_stats[src_reg];
        reg_stats[src_reg] = 2;
    }
    int total_size = cal_size(arr), len = arr->data.array.len;
    assert(total_size % len == 0);
    int elem_size = total_size / len;
    struct Reg ind_var = Visit(get_elem_ptr.index), tmp_var;
    if (elem_size != 0 && ind_var.reg_name != 15)
    {
        int ind_reg = ind_var.reg_name;
        int ind_old_stat = reg_stats[ind_reg];
        reg_stats[ind_reg] = 2;
        tmp_var = {find_reg(0), -1};
        reg_stats[ind_reg] = ind_old_stat;
        std::cout << "\tli    " << reg_names[tmp_var.reg_name] << ", " <<
            elem_size << std::endl;
        std::cout << "\tmul   " << reg_names[tmp_var.reg_name] << ", " <<
            reg_names[tmp_var.reg_name] << ", " << reg_names[ind_reg] <<
            std::endl;
    }
    else tmp_var = {15, -1};
    reg_stats[result_var.reg_name] = 1;
    if (get_elem_ptr.src->name && get_elem_ptr.src->name[0] == '@')
        std::cout << "\tadd   " << reg_names[result_var.reg_name] << ", " <<
            reg_names[result_var.reg_name] << ", " <<
            reg_names[tmp_var.reg_name] << std::endl;
    else
    {
        std::cout << "\tadd   " << reg_names[result_var.reg_name] << ", " <<
            reg_names[src_reg] << ", " << reg_names[tmp_var.reg_name]
            << std::endl;
        reg_stats[src_reg] = src_old_stat;
    }
    return result_var;
}


Reg Visit(const koopa_raw_get_ptr_t &get_ptr)
{
    struct Reg src_var = value_map[get_ptr.src];
    koopa_raw_type_t arr = get_ptr.src->ty->data.pointer.base;
    struct Reg result_var = {find_reg(2), -1};
    int elem_size = cal_size(arr);
    struct Reg ind_var = Visit(get_ptr.index), tmp_var;
    if (elem_size != 0 && ind_var.reg_name != 15)
    {
        int ind_reg = ind_var.reg_name;
        int ind_old_stat = reg_stats[ind_reg];
        reg_stats[ind_reg] = 2;
        tmp_var = {find_reg(0), -1};
        reg_stats[ind_reg] = ind_old_stat;
        std::cout << "\tli    " << reg_names[tmp_var.reg_name] << ", " <<
            elem_size << std::endl;
        std::cout << "\tmul   " << reg_names[tmp_var.reg_name] << ", " <<
            reg_names[tmp_var.reg_name] << ", " << reg_names[ind_reg] <<
            std::endl;
    }
    else tmp_var = {15, -1};
    reg_stats[result_var.reg_name] = 1;
    std::cout << "\tadd   " << reg_names[result_var.reg_name] << ", " <<
        reg_names[src_var.reg_name] << ", " <<
        reg_names[tmp_var.reg_name] << std::endl;
    return result_var;
}
```

最后 ```load```、```store```、```global``` 的处理也要略加改动，这里以 ```load``` 为例：

```cpp
else if (src->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
    src->kind.tag == KOOPA_RVT_GET_PTR)
{
    struct Reg result_var = {find_reg(2), -1};
    struct Reg src_var = Visit(load.src);
    reg_stats[result_var.reg_name] = 1;
    std::cout << "\tlw    " << reg_names[result_var.reg_name] << ", (" <<
        reg_names[src_var.reg_name] << ")" << std::endl;
    return result_var;
}
```

### 3.2 工具软件介绍

1. Flex/Bison: 进行词法分析和语法分析；
2. LibKoopa: 用于生成 Koopa IR 中间代码的结构，以便 RISC-V 目标代码的生成。
3. Git/Docker: 版本控制和运行环境。

### 3.3 测试情况说明

在课程文档中，对 SysY 和 Koopa IR 的符号名有如下规范：

- 变量/常量的名字可以是 `main`；

- SysY 程序声明的函数名不能和 SysY 库函数名相同；

- 局部变量名可以和函数名相同；

- 全局变量和局部变量的作用域可以重叠，局部变量会覆盖同名全局变量；

- SysY 标准中未指定函数形式参数应该被放入何种作用域；

- 在 Koopa IR 中，全局的符号不能和其他全局符号同名，局部的符号（位于函数内部的符号）不能和其他全局符号以及局部符号同名。上述规则对具名符号和临时符号都适用。

不过测试用例中似乎没有怎么测试这些规范。我在写代码时为了遵守这些规范，对中间代码中的变量名做了一些处理，以确保不会有重名的符号。另外一个容易踩的坑是，有的同学在实现 ```if```、```while``` 和逻辑表达式时，可能会用 ```then```、```end``` 之类的名字作为跳转的 label，而这些 label 也是有可能和变量重名的。

为了避免这些情况，一个可行的测试用例如下：

```c
int x = 3;

int f(int then)
{
    return x + then;
}

int main()
{
    int f = 2, x = 5;
    if (x > 3)f = 3;
    const int main[2] = {1, 2};
    return f(main[1] + x + f);
}
```

上述测试用例检测了局部变量和全局变量重名、局部变量和函数重名、常量/变量和 ```main``` 重名，以及变量和 label 重名的情况。值得一提的是在 perf 的某个测试用例中用到了 ```end``` 作为变量名，而我一开始并没有处理变量和 label 重名的情况，也用 ```end``` 作为 ```if``` 的跳转 label，导致目标代码编译不通过。

## 四、实习总结

### 4.1 收获和体会

体会到了写编译器的不容易。能踩的坑实在太多了！而且非常不方便 debug，毕竟生成出来的中间代码和目标代码都没有办法像我们在 IDE 中那样一步步运行，还能打印出某个时刻变量的值。因此设计好测试用例（以及设计更多的测试用例）是非常重要的，否则就会面对一长段 wrong answer 的代码而无从下手。

### 4.2 学习过程中的难点，以及对实习过程和内容的建议

我在数组参数那一部分遇到了很大的困难，被 Koopa IR 的各种类型搞得很晕。我觉得这部分的文档是有改进空间的，文档中只给出了两个对数组完全解引用到 ```i32``` 的例子，但对于数组部分解引用、数组参数部分解引用、以及数组用作函数参数、甚至对数组参数部分解引用然后再用作函数参数，都要自己纠结很长时间才能搞明白。个人认为文档中至少是可以给出一个数组部分解引用（即不是一直解引用到 ```i32```）的例子的。另外文档中也可以举例说明一下各种 Koopa 指令对数组类型的影响，例如 ```*[[i32, 3], 4]``` 被 ```getptr``` 作用后的类型为 ```*[[i32, 3], 4]```、被 ```getelemptr``` 作用后的类型为 ```*[i32, 3]```，```alloc``` 会使类型增加一个 ```*``` 号、```load``` 会使类型减少一个 ```*``` 号等。

### 4.3 对老师讲解内容与方式的建议

出于显而易见的原因，没有多少人会在编译 lab 中实现教材中提到的那些中间代码优化和目标代码优化，如图着色法分配寄存器、活跃变量分析、可用表达式分析、控制流图和到达定值等。但我其实对这部分的实现是比较感兴趣的，尤其是现在流行的那些编译器都用到了怎样的优化、它们是怎样实现的，我认为这些是可以考虑在实践课中介绍一下的。
