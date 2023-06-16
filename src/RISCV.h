#pragma once
#include <iostream>
#include <string>
#include <cassert>
#include <map>
#include "koopa.h"


struct Reg { int reg_name; int reg_offset; };
std::string reg_names[16] = {"t0", "t1", "t2", "t3", "t4", "t5", "t6",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "x0"};
koopa_raw_value_t registers[16];
int reg_stats[16] = {0};
koopa_raw_value_t present_value = 0;
std::map<const koopa_raw_value_t, Reg> value_map;
int stack_size = 0, stack_top = 0;
std::vector<int> stack_frame_sizes;
std::map<uintptr_t, int> stack_frame;


void Visit(const koopa_raw_program_t &program);
void Visit(const koopa_raw_slice_t &slice);
void Visit(const koopa_raw_function_t &func);
void Visit(const koopa_raw_basic_block_t &bb);
void Visit(const koopa_raw_return_t &ret);
Reg Visit(const koopa_raw_value_t &value);
Reg Visit(const koopa_raw_integer_t &integer);
Reg Visit(const koopa_raw_binary_t &binary);
Reg Visit(const koopa_raw_load_t &load);
void Visit(const koopa_raw_store_t &store);
void Visit(const koopa_raw_branch_t &branch);
void Visit(const koopa_raw_jump_t &jump);
int find_reg(int stat);
int cal_stack_size(const koopa_raw_slice_t &slice);


void Visit(const koopa_raw_program_t &program)
{
    std::cout << "\t.text" << std::endl;
    Visit(program.values);
    Visit(program.funcs);
}


void Visit(const koopa_raw_slice_t &slice)
{
    for (size_t i = 0; i < slice.len; i++)
    {
        auto ptr = slice.buffer[i];
        switch (slice.kind)
        {
        case KOOPA_RSIK_FUNCTION:
            Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
            break;
        case KOOPA_RSIK_BASIC_BLOCK:
            Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
            break;
        case KOOPA_RSIK_VALUE:
            Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
            break;
        default:
            assert(false);
        }
    }
}


void Visit(const koopa_raw_function_t &func)
{
    std::cout << "\t.globl " << (func->name + 1) << std::endl;
    std::cout << (func->name + 1) << ":" << std::endl;
    int stack_frame_size = 0;
    for (size_t i = 0; i < func->bbs.len; i++)
    {
        auto ptr = func->bbs.buffer[i];
        koopa_raw_basic_block_t bb =
            reinterpret_cast<koopa_raw_basic_block_t>(ptr);
        stack_frame_size += cal_stack_size(bb->insts);
    }
    stack_frame_sizes.push_back(stack_frame_size);
    stack_size += stack_frame_size * 4;
    if (stack_frame_size > 0 && stack_frame_size * 4 <= 2048)
        std::cout << "\taddi  sp, sp, -" << stack_frame_size * 4 << std::endl;
    else if (stack_frame_size * 4 > 2048)
    {
        struct Reg tmp_var = {-1, -1};
        tmp_var.reg_name = find_reg(0);
        std::cout << "\tli    " << reg_names[tmp_var.reg_name] << ", -" <<
            stack_frame_size * 4 << std::endl;
        std::cout << "\taddi  sp, sp, " << reg_names[tmp_var.reg_name] <<
            std::endl;
    }
    Visit(func->bbs);
    stack_size -= stack_frame_size * 4;
    stack_frame_sizes.pop_back();
}


void Visit(const koopa_raw_basic_block_t &bb)
{
    std::cout << bb->name + 1 << ":" << std::endl;
    Visit(bb->insts);
}


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

    const auto &kind = value->kind;
    struct Reg result_var = {-1, -1};
    switch (kind.tag)
    {
    case KOOPA_RVT_RETURN:
        Visit(kind.data.ret);
        break;
    case KOOPA_RVT_INTEGER:
        result_var = Visit(kind.data.integer);
        break;
    case KOOPA_RVT_BINARY:
        result_var = Visit(kind.data.binary);
        value_map[value] = result_var;
        break;
    case KOOPA_RVT_ALLOC:
        result_var.reg_offset = stack_top;
        stack_top += 4;
        value_map[value] = result_var;
        break;
    case KOOPA_RVT_LOAD:
        result_var = Visit(kind.data.load);
        value_map[value] = result_var;
        break;
    case KOOPA_RVT_STORE:
        Visit(kind.data.store);
        break;
    case KOOPA_RVT_BRANCH:
        Visit(kind.data.branch);
        break;
    case KOOPA_RVT_JUMP:
        Visit(kind.data.jump);
        break;
    default:
        assert(false);
    }
    present_value = old_value;
    return result_var;
}


void Visit(const koopa_raw_return_t &ret)
{
    koopa_raw_value_t ret_value = ret.value;
    assert(!stack_frame_sizes.empty());
    int stack_frame_size = stack_frame_sizes.back();
    if (stack_frame_size > 0 && stack_frame_size * 4 <= 2048)
        std::cout << "\taddi  sp, sp, " << stack_frame_size * 4 << std::endl;
    else if (stack_frame_size * 4 > 2048)
    {
        std::cout << "\tli    t0, " << stack_frame_size * 4 << std::endl;
        std::cout << "\taddi  sp, sp, t0" << std::endl;
    }
    struct Reg result_var = Visit(ret_value);
    std::cout << "\tmv    a0, " << reg_names[result_var.reg_name] << std::endl;
    std::cout << "\tret" << std::endl;
}


Reg Visit(const koopa_raw_integer_t &integer)
{
    int32_t int_val = integer.value;
    struct Reg result_var = {-1, -1};
    if (int_val == 0) { result_var.reg_name = 15; return result_var; }
    result_var.reg_name = find_reg(0);
    std::cout << "\tli    " << reg_names[result_var.reg_name] << ", " <<
        int_val << std::endl;
    return result_var;
}


Reg Visit(const koopa_raw_binary_t &binary)
{
    struct Reg left_val = Visit(binary.lhs);
    int left_reg = left_val.reg_name;
    int old_stat = reg_stats[left_reg];
    reg_stats[left_reg] = 2;
    struct Reg right_val = Visit(binary.rhs);
    int right_reg = right_val.reg_name;
    reg_stats[left_reg] = old_stat;
    struct Reg result_var = {find_reg(1), -1};
    std::string left_name = reg_names[left_reg];
    std::string right_name = reg_names[right_reg];
    std::string result_name = reg_names[result_var.reg_name];
    switch (binary.op)
    {
    case 0:  // ne
        if (right_name == "x0")
        {
            std::cout << "\tsnez  " << result_name << ", " << left_name <<
                std::endl;
            break;
        }
        if (left_name == "x0")
        {
            std::cout << "\tsnez  " << result_name << ", " << right_name <<
                std::endl;
            break;
        }
        std::cout << "\txor   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        std::cout << "\tsnez  " << result_name << ", " << result_name <<
            std::endl;
        break;
    case 1:  // eq
        if (right_name == "x0")
        {
            std::cout << "\tseqz  " << result_name << ", " << left_name <<
                std::endl;
            break;
        }
        if (left_name == "x0")
        {
            std::cout << "\tseqz  " << result_name << ", " << right_name <<
                std::endl;
            break;
        }
        std::cout << "\txor   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        std::cout << "\tseqz  " << result_name << ", " << result_name <<
            std::endl;
        break;
    case 2:  // gt
        std::cout << "\tsgt   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 3:  // lt
        std::cout << "\tslt   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 4:  // ge
        std::cout << "\tslt   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        std::cout << "\txori  " << result_name << ", " << result_name << ", 1"
            << std::endl;
        break;
    case 5:  // le
        std::cout << "\tsgt   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        std::cout << "\txori  " << result_name << ", " << result_name << ", 1"
            << std::endl;
        break;
    case 6:  // add
        std::cout << "\tadd   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 7:  // sub
        std::cout << "\tsub   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 8:  // mul
        std::cout << "\tmul   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 9:  // div
        std::cout << "\tdiv   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 10:  // mod
        std::cout << "\trem   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 11:  // and
        std::cout << "\tand   " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    case 12:  // or
        std::cout << "\tor    " << result_name << ", " << left_name << ", " <<
            right_name << std::endl;
        break;
    default:
        assert(false);
    }
    return result_var;
}


Reg Visit(const koopa_raw_load_t &load)
{
    koopa_raw_value_t src = load.src;
    int reg_name = find_reg(1), reg_offset = value_map[src].reg_offset;
    struct Reg result_var = {reg_name, reg_offset};
    if (reg_offset >= -2048 && reg_offset <= 2047)
        std::cout << "\tlw    " << reg_names[reg_name] << ", " << reg_offset <<
            "(sp)" << std::endl;
    else
    {
        struct Reg tmp_var = {-1, -1};
        tmp_var.reg_name = find_reg(0);
        std::cout << "\tli    " << reg_names[tmp_var.reg_name] << ", " <<
            reg_offset << std::endl;
        std::cout << "\tadd   " << reg_names[tmp_var.reg_name] << ", " <<
            reg_names[tmp_var.reg_name] << ", sp" << std::endl;
        std::cout << "\tlw    " << reg_names[reg_name] << ", (" <<
            reg_names[tmp_var.reg_name] << ")" << std::endl;
    }
    return result_var;
}


void Visit(const koopa_raw_store_t &store)
{
    struct Reg value = Visit(store.value);
    koopa_raw_value_t dest = store.dest;
    assert(value_map.count(dest));
    if (value_map[dest].reg_offset == -1)
    {
        value_map[dest].reg_offset = stack_top;
        stack_top += 4;
    }
    int reg_name = value.reg_name, reg_offset = value_map[dest].reg_offset;
    if (reg_offset >= -2048 && reg_offset <= 2047)
        std::cout << "\tsw    " << reg_names[reg_name] << ", " << reg_offset <<
            "(sp)" << std::endl;
    else
    {
        struct Reg tmp_var = {-1, -1};
        tmp_var.reg_name = find_reg(0);
        std::cout << "\tli    " << reg_names[tmp_var.reg_name] << ", " <<
            reg_offset << std::endl;
        std::cout << "\tadd   " << reg_names[tmp_var.reg_name] << ", " <<
            reg_names[tmp_var.reg_name] << ", sp" << std::endl;
        std::cout << "\tsw    " << reg_names[reg_name] << ", (" <<
            reg_names[tmp_var.reg_name] << ")" << std::endl;
    }
}


void Visit(const koopa_raw_branch_t &branch)
{
    std::string true_label = branch.true_bb->name + 1;
    std::string false_label = branch.false_bb->name + 1;
    int cond_reg = Visit(branch.cond).reg_name;
    std::cout << "\tbnez  " << reg_names[cond_reg] << ", " << true_label
        << std::endl;
    std::cout << "\tj     " << false_label << std::endl;
}


void Visit(const koopa_raw_jump_t &jump)
{
    std::string target_label = jump.target->name + 1;
    std::cout << "\tj     " << target_label << std::endl;
}


int find_reg(int stat)
{
    for (int i = 0; i < 15; i++)
        if (reg_stats[i] == 0)
        {
            registers[i] = present_value;
            reg_stats[i] = stat;
            return i;
        }
    for (int i = 0; i < 15; i++)
    {
        if (reg_stats[i] == 1)
        {
            value_map[registers[i]].reg_name = -1;
            int offset = value_map[registers[i]].reg_offset;
            if (offset == -1)
            {
                offset = stack_top;
                stack_top += 4;
                value_map[registers[i]].reg_offset = offset;
                std::cout << "\tsw    " << reg_names[i] << ", " << offset <<
                    "(sp)" << std::endl;
            }
            registers[i] = present_value;
            reg_stats[i] = stat;
            return i;
        }
    }
    for (int i = 0; i < 15; i++)
    {
        if (reg_stats[i] == 2)
        {
            value_map[registers[i]].reg_name = -1;
            int offset = value_map[registers[i]].reg_offset;
            if (offset == -1)
            {
                offset = stack_top;
                stack_top += 4;
                value_map[registers[i]].reg_offset = offset;
                std::cout << "\tsw    " << reg_names[i] << ", " << offset <<
                    "(sp)" << std::endl;
            }
            registers[i] = present_value;
            reg_stats[i] = stat;
            return i;
        }
    }
    assert(false);
    return -1;
}


int cal_stack_size(const koopa_raw_slice_t &slice)
{
    assert(slice.kind == KOOPA_RSIK_VALUE);
    int stack_frame_size = 0;
    for (int i = 0; i < slice.len; i++)
    {
        auto ptr = slice.buffer[i];
        auto value = reinterpret_cast<koopa_raw_value_t>(ptr);
        if (value->kind.tag == KOOPA_RVT_ALLOC ||
            value->ty->tag != KOOPA_RTT_UNIT)
            stack_frame[reinterpret_cast<uintptr_t>(value)]
                = stack_frame_size++;
    }
    if (stack_frame_size & 3)
        stack_frame_size = ((stack_frame_size >> 2) + 1) << 2;
    return stack_frame_size;
}
