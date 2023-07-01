#pragma once
#include <iostream>
#include <string>
#include <cassert>
#include <map>
#include <cmath>
#include "koopa.h"


struct Reg { int reg_name; int reg_offset; };
std::string reg_names[16] = {"t0", "t1", "t2", "t3", "t4", "t5", "t6",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "x0"};
koopa_raw_value_t registers[16];
int reg_stats[16] = {0};
koopa_raw_value_t present_value = 0;
std::map<const koopa_raw_value_t, Reg> value_map;
int global_num = 0;
std::map<const koopa_raw_value_t, std::string> global_values;
int stack_size = 0, stack_top = 0;
bool restore_ra = false;


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
Reg Visit(const koopa_raw_call_t &call);
Reg Visit(const koopa_raw_get_elem_ptr_t &get_elem_ptr);
Reg Visit(const koopa_raw_get_ptr_t &get_ptr);
std::string Visit(const koopa_raw_global_alloc_t &global);
int find_reg(int stat);
void clear_registers(bool save_temps = true);
int cal_size(const koopa_raw_type_t &ty);
void init_aggregate(const koopa_raw_value_t &aggr);


void Visit(const koopa_raw_program_t &program)
{
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
    if (func->bbs.len == 0)return;
    std::cout << "\t.text" << std::endl;
    std::cout << "\t.globl " << (func->name + 1) << std::endl;
    std::cout << (func->name + 1) << ":" << std::endl;
    assert(stack_size == 0); assert(stack_top == 0);
    int max_arg_num = 0;
    for (size_t i = 0; i < func->bbs.len; i++)
    {
        auto ptr = func->bbs.buffer[i];
        koopa_raw_basic_block_t bb =
            reinterpret_cast<koopa_raw_basic_block_t>(ptr);
        for (size_t j = 0; j < bb->insts.len; j++)
        {
            ptr = bb->insts.buffer[j];
            koopa_raw_value_t inst = reinterpret_cast<koopa_raw_value_t>(ptr);
            if (inst->ty->tag != KOOPA_RTT_UNIT)
            {
                if (inst->kind.tag == KOOPA_RVT_ALLOC)
                    stack_size += cal_size(inst->ty->data.pointer.base);
                else stack_size += 4;
            }
            if (inst->kind.tag == KOOPA_RVT_CALL)
            {
                restore_ra = true;
                int arg_num = inst->kind.data.call.args.len;
                if (arg_num > max_arg_num)max_arg_num = arg_num;
            }
        }
    }
    int arg_stack_size = 0;
    if (max_arg_num > 8)arg_stack_size = (max_arg_num - 8) * 4;
    stack_size += arg_stack_size;
    stack_top += arg_stack_size;
    if (restore_ra)stack_size += 4;
    stack_size = ceil(stack_size / 16.0) * 16;
    if (stack_size > 0 && stack_size <= 2048)
        std::cout << "\taddi  sp, sp, -" << stack_size << std::endl;
    else if (stack_size > 2048)
    {
        std::cout << "\tli    s11, -" << stack_size << std::endl;
        std::cout << "\tadd   sp, sp, s11" << std::endl;
    }
    if (restore_ra)
    {
        if (stack_size - 4 >= -2048 && stack_size - 4 <= 2047)
            std::cout << "\tsw    ra, " << stack_size - 4 << "(sp)" <<
                std::endl;
        else
        {
            std::cout << "\tli    s11, " << stack_size - 4 << std::endl;
            std::cout << "\tadd   s11, sp, s11" << std::endl;
            std::cout << "\tsw    ra, (s11)" << std::endl;
        }
    }
    for (size_t i = 0; i < func->params.len; i++)
    {
        auto ptr = func->params.buffer[i];
        koopa_raw_value_t param = reinterpret_cast<koopa_raw_value_t>(ptr);
        if (i < 8)
        {
            struct Reg param_var = { static_cast<int>(i + 7), -1 };
            value_map[param] = param_var;
            // reg_stats[i + 7] = 1;
            // registers[i + 7] = param;
            // for now param will only be used once at the beginning of a
            // function and never used again, so we don't need to maintain
            // the correct register states for it
        }
        else
        {
            int offset = stack_size + (i - 8) * 4;
            struct Reg param_var = { -1, offset };
            value_map[param] = param_var;
        }
    }
    Visit(func->bbs);
    stack_size = stack_top = 0;
    for (int i = 0; i < 16; i++)reg_stats[i] = 0;
    value_map.clear();
    restore_ra = false;
    std::cout << std::endl;
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
            int reg_offset = value_map[value].reg_offset;
            if (reg_offset >= -2048 && reg_offset <= 2047)
                std::cout << "\tlw    " << reg_names[reg_name] << ", " <<
                    reg_offset << "(sp)" << std::endl;
            else
            {
                std::cout << "\tli    s11, " << reg_offset << std::endl;
                std::cout << "\tadd   s11, sp, s11" << std::endl;
                std::cout << "\tlw    " << reg_names[reg_name] << ", (s11)" <<
                    std::endl;
            }
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
        assert(result_var.reg_name >= 0);
        break;
    case KOOPA_RVT_ALLOC:
        result_var.reg_offset = stack_top;
        assert(value->ty->tag == KOOPA_RTT_POINTER);
        stack_top += cal_size(value->ty->data.pointer.base);
        value_map[value] = result_var;
        break;
    case KOOPA_RVT_GLOBAL_ALLOC:
        global_values[value] = Visit(kind.data.global_alloc);
        break;
    case KOOPA_RVT_LOAD:
        result_var = Visit(kind.data.load);
        value_map[value] = result_var;
        assert(result_var.reg_name >= 0);
        break;
    case KOOPA_RVT_STORE:
        Visit(kind.data.store);
        break;
    case KOOPA_RVT_BRANCH:
        Visit(kind.data.branch);
        break;
    case KOOPA_RVT_GET_ELEM_PTR:
        result_var = Visit(kind.data.get_elem_ptr);
        value_map[value] = result_var;
        assert(result_var.reg_name >= 0);
        break;
    case KOOPA_RVT_GET_PTR:
        result_var = Visit(kind.data.get_ptr);
        value_map[value] = result_var;
        assert(result_var.reg_name >= 0);
        break;
    case KOOPA_RVT_JUMP:
        Visit(kind.data.jump);
        break;
    case KOOPA_RVT_CALL:
        result_var = Visit(kind.data.call);
        value_map[value] = result_var;
        if (value->ty->tag != KOOPA_RTT_UNIT)  // has ret
        {
            registers[result_var.reg_name] = value;
            reg_stats[result_var.reg_name] = 1;
        }
        assert(result_var.reg_name >= 0);
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
    if (ret_value)
    {
        struct Reg result_var = Visit(ret_value);
        assert(result_var.reg_name >= 0);
        if (result_var.reg_name != 7)
            std::cout << "\tmv    a0, " << reg_names[result_var.reg_name] <<
                std::endl;
    }
    clear_registers(false);
    if (restore_ra)
    {
        if (stack_size - 4 >= -2048 && stack_size - 4 <= 2047)
            std::cout << "\tlw    ra, " << stack_size - 4 << "(sp)" <<
                std::endl;
        else
        {
            std::cout << "\tli    t0, " << stack_size - 4 << std::endl;
            std::cout << "\tadd   t0, sp, t0" << std::endl;
            std::cout << "\tlw    ra, (t0)" << std::endl;
        }
    }
    if (stack_size > 0 && stack_size <= 2047)
        std::cout << "\taddi  sp, sp, " << stack_size << std::endl;
    else if (stack_size > 2047)
    {
        std::cout << "\tli    t0, " << stack_size << std::endl;
        std::cout << "\tadd   sp, sp, t0" << std::endl;
    }
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
    old_stat = reg_stats[right_reg];
    reg_stats[right_reg] = 2;
    struct Reg result_var = {find_reg(1), -1};
    reg_stats[right_reg] = old_stat;
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
    if (src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
    {
        int reg_name = find_reg(1);
        struct Reg result_var = {reg_name, -1};
        std::cout << "\tla    " << reg_names[reg_name] << ", " <<
            global_values[src] << std::endl;
        std::cout << "\tlw    " << reg_names[reg_name] << ", 0(" <<
            reg_names[reg_name] << ")" << std::endl;
        return result_var;
    }
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
    // we have to make sure one offset is at most loaded to one register
    if (value_map[src].reg_name >= 0)return value_map[src];
    int reg_name = find_reg(1), reg_offset = value_map[src].reg_offset;
    struct Reg result_var = {reg_name, reg_offset};
    if (reg_offset >= -2048 && reg_offset <= 2047)
        std::cout << "\tlw    " << reg_names[reg_name] << ", " << reg_offset <<
            "(sp)" << std::endl;
    else
    {
        std::cout << "\tli    s11, " << reg_offset << std::endl;
        std::cout << "\tadd   s11, s11, sp" << std::endl;
        std::cout << "\tlw    " << reg_names[reg_name] << ", (s11)" <<
            std::endl;
    }
    return result_var;
}


void Visit(const koopa_raw_store_t &store)
{
    struct Reg value = Visit(store.value);
    koopa_raw_value_t dest = store.dest;
    assert(value.reg_name >= 0);
    if (dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC)
    {
        std::cout << "\tla    s11, " << global_values[dest] << std::endl;
        std::cout << "\tsw    " << reg_names[value.reg_name] << ", 0(s11)" <<
            std::endl;
        return;
    }
    else if (dest->kind.tag == KOOPA_RVT_GET_ELEM_PTR ||
        dest->kind.tag == KOOPA_RVT_GET_PTR)
    {
        int old_stat = reg_stats[value.reg_name];
        reg_stats[value.reg_name] = 2;
        struct Reg dest_var = Visit(dest);
        assert(dest_var.reg_name >= 0);
        reg_stats[value.reg_name] = old_stat;
        std::cout << "\tsw    " << reg_names[value.reg_name] << ", (" <<
            reg_names[dest_var.reg_name] << ")" << std::endl;
        return;
    }
    assert(value_map.count(dest));
    if (value_map[dest].reg_offset == -1)
    {
        value_map[dest].reg_offset = stack_top;
        stack_top += 4;
    }
    else  // old register loaded from reg_offset is outdated ...
        for (int i = 0; i < 16; i++)
            if (i == value.reg_name)continue;
            else if (reg_stats[i] > 0 && value_map[registers[i]].reg_offset ==
                value_map[dest].reg_offset)
            {
                reg_stats[i] = 0;  // ... so clear it and update value_map
                value_map[registers[i]].reg_name = value.reg_name;
            }
    int reg_name = value.reg_name, reg_offset = value_map[dest].reg_offset;
    if (reg_offset >= -2048 && reg_offset <= 2047)
        std::cout << "\tsw    " << reg_names[reg_name] << ", " << reg_offset <<
            "(sp)" << std::endl;
    else
    {
        std::cout << "\tli    s11, " << reg_offset << std::endl;
        std::cout << "\tadd   s11, s11, sp" << std::endl;
        std::cout << "\tsw    " << reg_names[reg_name] << ", (s11)" <<
            std::endl;
    }
}


void Visit(const koopa_raw_branch_t &branch)
{
    std::string true_label = branch.true_bb->name + 1;
    std::string false_label = branch.false_bb->name + 1;
    int cond_reg = Visit(branch.cond).reg_name;
    clear_registers(false);
    std::cout << "\tbnez  " << reg_names[cond_reg] << ", " << true_label
        << std::endl;
    std::cout << "\tj     " << false_label << std::endl;
}


void Visit(const koopa_raw_jump_t &jump)
{
    clear_registers(false);
    std::string target_label = jump.target->name + 1;
    std::cout << "\tj     " << target_label << std::endl;
}


Reg Visit(const koopa_raw_call_t &call)
{
    struct Reg result_var = { 7, -1 };
    clear_registers();
    std::vector<int> old_stats;
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
            old_stats.push_back(reg_stats[i + 7]);
            reg_stats[i + 7] = 2;
        }
        else if ((i - 8) * 4 >= -2048 && (i - 8) * 4 <= 2047)
            std::cout << "\tsw    " << reg_names[arg_var.reg_name] << ", " <<
                (i - 8) * 4 << "(sp)" << std::endl;
        else
        {
            std::cout << "\tli    s11, " << (i - 8) * 4 << std::endl;
            std::cout << "\tadd   s11, s11, sp" << std::endl;
            std::cout << "\tsw    " << reg_names[arg_var.reg_name] << ", (s11)"
                << std::endl;
        }
    }
    for (int i = 0; i < old_stats.size(); i++)reg_stats[i + 7] = old_stats[i];
    std::cout << "\tcall  " << call.callee->name + 1 << std::endl;
    clear_registers(false);
    return result_var;
}


std::string Visit(const koopa_raw_global_alloc_t &global)
{
    std::string name = "var_" + std::to_string(global_num++);
    std::cout << "\t.data" << std::endl;
    std::cout << "\t.globl " << name << std::endl;
    std::cout << name << ":" << std::endl;
    switch (global.init->kind.tag)
    {
    case KOOPA_RVT_ZERO_INIT:
        std::cout << "\t.zero " << cal_size(global.init->ty) << std::endl <<
            std::endl;
        break;
    case KOOPA_RVT_INTEGER:
        std::cout << "\t.word " << global.init->kind.data.integer.value <<
            std::endl << std::endl;
        break;
    case KOOPA_RVT_AGGREGATE:
        init_aggregate(global.init);
        std::cout << std::endl;
        break;
    default:
        assert(false);
    }
    return name;
}


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
            }
            if (offset >= -2048 && offset <= 2047)
                std::cout << "\tsw    " << reg_names[i] << ", " << offset <<
                    "(sp)" << std::endl;
            else
            {
                std::cout << "\tli    s11, " << offset << std::endl;
                std::cout << "\tadd   s11, s11, sp" << std::endl;
                std::cout << "\tsw    " << reg_names[i] << ", (s11)" <<
                    std::endl;
            }
            registers[i] = present_value;
            reg_stats[i] = stat;
            return i;
        }
    }
    assert(false);
    return -1;
}


void clear_registers(bool save_temps)
{
    for (int i = 0; i < 15; i++)
        if (reg_stats[i] > 0)
        {
            value_map[registers[i]].reg_name = -1;
            int offset = value_map[registers[i]].reg_offset;
            if (offset == -1)
            {
                offset = stack_top;
                stack_top += 4;
                value_map[registers[i]].reg_offset = offset;
                if (save_temps)
                {
                    if (offset >= -2048 && offset <= 2047)
                        std::cout << "\tsw    " << reg_names[i] << ", " <<
                            offset << "(sp)" << std::endl;
                    else
                    {
                        std::cout << "\tli    s11, " << offset << std::endl;
                        std::cout << "\tadd   s11, s11, sp" << std::endl;
                        std::cout << "\tsw    " << reg_names[i] << ", (s11)" <<
                            std::endl;
                    }
                }
            }
            reg_stats[i] = 0;
        }
}


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


void init_aggregate(const koopa_raw_value_t &aggr)
{
    koopa_raw_slice_t elems = aggr->kind.data.aggregate.elems;
    for (size_t i = 0; i < elems.len; i++)
    {
        auto ptr = elems.buffer[i];
        assert(elems.kind == KOOPA_RSIK_VALUE);
        auto value = reinterpret_cast<koopa_raw_value_t>(ptr);
        if (value->kind.tag == KOOPA_RVT_INTEGER)
            std::cout << "\t.word " << value->kind.data.integer.value <<
                std::endl;
        else if (value->kind.tag == KOOPA_RVT_AGGREGATE)
            init_aggregate(value);
        else assert(false);
    }
}
