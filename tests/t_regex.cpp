#include <cstdio>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#include <cstdlib>

std::string 
replace(const boost::smatch& what)
{
    std::string full(what[0].first, what[0].second);
    std::string env(what[1].first, what[1].second);
    
    const char *value = std::getenv(env.c_str());
    
    if (value == nullptr)
    {
        // Return unmodified full environment string,
        // including the $<...> markers
        return full;
    }
    
    return std::string(value);
}

int main()
{
    boost::regex pat("\\$<([^>]+?)>");
    
    std::string s("abc$<home>1234$<HOME>");    
    std::string t;
    
    t = boost::regex_replace(s,
        pat, 
        replace, //"DEF",
        boost::match_default | boost::format_all);
    
    printf("%s\n", t.c_str());
    
    return 0;
}
