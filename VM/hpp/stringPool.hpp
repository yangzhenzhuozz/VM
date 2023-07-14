#ifndef _STRINGPOOL
#define _STRINGPOOL
#include "./environment.hpp"
class StringPool
{
private:
    char* buffer;

public:
    u64 length;
    char** items;
    StringPool(const char* filename);
    ~StringPool();
};
#endif