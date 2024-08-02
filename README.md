# Jem
Just a simple, single-header, modern C++ (17) library for parsing JSON.

# Usage example
First, make sure the JSON file is correct as the library doesn't check for errors. Now here's how you can extract what you need out of this nonsense:
```json
// comments are supported
{
  "key": null,
  "todos": [
    "do the laundry",  // <-- let's extract this
    "work"
  ],

  "pings": 32932939231212,
  "cake-left": 0.0001021212112
}
```
<br/>

```cpp
#include <iostream>
#include "Jem/jem.hpp"


int main() {
    // parse the file via the path, there is also a secondary constructor (Json(std::string)) to allow
    // feeding the JSON source string directly
    jem::Json json(std::filesystem::path("path/to/file.json"));

    auto my_json = json.dump();  // type: jem::JSON_t

    bool has_key = my_json.toObject()["key"].isNull();  // to check if the field/value is null

    // let's extract the first todo
    std::string first_todo = my_json.toObject()["todos"].toList()[0].toString();
    std::cout << first_todo << std::endl;  // output: `do the laundry`
}
```

# Trivia
- Numbers aren't kept in their own type but rather in an `std::string` to prevent precision loss.
- The type `jem::JSON_t` returned by dump, is basically an abstraction over `std::variant<std::string, bool, JSObject, JSList, std::nullptr_t>`.
- The lib internally uses a recursive-descent approach for parsing. 