#include <iomanip>
#include <locale>
#include <iostream>

struct separated : std::numpunct<char>
{
    std::string do_grouping() const
    {
        return "\03";
    }
};
