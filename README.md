# constexpr_regex

Constexpr regex parser. Constructs a non-deterministic finite automaton at compile-time from a string literal 
containing a regex expression. See the example project for some examples.

```cpp
#include "constexpr_regex.hpp"

int main() {
    using email_regex = kaixo::regex<"\\b(?'username'[a-zA-Z0-9._%+-]+)@(?'domain'[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}\\b)">;

    constexpr auto parsed = email_regex::parse("my email address is test@example.com");
    constexpr auto username = parsed.groups["username"].match;
    constexpr auto domain   = parsed.groups["domain"].match;

    static_assert(parsed   == "test@example.com");
    static_assert(username == "test");
    static_assert(domain   == "example.com");

    return 0;
}
```