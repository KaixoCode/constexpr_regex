# constexpr_regex

Constexpr regex parser. Constructs a non-deterministic finite automaton at compile-time from a string literal 
containing a regex expression. See the example project for some examples.

```cpp
#include "constexpr_regex.hpp"

int main() {
    using my_regex = kaixo::regex<"cat|dog">; 

    static_assert(my_regex::parse("cat") == "cat");
    static_assert(my_regex::parse("dog") == "dog");
    static_assert(!my_regex::parse("fish").has_value());

    return 0;
}
```