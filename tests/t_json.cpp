#include <cstdio>
#include "json.hpp"

using json = nlohmann::json;

int main()
{
    json j = "{ \"f\": 3.14, \"i\": 8, \"b\": true }"_json;
    
    printf("is_number f %d\n", j["f"].is_number());
    
    // Implicit casts from float to int
    int i = j["f"];
    int i2 = j["f"].get<int>();
    // Implicit cast from bool to int
    int i3 = j["b"].get<int>();
    printf("i = %d, i2 = %d, i3 = %d\n", i, i2, i3);
    
    // Implicit cast from int to float
    float f = j["i"];
    printf("f = %g\n", f);
    
    bool b = j["b"];
    printf("b = %s\n", b ? "true" : "false");

    /*
    // No implicit cast from int to bool allowed
    bool b2 = j["i"];   // <-- raises [json.exception.type_error.302] type must be boolean, but is number
    printf("b2 = %b\n", b2 ? "true" : "false");
    */
    
    printf("is i an int:%d, float:%d\n", j["i"].is_number_integer(), j["i"].is_number_float());
    printf("is f an int:%d, float:%d\n", j["f"].is_number_integer(), j["f"].is_number_float());
}