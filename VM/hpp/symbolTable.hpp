#ifndef _SYMBOLTABLE
#define _SYMBOLTABLE
#include "./environment.hpp"
struct symbolTableItem
{
    u64 name;
    u64 offset;
};
class SymbolTable
{
public:
    u64 length;
    symbolTableItem *items;
    SymbolTable(const char *filename);
    ~SymbolTable();
};
#endif