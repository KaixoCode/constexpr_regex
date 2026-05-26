
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

static_assert(kaixo::regex<"(ab|a(bc)+?)(x|y)*?z?([0-9]+|[A-F]+)?\\.(foo|bar)+">::parse("abcbcxyyy1234.foobarbarZZ") == "abcbcxyyy1234.foobarbar");

// ------------------------------------------------

constexpr auto r0 = kaixo::regex<"(ab)(cd)">::parse("abcd");
static_assert(r0 == "abcd");
static_assert(r0.groups.size() == 2);
static_assert(r0.groups[0].match == "ab");
static_assert(r0.groups[1].match == "cd");

constexpr auto r1 = kaixo::regex<"()(abcd)">::parse("abcd");
static_assert(r1 == "abcd");
static_assert(r1.groups.size() == 2);
static_assert(r1.groups[0].match == "");
static_assert(r1.groups[1].match == "abcd");

constexpr auto r2 = kaixo::regex<"(ab)+">::parse("abab");
static_assert(r2 == "abab");
static_assert(r2.groups.size() == 1);
static_assert(r2.groups[0].match == "ab");

constexpr auto r3 = kaixo::regex<"((a)+)">::parse("aa");
static_assert(r3 == "aa");
static_assert(r3.groups.size() == 2);
static_assert(r3.groups[0].match == "aa");
static_assert(r3.groups[1].match == "a");

constexpr auto r4 = kaixo::regex<"((?:a)+)">::parse("aa");
static_assert(r4 == "aa");
static_assert(r4.groups.size() == 1);
static_assert(r4.groups[0].match == "aa");

constexpr auto r5 = kaixo::regex<"a(.*)b">::parse("axxxbxxb");
static_assert(r5 == "axxxbxxb");
static_assert(r5.groups.size() == 1);
static_assert(r5.groups[0].match == "xxxbxx");

constexpr auto r6 = kaixo::regex<"a(.*?)b">::parse("axxxbxxb");
static_assert(r6 == "axxxb");
static_assert(r6.groups.size() == 1);
static_assert(r6.groups[0].match == "xxx");

// ------------------------------------------------

static_assert(kaixo::regex<"a{0}">::parse("aaaa") == "");
static_assert(kaixo::regex<"a{1}">::parse("aaaa") == "a");
static_assert(kaixo::regex<"\\w{0,}b">::parse("b") == "b");
static_assert(kaixo::regex<"\\w{0,}b">::parse("abaab") == "abaab");
static_assert(kaixo::regex<"\\w{1,}b">::parse("abaab") == "abaab");
static_assert(kaixo::regex<"\\w{2,}b">::parse("aaabaab") == "aaabaab");
static_assert(kaixo::regex<"\\w{3,}b">::parse("aaabaab") == "aaabaab");
static_assert(kaixo::regex<"\\w{4,}b">::parse("aaabaab") == "aaabaab");

static_assert(kaixo::regex<"\\w{0,}?b">::parse("abaab") == "ab");
static_assert(kaixo::regex<"\\w{1,}?b">::parse("abaab") == "ab");
static_assert(kaixo::regex<"\\w{1,}?b">::parse("baab") == "baab");
static_assert(kaixo::regex<"\\w{2,}?b">::parse("aaabaab") == "aaab");
static_assert(kaixo::regex<"\\w{3,}?b">::parse("aaabaab") == "aaab");
static_assert(kaixo::regex<"\\w{4,}?b">::parse("aaabaab") == "aaabaab");

static_assert(kaixo::regex<"\\w{0,2}b">::parse("b") == "b");
static_assert(kaixo::regex<"\\w{0,4}b">::parse("abaab") == "abaab");
static_assert(kaixo::regex<"\\w{1,4}b">::parse("abaab") == "abaab");
static_assert(kaixo::regex<"\\w{2,5}b">::parse("aaabaab") == "aaab");
static_assert(kaixo::regex<"\\w{3,6}b">::parse("aaabaab") == "aaabaab");
static_assert(kaixo::regex<"\\w{4,6}b">::parse("aaabaab") == "aaabaab");

static_assert(kaixo::regex<"\\w{0,2}?b">::parse("abaab") == "ab");
static_assert(kaixo::regex<"\\w{1,4}?b">::parse("abaab") == "ab");
static_assert(kaixo::regex<"\\w{1,4}?b">::parse("baab") == "baab");
static_assert(kaixo::regex<"\\w{2,5}?b">::parse("aaabaab") == "aaab");
static_assert(kaixo::regex<"\\w{3,6}?b">::parse("aaabaab") == "aaab");
static_assert(kaixo::regex<"\\w{4,4}?b">::parse("aaabaab") == "abaab");

static_assert(kaixo::regex<"(?:a{1,2}){1,2}">::parse("aaaaaaaa") == "aaaa");
static_assert(kaixo::regex<"(?:a{1,2}?){1,2}?">::parse("aaaaaaaa") == "a");

// ------------------------------------------------

static_assert(!kaixo::regex<"(?>.+)a">::parse("aaaaaa"));
static_assert(kaixo::regex<"(?>b+?)a">::parse("bbbbbbbba") == "ba");

static_assert(kaixo::regex<"(?#parse the character 'a')a">::parse("a") == "a");

// ------------------------------------------------

static_assert(kaixo::regex<"(?'a'a)">::parse("a").groups["a"].match == "a");
static_assert(kaixo::regex<"(?'a'a)|(?'b'b)">::parse("b").groups["b"].match == "b");
static_assert(kaixo::regex<"(?<a>a)">::parse("a").groups["a"].match == "a");
static_assert(kaixo::regex<"(?<a>a)|(?<b>b)">::parse("b").groups["b"].match == "b");
static_assert(kaixo::regex<"(?'a'a(?'sub'a))">::parse("aa").groups["a"].match == "aa");
static_assert(kaixo::regex<"(?'a'a(?'sub'a))">::parse("aa").groups["sub"].match == "a");
static_assert(kaixo::regex<"(?'a'a)(?'b'b)">::parse("ab").groups["a"].match == "a");
static_assert(kaixo::regex<"(?'a'a)(?'b'b)">::parse("ab").groups["b"].match == "b");
static_assert(kaixo::regex<"(?'all'(?'a'a)(?'b'b))">::parse("ab").groups["all"].match == "ab");

// ------------------------------------------------

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

// ------------------------------------------------
