#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include "AST.h"
#define _SUB_MODE
using namespace std;


extern FILE *yyin;
extern int yyparse(unique_ptr<BaseAST> &ast);


int main(int argc, const char *argv[])
{
    assert(argc == 5);
    auto mode = argv[1];
    auto input = argv[2];
    auto output = argv[4];

    yyin = fopen(input, "r");
    #ifdef  _SUB_MODE
    freopen(output, "w", stdout); 
    #endif
    assert(yyin);

    unique_ptr<BaseAST> ast;
    auto ret = yyparse(ast);
    assert(!ret);

    if (string(mode) == "-koopa")ast->dumpIR();
    else cout << "NotImplementedError" << endl;
    cout << endl;
    return 0;
}
