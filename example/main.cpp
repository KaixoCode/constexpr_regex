
// ------------------------------------------------

#include "constexpr_regex.hpp"

// ------------------------------------------------

static_assert(kaixo::regex<"abc">::parse("abc") == "abc");
static_assert(!kaixo::regex<"abc">::parse("ab"));
static_assert(kaixo::regex<"abc">::parse("xabcx") == "abc");

static_assert(kaixo::regex<"a.c">::parse("abc") == "abc");
static_assert(kaixo::regex<"a.c">::parse("a c") == "a c");
static_assert(!kaixo::regex<"a.c">::parse("ac"));

static_assert(kaixo::regex<"[abc]">::parse("b") == "b");
static_assert(!kaixo::regex<"[abc]">::parse("d"));
static_assert(kaixo::regex<"[a-z]">::parse("m") == "m");
static_assert(kaixo::regex<"[a-zA-Z]">::parse("Z") == "Z");

static_assert(!kaixo::regex<"[^abc]">::parse("b"));
static_assert(kaixo::regex<"[^abc]">::parse("d") == "d");
static_assert(!kaixo::regex<"[^a-z]">::parse("m"));
static_assert(!kaixo::regex<"[^a-zA-Z]">::parse("Z"));

static_assert(kaixo::regex<"[a-z]+">::parse("hello") == "hello");
static_assert(kaixo::regex<"[\\w]+">::parse("hello") == "hello");
static_assert(!kaixo::regex<"[\\W]+">::parse("hello"));

static_assert(kaixo::regex<"cat|dog">::parse("cat") == "cat");
static_assert(kaixo::regex<"cat|dog">::parse("dog") == "dog");
static_assert(!kaixo::regex<"cat|dog">::parse("cow"));
static_assert(kaixo::regex<"ab|cd+">::parse("cddd") == "cddd");

static_assert(kaixo::regex<"(ab)+">::parse("abab") == "abab");
static_assert(kaixo::regex<"(ab)+">::parse("aba") == "ab");

static_assert(kaixo::regex<"a*">::parse("") == "");
static_assert(kaixo::regex<"ab*">::parse("a") == "a");
static_assert(kaixo::regex<"ab*">::parse("abbb") == "abbb");
static_assert(!kaixo::regex<"ab+">::parse("a"));
static_assert(kaixo::regex<"ab+">::parse("abbb") == "abbb");
static_assert(kaixo::regex<"colou?r">::parse("color") == "color");
static_assert(kaixo::regex<"colou?r">::parse("colour") == "colour");

static_assert(kaixo::regex<"a.*b">::parse("axxxbxxb") == "axxxbxxb");
static_assert(kaixo::regex<"a.*?b">::parse("axxxbxxb") == "axxxb");
static_assert(kaixo::regex<"a.+b">::parse("axxxbxxb") == "axxxbxxb");
static_assert(kaixo::regex<"a.+?b">::parse("axxxbxxb") == "axxxb");
static_assert(kaixo::regex<"a.??c">::parse("abc") == "abc");
static_assert(kaixo::regex<"a.??c">::parse("acc") == "ac");

static_assert(kaixo::regex<"(ab*)*">::parse("abbabbbb") == "abbabbbb");
static_assert(kaixo::regex<"(ab|a)*b">::parse("abab") == "abab");
static_assert(kaixo::regex<"(ab|a)*b">::parse("aabab") == "aabab");
static_assert(kaixo::regex<"(ab|a)*b">::parse("aabb") == "aabb");
static_assert(kaixo::regex<"(ab|a)*b">::parse("ab") == "ab");

static_assert(kaixo::regex<"^abc">::parse("abcx") == "abc");
static_assert(!kaixo::regex<"^abc">::parse("xabc"));
static_assert(kaixo::regex<"abc$">::parse("xabc") == "abc");
static_assert(!kaixo::regex<"abc$">::parse("abcx"));
static_assert(kaixo::regex<"^abc$">::parse("abc") == "abc");
static_assert(!kaixo::regex<"^abc$">::parse("xabc"));
static_assert(!kaixo::regex<"^abc$">::parse("abcx"));
static_assert(!kaixo::regex<"^abc$">::parse("xabcx"));

static_assert(kaixo::regex<"\\.">::parse(".") == ".");
static_assert(kaixo::regex<"\\*">::parse("*") == "*");
static_assert(kaixo::regex<"\\\\">::parse("\\") == "\\");

static_assert(kaixo::regex<"foo(?=bar)">::parse("foobar") == "foo");
static_assert(!kaixo::regex<"foo(?=bar)">::parse("foobaz"));
static_assert(!kaixo::regex<"foo(?!bar)">::parse("foobar"));
static_assert(kaixo::regex<"foo(?!bar)">::parse("foobaz") == "foo");

static_assert(kaixo::regex<"(?<=foo)bar">::parse("foobar") == "bar");
static_assert(!kaixo::regex<"(?<=foo)bar">::parse("foxbar"));
static_assert(kaixo::regex<"(?<!foo)bar">::parse("foxbar") == "bar");
static_assert(!kaixo::regex<"(?<!foo)bar">::parse("foobar"));

static_assert(kaixo::regex<"(?<=foo)bar(?=baz)">::parse("foobarbaz") == "bar");
static_assert(kaixo::regex<"foo(?!(bar|baz))">::parse("fooqux") == "foo");
static_assert(!kaixo::regex<"foo(?!(bar|baz))">::parse("foobar"));
static_assert(!kaixo::regex<"foo(?!(bar|baz))">::parse("foobaz"));

static_assert(kaixo::regex<"\\w+(?=\\.)">::parse("filename.txt") == "filename");
static_assert(kaixo::regex<"a.*(?=b)">::parse("axxxbxxb") == "axxxbxx");
static_assert(kaixo::regex<"a.*?(?=b)">::parse("axxxbxxb") == "axxx");

static_assert(kaixo::regex<"(?=a)">::parse("a") == "");
static_assert(!kaixo::regex<"(?=a)">::parse("b"));
static_assert(kaixo::regex<"(?<=b)">::parse("b") == "");
static_assert(!kaixo::regex<"(?<=b)">::parse("a"));
//static_assert(!kaixo::regex<"(?<=ab|xyz)c">::parse("xyzc"));   // <- Won't compile, because variable-length lookbehind is not allowed.
static_assert(kaixo::regex<"(?<=foo)(bar|baz)+?(?=qux)(?!barbazqux)">::parse("foobarbazqux") == "barbaz");

static_assert(kaixo::regex<"(ab|a(bc)+?)(x|y)*?z?([0-9]+|[A-F]+)?\.(foo|bar)+">::parse("abcbcxyyy1234.foobarbarZZ") == "abcbcxyyy1234.foobarbar");

// ------------------------------------------------

int main() {
    return 0;
}

// ------------------------------------------------
