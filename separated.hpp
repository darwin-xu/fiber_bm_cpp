#ifndef SEPARATED_H
#define SEPARATED_H

#include <iomanip>
#include <locale>
#include <string>

struct separated : std::numpunct<char>
{
    std::string do_grouping() const
    {
        return "\03";
    }
};

#endif // SEPARATED_H
