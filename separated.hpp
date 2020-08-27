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
