#include <iostream>
#include "Jem/jem.hpp"


int main() {
    jem::Json json(std::filesystem::path("/home/in-diaonic/Programming/Jem/test.json"));
    auto e = json.dump();

    std::cout << e.toList()[1].toObject()["task"].toList()[0].toString() << std::endl;
}
