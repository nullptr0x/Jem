#include <iostream>
#include "Jem/jem.hpp"


int main() {
    jem::Json json("/home/in-diaonic/Programming/Jem/test.json");
    auto e = json.dump();
    std::cout << e.getList().at(0).getObject()[":)"].getList().at(0).getString() << std::endl;
}
