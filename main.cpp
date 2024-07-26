#include <iostream>
#include <any>
#include "Jem/jem.hpp"


int main() {
    jem::Json json("/home/in-diaonic/Programming/Jem/test.json");
    auto e = json.dump();
    std::cout << std::boolalpha << std::get<std::string>(e) << std::endl;
}
