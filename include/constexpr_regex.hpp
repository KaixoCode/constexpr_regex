
// ------------------------------------------------

#pragma once

// ------------------------------------------------

#include <algorithm>
#include <array>
#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

// ------------------------------------------------

namespace kaixo {
    
    // ------------------------------------------------

    /**
        Wrapper around a string literal, so it can be used as a template parameter.
        Contains a couple simple helper function for easy tokenizing.

        @tparam N           length of the literal.
     */
    template<std::size_t N>
    struct regex_literal {
        consteval regex_literal(const char(&data)[N], std::size_t offset = 0) 
            : offset(offset) { std::copy_n(data, N, value); }

        consteval char operator[](std::size_t n) const { return value[offset + n]; }
        consteval regex_literal operator+=(std::size_t n) const { return { value, offset + n }; }
        consteval bool empty() const { return std::string_view{ value + offset, N - offset - 1 }.empty(); }
        consteval std::size_t size() const { return std::string_view{ value + offset, N - offset - 1 }.size(); }

        std::size_t offset = 0; // Start offset in the literal. Used during tokenizing.
        char value[N]{};
    };

    // ------------------------------------------------

    /**
        Capture group.
     */
    struct group {
        std::optional<std::string_view> match;
    };

    /**
        Parse result containing N groups.

        @tparam N       number of capture groups, determined when parsing the regex.
     */
    template<std::size_t N = 0>
    struct parse_result {

        // ------------------------------------------------

        constexpr static std::size_t size = N;

        // ------------------------------------------------

        constexpr parse_result() = default;
        constexpr parse_result(std::nullopt_t) {}
        constexpr parse_result(std::string_view result) : match(result) {}

        // ------------------------------------------------

        template<std::size_t I> 
            requires (I < N) // Allow casting to results with more groups, not less.
        constexpr parse_result(parse_result<I> other)
            : match(other.match)
        {
            std::copy(other.groups.begin(), other.groups.end(), groups.begin());
        }

        // ------------------------------------------------

        // @returns true if the parse result contains a match.
        constexpr bool has_match() const { return match.has_value(); }

        // @returns true if the parse result contains a match.
        constexpr operator bool() const { return match.operator bool(); }

        // @returns returns the full match as a string_view, throws if no match.
        constexpr explicit operator std::string_view() const { return match.value(); }

        /** Compare the contained match to a string_view.
        
            @param other        string to compare match to.

            @returns true if the match equals the string.
         */
        constexpr bool operator==(std::string_view other) const { return match == other; }

        // ------------------------------------------------

        /** Merge 2 parse results into a single result. Only valid
            if these 2 parse results are neighbours in the input string.

            @param a            first parse result.
            @param b            second parse result.

            @returns the combined match, including all the groups, with groups in b taking precedence over a.
         */
        template<std::size_t A, std::size_t B>
            requires (A <= N && B <= N)
        constexpr static parse_result merge(parse_result<A> a, parse_result<B> b) {
            parse_result result;
            result.match = {
                a.match->data(),
                a.match->size() + b.match->size()
            };

            if constexpr (A != 0) {
                for (std::size_t i = 0; i < A; ++i) {
                    if (a.groups[i].match) {
                        result.groups[i] = a.groups[i];
                    }
                }
            }

            if constexpr (B != 0) {
                for (std::size_t i = 0; i < B; ++i) {
                    if (b.groups[i].match) {
                        result.groups[i] = b.groups[i];
                    }
                }
            }

            return result;
        }

        // ------------------------------------------------

        std::optional<std::string_view> match{};
        std::array<group, N> groups{};

        // ------------------------------------------------

    };

    // ------------------------------------------------

    constexpr static bool is_word_char(char c) { return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_'; }
    constexpr static bool is_whitespace_char(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
    constexpr static bool is_digit_char(char c) { return c >= '0' && c <= '9'; }

    // ------------------------------------------------

    /**
        The context keeps track of what part of the input string is consumed,
        also contains a couple helper functions for consuming characters and backtracking.
     */
    struct context {

        // ------------------------------------------------

        std::string_view original;
        std::string_view value = original;

        // ------------------------------------------------

        /** Backtrack a certain amount, returns a new context if backtracking is possible.
            
            @param size         number of characters to backtrack.

            @returns a new context if it's possible, nullopt if not enough characters to backtrack.
         */
        constexpr std::optional<context> backtrack(std::size_t size) {
            std::size_t available_room = original.size() - value.size();
            if (size > available_room) return std::nullopt; // Not enough room to backtrack
            std::size_t backtrack_position = original.size() - value.size() - size;
            context result{
                .original = original,
                .value = original.substr(backtrack_position)
            };
            return result;
        }

        // ------------------------------------------------

        // @returns the previous character, or null-terminator if no previous.
        constexpr char previous() const {
            if (value.size() == original.size()) return '\0';
            return original[original.size() - value.size() - 1];
        }

        // @returns the upcoming character, or null-terminator if no next.
        constexpr char next() const {
            if (value.empty()) return '\0';
            return value.front();
        }

        // ------------------------------------------------

        // @returns zero-width parse result if at start of input, otherwise nullopt.
        constexpr parse_result<> beginning_boundary() {
            if (value.size() != original.size()) return std::nullopt;
            if (value.empty()) return std::nullopt;
            return value.substr(0, 0);
        }

        // @returns zero-width parse result if at end of input, otherwise nullopt.
        constexpr parse_result<> end_boundary() {
            if (value.size() != 0) return std::nullopt;
            return value.substr(0, 0);
        }

        // @returns zero-width parse result if at word boundary, otherwise nullopt.
        constexpr parse_result<> word_boundary() {
            bool was_word = is_word_char(previous());
            bool is_word = is_word_char(next());
            if (was_word != is_word) return value.substr(0, 0);
            return std::nullopt;
        }

        // @returns zero-width parse result if not at word boundary, otherwise nullopt.
        constexpr parse_result<> non_word_boundary() {
            bool was_word = is_word_char(previous());
            bool is_word = is_word_char(next());
            if (was_word == is_word) return value.substr(0, 0);
            return std::nullopt;
        }

        // ------------------------------------------------

        /** Tries to consume a specific character.
            
            @param c            character.

            @returns the parsed character if success, otherwise nullopt.
         */
        constexpr parse_result<> consume(char c) {
            if (value.empty() || value.front() != c) return std::nullopt;
            std::string_view result = value.substr(0, 1);
            value = value.substr(1);
            return result;
        }

        /** Tries to consume a single character, unless it's a specific character.
            
            @param c            blacklisted character.

            @returns a single parsed character if success, otherwise nullopt.
         */
        constexpr parse_result<> consume_not(char c) {
            if (value.empty() || value.front() == c) return std::nullopt;
            std::string_view result = value.substr(0, 1);
            value = value.substr(1);
            return result;
        }

        /** Tries to consume one character.

            @param c            character.

            @returns a single parsed character if the input is not empty, otherwise nullopt.
         */
        constexpr parse_result<> consume_one() {
            if (value.empty()) return std::nullopt;
            std::string_view result = value.substr(0, 1);
            value = value.substr(1);
            return result;
        }

        /** Tries to consume one of the characters in the input string view.

            @param chars        list of whitelisted characters.

            @returns the parsed character if success, otherwise nullopt.
         */
        constexpr parse_result<> consume_one_of(std::string_view chars) {
            if (value.empty()) return std::nullopt;
            auto it = chars.find(value.front());
            if (it != std::string_view::npos) {
                std::string_view result = value.substr(0, 1);
                value = value.substr(1);
                return result;
            }
            return std::nullopt;
        }

        /** Tries to consume one character that's not in the input string view.

            @param chars        list of blacklisted characters.

            @returns the parsed character if success, otherwise nullopt.
         */
        constexpr parse_result<> consume_one_not_of(std::string_view chars) {
            if (value.empty()) return std::nullopt;
            auto it = chars.find(value.front());
            if (it == std::string_view::npos) {
                std::string_view result = value.substr(0, 1);
                value = value.substr(1);
                return result;
            }
            return std::nullopt;
        }
        
        /** Tries to consume one character if it passes the passed constraint.

            @param functor      functor that returns true if it should consume.

            @returns the parsed character if success, otherwise nullopt.
         */
        template<std::invocable<char> Lambda>
        constexpr parse_result<> consume_one_of(Lambda functor) {
            if (value.empty()) return std::nullopt;
            if (functor(value.front())) {
                std::string_view result = value.substr(0, 1);
                value = value.substr(1);
                return result;
            }
            return std::nullopt;
        }

        /** Tries to consume one character if it fails the passed constraint.

            @param functor      functor that returns true if it should not consume.

            @returns the parsed character if success, otherwise nullopt.
         */
        template<std::invocable<char> Lambda>
        constexpr parse_result<> consume_one_not_of(Lambda functor) {
            if (value.empty()) return std::nullopt;
            if (!functor(value.front())) {
                std::string_view result = value.substr(0, 1);
                value = value.substr(1);
                return result;
            }
            return std::nullopt;
        }

        // ------------------------------------------------

        /**
            Scoped backup struct, used to backup to a previous state if
            parsing fails for the current path.
         */
        struct backup_struct {
            context* self;
            std::string_view backup;

            /** Revert to the backed up string.
            
                @returns a failed parse result.
             */
            constexpr parse_result<> revert() {
                self->value = backup;
                return std::nullopt;
            }
        };

        // ------------------------------------------------

        // @returns a backup struct.
        constexpr backup_struct backup() { return backup_struct{ this, value }; }

        // ------------------------------------------------

    };

    // ------------------------------------------------
    
    /**
        Regex metadata struct is set in every regex parser object, and 
        defines its min and max possible parse result length.

        @tparam Min         minimum possible length of a match.
        @tparam Max         maximum possible length of a match.
     */
    template<std::size_t Min
           , std::size_t Max>
    struct regex_metadata {
        constexpr static std::size_t min = Min;
        constexpr static std::size_t max = Max;
    };

    // ------------------------------------------------

    /**
        Parses any single character.
     */
    struct wildcard_character_parser {
        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            return ctx.consume_one();
        }
    };
    
    /**
        Parses a specific character.

        @tparam C           character.
     */
    template<char C>
    struct single_character_parser {
        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            return ctx.consume(C);
        }
    };
    
    /**
        Parses a single character that matches the meta character; 
        "\\w" parses a word character, "\\d" parses a digit etc.

        @tparam C           meta character.
     */
    template<char C>
    struct meta_character_parser {
        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            if constexpr (C == 's') return ctx.consume_one_of(is_whitespace_char); // Whitespace
            else if constexpr (C == 'S') return ctx.consume_one_not_of(is_whitespace_char); // Non-Whitespace
            else if constexpr (C == 'd') return ctx.consume_one_of(is_digit_char); // Digit
            else if constexpr (C == 'D') return ctx.consume_one_not_of(is_digit_char); // Non-digit
            else if constexpr (C == 'w') return ctx.consume_one_of(is_word_char); // Word character
            else if constexpr (C == 'W') return ctx.consume_one_not_of(is_word_char); // Non-Word character
            else if constexpr (C == 'X') static_assert(C != C, "Unicode sequences not supported"); // TODO: Unicode sequences
            else if constexpr (C == 'C') static_assert(C != C, "Data unit not supported"); // TODO: Data unit
            else if constexpr (C == 'R') static_assert(C != C, "Unicode newlines not supported"); // TODO: Unicode newline
            else if constexpr (C == 'N') return ctx.consume_one_not_of("\n"); // Unicode newline
            else if constexpr (C == 'h') return ctx.consume_one_of(" \t"); // Horizontal whitespace
            else if constexpr (C == 'H') return ctx.consume_one_not_of(" \t"); // Horizontal whitespace
            else if constexpr (C == 'v') return ctx.consume_one_of("\n\r\f\v"); // Vertical whitespace
            else if constexpr (C == 'V') return ctx.consume_one_not_of("\n\r\f\v"); // Vertical whitespace
        }
    };

    /**
        Parses a single character that's within the character range.

        @tparam A           start of range; inclusive.
        @tparam B           end of range; inclusive.
     */
    template<char A, char B>
    struct character_range_parser {
        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            return ctx.consume_one_of([](char c) -> bool { return c >= A && c <= B; });
        }
    };
    
    /**
        Boundary assertion, tests for a specific position, and results in 
        an empty string if successful.

        @tparam C           boundary assertion type.
     */
    template<char C>
    struct boundary_assertion {
        using metadata = regex_metadata<0, 0>;

        static constexpr parse_result<> parse(context& ctx) {
            if constexpr (C == 'b') return ctx.word_boundary(); 
            else if constexpr (C == 'B') return ctx.non_word_boundary();
            else if constexpr (C == '^') return ctx.beginning_boundary();
            else if constexpr (C == 'A') return ctx.beginning_boundary();
            else if constexpr (C == '$') return ctx.end_boundary();
            else if constexpr (C == 'Z') return ctx.end_boundary(); // TODO: multiline mode etc.
            else if constexpr (C == 'z') return ctx.end_boundary();
        }
    };

    /**
        Lookahead assertion, looks ahead for a match.

        @tparam A           nested NFA graph.
     */
    template<class A>
    struct lookahead_assertion {
        using metadata = regex_metadata<0, 0>;

        static constexpr parse_result<> parse(context& ctx) {
            auto _ = ctx.backup();
            auto result = A::parse(ctx);
            if (result) return _.revert(), ctx.value.substr(0, 0);
            return _.revert();
        }
    };

    /**
        Negative lookahead assertion, looks ahead for a match, and 
        fails if it finds one.

        @tparam A           nested NFA graph.
     */
    template<class A>
    struct negative_lookahead_assertion {
        using metadata = regex_metadata<0, 0>;

        static constexpr parse_result<> parse(context& ctx) {
            auto _ = ctx.backup();
            auto result = A::parse(ctx);
            if (!result) return _.revert(), ctx.value.substr(0, 0);
            return _.revert();
        }
    };

    /**
        Lookbehind assertion, looks behind for a match. The nested
        NFA graph cannot be variable length.

        @tparam A           nested NFA graph.
     */
    template<class A>
    struct lookbehind_assertion {
        static_assert(A::metadata::min == A::metadata::max, "Lookbehind assertions must have a fixed length");
        using metadata = regex_metadata<0, 0>;

        static constexpr parse_result<> parse(context& ctx) {
            auto backtrack = ctx.backtrack(A::metadata::min);
            if (!backtrack) return std::nullopt;
            auto result = A::parse(*backtrack);
            if (result) return ctx.value.substr(0, 0);
            return std::nullopt;
        }
    };

    /**
        Negative lookbehind assertion, looks behind for a match, and 
        fails if it finds one. The nested NFA graph cannot be variable length.

        @tparam A           nested NFA graph.
     */
    template<class A>
    struct negative_lookbehind_assertion {
        static_assert(A::metadata::min == A::metadata::max, "Lookbehind assertions must have a fixed length");
        using metadata = regex_metadata<0, 0>;

        static constexpr parse_result<> parse(context& ctx) {
            auto backtrack = ctx.backtrack(A::metadata::min);
            if (!backtrack) return ctx.value.substr(0, 0);
            auto result = A::parse(*backtrack);
            if (!result) return ctx.value.substr(0, 0);
            return std::nullopt;
        }
    };

    /**
        Character class parser parses a single character from a user-defined
        character class. 

        @tparam ...Args     all the characters or character ranges in this class.
     */
    template<class ...Args>
    struct character_class_parser {
        static_assert(((Args::metadata::min == 1 && Args::metadata::max == 1) && ...), 
            "All elements of a character class must consume a single character");

        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            parse_result final_result{};

            (([&] -> bool {
                auto result = Args::parse(ctx);
                if (result) final_result = result;
                return static_cast<bool>(result);
            }()) || ...);

            return final_result;
        }
    };

    /**
        Character class parser parses a single character that's not in the 
        user-defined character class.using length

        @tparam A           the character class parser that's negated.
     */
    template<class A>
    struct negated_character_class_parser {
        using metadata = regex_metadata<1, 1>;

        static constexpr parse_result<> parse(context& ctx) {
            auto _ = ctx.backup();
            if (A::parse(ctx)) return _.revert();
            return ctx.consume_one();
        }
    };

    // ------------------------------------------------

    /**
        The start node for every NFA graph. Dummy class, just used to signal first node.
     */
    struct graph_start_node {
        using metadata = regex_metadata<0, 0>;
    };

    /**
        Nfa graph node metadata, keeps track of this node's group, and 
        in the case that it's recursive, the min and max amount of times
        it can recurse.

        @tparam MinRepeats      minimum number of recurses to itself.
        @tparam MaxRepeats      maximum number of recurses to itself.
        @tparam Group           group index that this node captures into.
     */
    template<std::size_t MinRepeats = 0
           , std::size_t MaxRepeats = std::string_view::npos
           , std::size_t Group = std::string_view::npos>
    struct nfa_graph_node_metadata {
        constexpr static std::size_t min_repeats = MinRepeats;
        constexpr static std::size_t max_repeats = MaxRepeats;
        constexpr static std::size_t group = Group;

        /**
            Update the maximum number of repeats.

            @tparam N       new maximum number of repeats.
         */
        template<std::size_t N>
        using with_max_repeat = nfa_graph_node_metadata<MinRepeats, N, Group>;

        /**
            Update the minimum number of repeats.

            @tparam N       new minimum number of repeats.
         */
        template<std::size_t N>
        using with_min_repeat = nfa_graph_node_metadata<N, MaxRepeats, Group>;

        /**
            Set the groups index.

            @tparam I           group index.
         */
        template<std::size_t I>
        using with_group = nfa_graph_node_metadata<MinRepeats, MaxRepeats, I>;
    };

    /**
        A node in an NFA graph. Contains an operation and
        the relative indices of the next possible nodes.

        @tparam MetaData    the nfa_graph_node_metadata.
        @tparam Operation   the actual parser in this node.
        @tparam ...Outs     the relative indices of the next possible nodes.
     */
    template<class MetaData, class Operation, std::size_t ...Outs>
    struct nfa_graph_node {
        using metadata = MetaData;
        using operation = Operation;
        using outs = std::index_sequence<Outs...>;

        /**
            Update the maximum number of repeats of this node.

            @tparam N       new maximum number of repeats.
         */
        template<std::size_t N>
        using with_max_repeat = nfa_graph_node<typename metadata::template with_max_repeat<N>, Operation, Outs...>;

        /**
            Update the minimum number of repeats of this node.

            @tparam N       new minimum number of repeats.
         */
        template<std::size_t N>
        using with_min_repeat = nfa_graph_node<typename metadata::template with_min_repeat<N>, Operation, Outs...>;
        
        /**
            Set the groups index of this node.

            @tparam I           group index.
         */
        template<std::size_t I>
        using with_group = nfa_graph_node<typename metadata::template with_group<I>, Operation, Outs...>;

        /** 
            Append a possible next node to the list. This means it will
            have lower precedence than existing connections. Does not
            add if it's already a connection.

            @tparam A       relative node index.
         */
        template<std::size_t A>
        using append_connection = std::conditional_t<((A == Outs) || ...)
            , nfa_graph_node<MetaData, Operation, Outs...>
            , nfa_graph_node<MetaData, Operation, Outs..., A>>;

        /**
            Prepend a possible next node to the list. This means it will
            have a higher precedence than existing connections. Does not
            add if it's already a connection.

            @tparam A       relative node index.
         */
        template<std::size_t A>
        using prepend_connection = std::conditional_t<((A == Outs) || ...)
            , nfa_graph_node<MetaData, Operation, Outs...>
            , nfa_graph_node<MetaData, Operation, A, Outs...>>;
    };

    /** Test if type is an NFA graph.
    
        @tparam Ty          type to test.
     */
    template<class Ty>
    concept is_nfa_graph = requires { typename Ty::is_nfa_graph_specifier; };

    /**
        Metadata for the parse method in an nfa_graph, used when recursing to nested 
        NFA graphs to remember the parent graph's type and index in there. This is 
        necessary for recursing to the next node that comes after the final node in a
        nested graph. As well as remembering the current group index. 

        @tparam ResultType      when constructing, the current NFA graph's parse result type
        @tparam ParentGraph     when constructing, the current NFA graph type.
        @tparam NodeMeta        when constructing, the metadata of the node type of the nested graph we're recursing to.
        @tparam I               when constructing, the index of the nested graph we're recursing to, necessary for recursing back up.
        @tparam ParentMeta      when constructing, the current NFA graph's graph_parse_metadata, necessary for recursing back up.
     */
    template<class ResultType
           , class ParentGraph = void
           , class NodeMeta = nfa_graph_node_metadata<>
           , std::size_t I = 0
           , class ParentMeta = void>
    struct graph_parse_metadata {
        constexpr static std::size_t index = I;
        constexpr static std::size_t group = NodeMeta::group;
        using result_type = ResultType;
        using parent = ParentGraph;
        using node_meta = NodeMeta;
        using parent_meta = ParentMeta;
    };
    
    /**
        Constexpr NFA graph, with nfa_graph_nodes as template parameters.

        @tparam MetaData    a regex_metadata constructed during parsing.
        @tparam ...Nodes    all the nfa_graph_nodes in this graph.
     */
    template<class MetaData, class ...Nodes>
    struct nfa_graph {
        constexpr static std::size_t groups = MetaData::group_counter;
        using metadata = MetaData;
        using nodes = std::tuple<Nodes...>;

        // used in the is_nfa_graph concept.
        using is_nfa_graph_specifier = int; 

        /** Try to find a match in a string view.
            
            @param str      input string.

            @returns parse result.
         */
        constexpr static parse_result<groups> parse(std::string_view str) {
            context ctx{ str };

            for (std::size_t i = 0; i <= str.size(); i++) {
                std::string_view sub_str = str.substr(i);
                context ctx{ str, sub_str };
                if (auto result = parse<graph_parse_metadata<parse_result<groups>>>(ctx)) return result;
            }

            return std::nullopt;
        }

        /** Parse method used by lookaround assertions to evaluate this sub NFA graph.
        
            @param ctx      parse context.

            @returns parse result.
         */
        constexpr static parse_result<groups> parse(context& ctx) {
            return parse<graph_parse_metadata<parse_result<groups>>>(ctx);
        }

        /** Try to parse one of the possible followup nodes.
            
            @tparam Parent  parent_wrapper instance.
            @tparam I       index of the node we're parsing.

            @param ctx      parse context.

            @returns parse result.
         */
        template<class Meta, std::size_t I, class ...Ns>
        constexpr static typename Meta::result_type parse_followup(context& ctx, std::size_t n = 0, Ns...ns) {
            using node = std::tuple_element_t<I, nodes>;
            using result_type = typename Meta::result_type;
            using outs = typename node::outs;

            constexpr std::size_t min_repeats = node::metadata::min_repeats;
            constexpr std::size_t max_repeats = node::metadata::max_repeats;

            return [&]<std::size_t ...Is>(std::index_sequence<Is...>) -> result_type {
                result_type final_result{};

                if constexpr (((Is == 0) || ...)) { // If this node recurses.
                    if (n + 1 < min_repeats) { // And we're not at minimum repeats yet.
                        final_result = parse<Meta, I>(ctx, n + 1, ns...); // Then return recurse.
                        return final_result;
                    }
                }

                (([&] { // Find first followup that succeeds
                    if constexpr (Is == 0) {
                        if (n + 1 >= max_repeats) {
                            return false;
                        }
                    }

                    auto result = parse<Meta, I + Is>(ctx, Is == 0 ? n + 1 : n, ns...);
                    if (result) final_result = result;
                    return static_cast<bool>(result);
                }()) || ...);

                return final_result;
            }(outs{});
        }

        /** Parse method for a node in this NFA graph.
        
            @tparam Parent  parent_wrapper instance.
            @tparam I       index of the node we're parsing.

            @param ctx      parse context.

            @returns parse result.
         */
        template<class Meta, std::size_t I = 0, class ...Ns>
        constexpr static typename Meta::result_type parse(context& ctx, std::size_t n = 0, Ns...ns) {
            constexpr std::size_t parent_index = Meta::index;
            constexpr std::size_t group = Meta::group;
            using parent = typename Meta::parent;
            using parent_meta = typename Meta::parent_meta;
            using result_type = typename Meta::result_type;

            if constexpr (I >= sizeof...(Nodes)) {
                result_type result{};
                std::string_view end_of_capture_boundary = ctx.value.substr(0, 0);

                if constexpr (std::same_as<parent, void>) { // End of expression, no parent.
                    result.match = end_of_capture_boundary;
                } else { // Otherwise, recurse to followup of parent graph.
                    result = parent::template parse_followup<parent_meta, parent_index>(ctx, ns...);
                }
                
                // If sub-graph is linked to a group number, and no match has been set yet.
                // Set it to the end_of_capture_boundary, this will be matched by the 
                // graph_start_node to construct the full capture using the different in char*.
                if constexpr (group != std::string_view::npos) {
                    if (!result.groups[group].match) {
                        result.groups[group].match = end_of_capture_boundary;
                    }
                }

                return result;
            } else {
                using node = std::tuple_element_t<I, nodes>;
                using operation = typename node::operation;

                constexpr std::size_t min_repeats = node::metadata::min_repeats;
                constexpr std::size_t max_repeats = node::metadata::max_repeats;

                if constexpr (std::same_as<graph_start_node, operation>) {
                    auto _ = ctx.backup();
                    auto result = parse_followup<Meta, I>(ctx, n, ns...); // Start node, just move on to followups

                    // If sub-graph is linked to a group number, and the match in the group is
                    // set to a value, and that value is zero-length (boundary marker), assume this
                    // to be our end_of_capture_boundary, and calculate the full capture using the backup.
                    if constexpr (group != std::string_view::npos) {
                        auto& match = result.groups[group].match;
                        if (match && match.value().empty()) {
                            std::size_t size = std::distance(_.backup.data(), match.value().data());
                            match = std::string_view{ _.backup.data(), size };
                        }
                    }

                    return result;
                } else if constexpr (is_nfa_graph<operation>) { 
                    // Nested graph, handles recursion and followups itself
                    using parse_metadata = graph_parse_metadata<result_type, nfa_graph, typename node::metadata, I, Meta>;
                    return operation::template parse<parse_metadata>(ctx, 0, n, ns...);
                } else if constexpr (max_repeats == 0) { // Max repeats == 0, means it shouldn't actually parse this node.
                    return parse_followup<Meta, I>(ctx, n, ns...);
                } else {
                    auto _ = ctx.backup();

                    auto result = operation::parse(ctx);
                    if (!result) return _.revert();

                    auto followup = parse_followup<Meta, I>(ctx, n, ns...);
                    if (!followup) return _.revert();

                    return result_type::merge(result, followup);
                }
            }
        }
    };
    
    // ------------------------------------------------

    /** 
        List of dummy classes that are used as tokens during parsing.
     */

    struct many_token;
    struct some_token;
    struct optional_token;
    struct lazy_many_token;
    struct lazy_some_token;
    struct lazy_optional_token;
    struct disjunction_token;
    struct right_parenthesis_token;
    struct begin_character_class_token;
    struct begin_negated_character_class_token;
    struct end_character_class_token;
    struct capture_group_token;

    template<template<class> class Nested>
    struct nested_graph_token; // Signals nested NFA graph.

    template<class A>
    using unit_token = A; // Used as unit in nested_graph_token.

    template<std::size_t Min, std::size_t Max, bool Lazy>
    struct match_repeat_token {
        using is_match_repeat_token_specifier = int;
        constexpr static std::size_t min = Min;
        constexpr static std::size_t max = Max;
        constexpr static bool lazy = Lazy;
    };

    template<class Ty> // {N,M}, where N != 0
    concept match_repeat_some_token = !Ty::lazy && Ty::min > 0 &&
        requires() { typename Ty::is_match_repeat_token_specifier; };

    template<class Ty> // {0,M},
    concept match_repeat_many_token = !Ty::lazy && Ty::min == 0 &&
        requires() { typename Ty::is_match_repeat_token_specifier; };

    template<class Ty> // {N,M}?, where N != 0
    concept lazy_match_repeat_some_token = Ty::lazy && Ty::min > 0 &&
        requires() { typename Ty::is_match_repeat_token_specifier; };

    template<class Ty> // {0,M}?,
    concept lazy_match_repeat_many_token = Ty::lazy && Ty::min == 0 &&
        requires() { typename Ty::is_match_repeat_token_specifier; };

    // ------------------------------------------------

    template<class ...Tokens>
    struct token_stream;

    // ------------------------------------------------
    
    /**
        Regex parser metadata struct is used during parsing to determine the 
        minimum and maximum possible length of a result from a particular regex.
        This is used to determine how far to backtrack during a lookbehind.
        And is also used to determine whether to disallow a variable-length 
        lookbehind. As well as group index for the parse result.

        @tparam Min             minimum possible length of a match.
        @tparam Max             maximum possible length of a match.
        @tparam GroupCounter    current group index counter.
     */
    template<std::size_t Min = 0
           , std::size_t Max = 0
           , std::size_t GroupCounter = 0>
    struct regex_parser_metadata {
        constexpr static std::size_t min = Min;
        constexpr static std::size_t max = Max;
        constexpr static std::size_t group_counter = GroupCounter;
        
        /**
            Add a token. Accumulates onto min and max using the min
            and max size of the token.

            @tparam Token       The metadata of the token added to node list.
         */
        template<class Token>
        using add_token = regex_parser_metadata<
              min == std::string_view::npos ? min 
            : Token::min == std::string_view::npos ? Token::min
            : min + Token::min,
              max == std::string_view::npos ? max 
            : Token::max == std::string_view::npos ? Token::max
            : max + Token::max,
            group_counter
        >;
        
        /**
            Adjust the metadata for a repeat parser.

            @tparam I           number of repeats.
         */
        template<std::size_t I>
        using repeat = regex_parser_metadata<
            min == std::string_view::npos ? std::string_view::npos : min * I,
            max == std::string_view::npos ? std::string_view::npos : max * I,
            group_counter
        >;
        
        /**
            Apply a disjunction to the metadata.

            @tparam Other       Other regex_parser_metadata.
         */
        template<class Other>
        using apply_disjunction = regex_parser_metadata<
              min == std::string_view::npos ? min
            : Other::min == std::string_view::npos ? Other::min
            : std::min(min, Other::min),
              max == std::string_view::npos ? max 
            : Other::max == std::string_view::npos ? Other::max
            : std::max(max, Other::max),
            std::max(group_counter, Other::group_counter)
        >;

        /**
            Special case for an optional token. This is evaluated after
            the token that's optional is already added. So it has to remove
            its minimum length, as it's now optional.

            @tparam Token       metadata of the token that's now marked as optional.
         */
        template<class Token>
        using apply_optional = regex_parser_metadata< // Subtract min length, as this was added before we knew it was 'many'.
            min - (Token::min == std::string_view::npos ? 0 : Token::min),
            max,
            group_counter
        >;

        /**
            Special case for a '*' token. This is evaluated after
            the token that's now '*' is already added. So it has to remove
            its minimum length, as it can now potentially match 0 times.
            And the maximum length is now infinity.

            @tparam Token       metadata of the token that's now marked as '*'.
         */
        template<class Token>
        using apply_many = regex_parser_metadata< // Subtract min length, as this was added before we knew it was 'many'.
            min - (Token::min == std::string_view::npos ? 0 : Token::min), 
            std::string_view::npos,
            group_counter
        >;
        
        /**
            Special case for a '+' token. This is evaluated after
            the token that's now '+' is already added. The maximum 
            length is now infinity.

            @tparam Token       metadata of the token that's now marked as '+'.
         */
        template<class Token>
        using apply_some = regex_parser_metadata<
            min, // Token length was already added before we knew it was 'some'.
            std::string_view::npos,
            group_counter
        >;
        
        /**
            Special case for a repeated token '{N}'. This is evaluated after
            the token that's now repeated is already added.

            @tparam Token       metadata of the token that's now marked as repeated.
            @tparam N           number of repeats.
         */
        template<class Token, std::size_t N>
        using apply_repeat = regex_parser_metadata< // Token length was already added before we knew it was repeated, so add N - 1 more.
            min == std::string_view::npos || Token::min == std::string_view::npos ? std::string_view::npos : min + Token::min * (N - 1), 
            max == std::string_view::npos || Token::max == std::string_view::npos ? std::string_view::npos : max + Token::max * (N - 1),
            group_counter
        >;
        
        /**
            Special case for a repeated token '{N,}'. This is evaluated after
            the token that's now repeated is already added.

            @tparam Token       metadata of the token that's now marked as repeated.
            @tparam N           minimum number of repeats.
         */
        template<class Token, std::size_t N>
        using apply_min_repeat = regex_parser_metadata< // Token length was already added before we knew it was repeated, so add N - 1 more.
            min + Token::min * (N - 1),
            std::string_view::npos,
            group_counter
        >;
        
        /**
            Set the group counter.

            @tparam I           new group counter value.
         */
        template<std::size_t I>
        using with_group_counter = regex_parser_metadata<Min, Max, I>;
    };

    // ------------------------------------------------

    /**
        Converts a token_stream to an nfa_graph type. Because of how
        the parser works, it needs to reverse the types of the token_stream.

        @tparam MetaData      the regex_metadata for the resulting NFA graph.
        @tparam Nodes       a token_stream of graph nodes.
        @tparam Result      the nfa_graph we're building.
     */
    template<class MetaData, class Nodes, class Result = nfa_graph<MetaData>>
    struct to_nfa_graph;

    template<class MetaData, class ...Result>
    struct to_nfa_graph<MetaData, token_stream<>, nfa_graph<MetaData, Result...>> {
        using type = nfa_graph<MetaData, Result...>;
    };

    template<class MetaData, class Node, class ...Nodes, class ...Result>
    struct to_nfa_graph<MetaData, token_stream<Node, Nodes...>, nfa_graph<MetaData, Result...>> {
        using type = typename to_nfa_graph<
            MetaData
          , token_stream<Nodes...>
          , nfa_graph<MetaData, Node, Result...>
        >::type;
    };

    template<class MetaData, class ...Nodes>
    using to_nfa_graph_t = typename to_nfa_graph<MetaData, token_stream<Nodes...>>::type;

    // ------------------------------------------------
    
    /**
        The parser for user-defined character classes: '[a-z]'.

        @tparam Stream      the steam of tokens we're parsing from/
        @tparam Result      the resulting list of character parsers to put in the character_class_parser.
     */
    template<class Stream, class Result = token_stream<>>
    struct character_class_regex_parser;

    // Base case for when the token stream runs out. Should not happen, 
    // we should expect an end_character_class_token.
    template<class ...Result>
    struct character_class_regex_parser<token_stream<>, token_stream<Result...>> {
        using type = character_class_parser<Result...>;
        using remainder = token_stream<>;
    };

    // Stop when encountering the end_character_class_token.
    template<class ...Tokens, class ...Result>
    struct character_class_regex_parser<token_stream<end_character_class_token, Tokens...>, token_stream<Result...>> {
        using type = character_class_parser<Result...>;
        using remainder = token_stream<Tokens...>;
    };

    // Consume token into the result.
    template<class Token, class ...Tokens, class ...Result>
    struct character_class_regex_parser<token_stream<Token, Tokens...>, token_stream<Result...>> {
        using parser = character_class_regex_parser<token_stream<Tokens...>, token_stream<Result..., Token>>;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // ------------------------------------------------

    /**
        The regex parser converts a stream of tokens into an NFA graph 
        using partial specializations of this class to match agains specific tokens.

        @tparam MetaData    the regex_parser_metadata instance that we're keeping track of for this nfa_graph.
        @tparam Stream      the stream of tokens we're parsing from.
        @tparam Result      the stream of graph nodes we're constructing.
     */
    template<class MetaData, class Stream, class Result = token_stream<nfa_graph_node<nfa_graph_node_metadata<>, graph_start_node>>>
    struct regex_parser;

    // Base case when the token stream runs out.
    template<class MetaData, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<>, token_stream<A, Nodes...>> {
        using type = to_nfa_graph_t<
            MetaData
          , typename A::template append_connection<1> // Final node, connect to end.
          , Nodes...>;
        using remainder = token_stream<>;
    };

    // Simple conjunction case, wrap the next token in a graph node, and 
    // add a connection to this new node from the previous node.
    template<class MetaData, class Token, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<Token, Tokens...>, token_stream<A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template add_token<typename Token::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>, Token>
                       , typename A::template append_connection<1> // Make connection to next node
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '*', make the previous node we parsed recursive, and
    // add a connection to the node before it such that it can optionally skip it.
    template<class MetaData, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<many_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_many<typename B::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename B::template append_connection<0> // Make the node recurse
                       , typename A::template append_connection<2> // Make the next node optional
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '*?', make the previous node we parsed recursive, but give
    // parsing the next node precedence, to ensure the shortest possible match.
    // Also add a connection to the node before it such that it can optionally skip it,
    // prepend to give that case a higher precedence.
    template<class MetaData, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<lazy_many_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_many<typename B::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename B::template append_connection<1>
                                   ::template append_connection<0> // Make the node recurse, but prefer the non-recursive path
                       , typename A::template prepend_connection<2> // Make the next node optional, but make that the first path
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '+', make the previous node we parsed recursive.
    template<class MetaData, class ...Tokens, class Top, class ...Nodes>
    struct regex_parser<MetaData, token_stream<some_token, Tokens...>, token_stream<Top, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_some<typename Top::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename Top::template append_connection<0> // Make the node recurse
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '+?', make the previous node we parsed recursive, but give
    // parsing the next node precedence, to ensure the shortest possible match.
    template<class MetaData, class ...Tokens, class Top, class ...Nodes>
    struct regex_parser<MetaData, token_stream<lazy_some_token, Tokens...>, token_stream<Top, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_some<typename Top::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename Top::template append_connection<1>
                                     ::template append_connection<0> // Make the node recurse, but prefer the non-recursive path
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '?', add a connection to the node before the previous one such
    // that it can skip it, making it optional.
    template<class MetaData, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<optional_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_optional<typename B::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<B
                       , typename A::template append_connection<2> // Make the next node optional
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '??', add a connection to the node before the previous one such
    // that it can skip it, making it optional. Prepend this, to give it a higher
    // precedence, to ensure shortest possible match.
    template<class MetaData, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<lazy_optional_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_optional<typename B::operation::metadata>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<B
                       , typename A::template prepend_connection<2> // Make the next node optional, and prefer no match
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '|', takes everything parsed up to this point, and wraps it in a sub-graph.
    // And then keeps parsing the rest to make another sub-graph. It then combines these 
    // 2 sub-graphs in such a way that it parses either one or the other, and continues
    // parsing with any remaining tokens.
    template<class MetaData, class Token, class ...Tokens, class Node, class ...Nodes>
    struct regex_parser<MetaData, token_stream<disjunction_token, Token, Tokens...>, token_stream<Node, Nodes...>> {
        using nested = regex_parser<regex_parser_metadata<>::template with_group_counter<MetaData::group_counter>
                                  , token_stream<Token, Tokens...>>;

        using a = to_nfa_graph_t<MetaData, typename Node::template append_connection<1>, Nodes...>;
        using b = typename nested::type;

        using combined_metadata = typename a::metadata
            ::template apply_disjunction<typename b::metadata>;

        using parser = regex_parser<
            combined_metadata
          , typename nested::remainder
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>
                                      , nfa_graph<combined_metadata
                                                , nfa_graph_node<nfa_graph_node_metadata<>, graph_start_node, 1, 2>
                                                , nfa_graph_node<nfa_graph_node_metadata<>, a, 2>
                                                , nfa_graph_node<nfa_graph_node_metadata<>, b, 1>>>
                       , nfa_graph_node<nfa_graph_node_metadata<>, graph_start_node, 1>>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for nested expression; lookaround assertions, or a non-capturing group.
    // Just keeps parsing until it finds the end of the nested expression, and wraps it
    // in a sub-graph.
    template<class MetaData, template<class> class Nested, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<nested_graph_token<Nested>, Tokens...>, token_stream<A, Nodes...>> {
        using nested = regex_parser<regex_parser_metadata<>::template with_group_counter<MetaData::group_counter>
                                  , token_stream<Tokens...>>;

        using new_metadata = typename MetaData
            ::template add_token<typename Nested<typename nested::type>::metadata>;

        using parser = regex_parser<
            new_metadata
          , typename nested::remainder
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>
                                      , Nested<typename nested::type>>
                       , typename A::template append_connection<1> // Make connection to next node
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for capture group. Just keeps parsing until it finds the end of the nested expression, 
    // and wraps it in a sub-graph. Also increments the metadata group counter.
    template<class MetaData, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<capture_group_token, Tokens...>, token_stream<A, Nodes...>> {
        using nested = regex_parser<regex_parser_metadata<>::with_group_counter<MetaData::group_counter + 1>
                                  , token_stream<Tokens...>>;

        using new_metadata = typename MetaData
            ::template add_token<typename nested::type::metadata>
            ::template with_group_counter<nested::type::metadata::group_counter>;

        using parser = regex_parser<
            new_metadata
          , typename nested::remainder
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>::with_group<MetaData::group_counter>
                                      , typename nested::type>
                       , typename A::template append_connection<1> // Make connection to next node
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // End-case for nested expressions. Stops parsing and sets the remainder.
    template<class MetaData, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<right_parenthesis_token, Tokens...>, token_stream<A, Nodes...>> {
        using type = to_nfa_graph_t<MetaData
                                  , typename A::template append_connection<1> // End of nested expression, connect to end.
                                  , Nodes...>;
        using remainder = token_stream<Tokens...>;
    };

    // Case for a user-defined character class. Uses the character_class_regex_parser to 
    // parse the character class, and then continues like normal.
    template<class MetaData, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<begin_character_class_token, Tokens...>, token_stream<A, Nodes...>> {
        using nested = character_class_regex_parser<token_stream<Tokens...>>;

        using new_metadata = typename MetaData
            ::template add_token<typename nested::type::metadata>;

        using parser = regex_parser<
            new_metadata
          , typename nested::remainder
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>
                                      , typename nested::type>
                       , typename A::template append_connection<1> // Make connection to next node
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for a negated user-defined character class, works the same as the normal one,
    // but wraps it in a negated_character_class_parser.
    template<class MetaData, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<begin_negated_character_class_token, Tokens...>, token_stream<A, Nodes...>> {
        using nested = character_class_regex_parser<token_stream<Tokens...>>;

        using new_metadata = typename MetaData
            ::template add_token<typename nested::type::metadata>;

        using parser = regex_parser<
            new_metadata
          , typename nested::remainder
          , token_stream<nfa_graph_node<nfa_graph_node_metadata<>
                                      , negated_character_class_parser<typename nested::type>>
                       , typename A::template append_connection<1> // Make connection to next node
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Match range case: '{N,M}', where N != 0.
    template<class MetaData, match_repeat_some_token N, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<N, Tokens...>, token_stream<A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_repeat<typename A::operation::metadata, N::min>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename A::template append_connection<1>
                                   ::template prepend_connection<0> // Make recursive, and prefer that path
                                   ::template with_min_repeat<N::min>
                                   ::template with_max_repeat<N::max>
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Match range case: '{0,M}'.
    template<class MetaData, match_repeat_many_token N, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<N, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_repeat<typename B::operation::metadata, N::min>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename B::template append_connection<1>
                                   ::template prepend_connection<0> // Make recursive, and prefer that path
                                   ::template with_max_repeat<N::max>
                       , typename A::template append_connection<2> // Make next node optional
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Match range case: '{N,M}?', where N != 0.
    template<class MetaData, lazy_match_repeat_some_token N, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<N, Tokens...>, token_stream<A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_repeat<typename A::operation::metadata, N::min>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename A::template append_connection<1>
                                   ::template append_connection<0> // Make recursive
                                   ::template with_min_repeat<N::min>
                                   ::template with_max_repeat<N::max>
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Match range case: '{0,M}?'.
    template<class MetaData, lazy_match_repeat_many_token N, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<MetaData, token_stream<N, Tokens...>, token_stream<B, A, Nodes...>> {
        using new_metadata = typename MetaData
            ::template apply_repeat<typename B::operation::metadata, N::min>;

        using parser = regex_parser<
            new_metadata
          , token_stream<Tokens...>
          , token_stream<typename B::template append_connection<1>
                                   ::template append_connection<0> // Make recursive
                                   ::template with_max_repeat<N::max>
                       , typename A::template prepend_connection<2> // Make next node optional
                       , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // ------------------------------------------------

    template<regex_literal A, int C = 0, class ...Tokens>
    consteval static auto regex_tokenizer();

    /**
        Result struct for the number parser. Keeps track of resulting
        integer, and how many characters were parsed.
     */
    struct regex_number_parser_result {
        std::size_t result;
        std::size_t characters_parsed;
    };

    /** Parse a number from a regex string literal.
    
        @tparam A           the regex string literal to parse from.
        @tparam N           the resulting integer, used in recursion.
        @tparam I           character counter, used in recursion.

        @returns regex_number_parser_result
     */
    template<regex_literal A, std::size_t N = 0, std::size_t I = 0>
    consteval static regex_number_parser_result regex_number_parser() {
        if constexpr (is_digit_char(A[0])) {
            return regex_number_parser<A += 1, N * 10 + (A[0] - '0'), I + 1>();
        } else {
            return regex_number_parser_result{ .result = N, .characters_parsed = I };
        }
    }

    /** Tokenize a match_repeat_token.
        
        @tparam A           the regex string literal.
        @tparam ...Tokens   the resulting list of tokens.

        @returns a token_stream type containing all tokens.
     */
    template<regex_literal A, class ...Tokens>
    consteval static auto regex_match_repeat_tokenizer() {
        constexpr auto number1 = regex_number_parser<A>();
        static_assert(number1.characters_parsed != 0, "No number specified in repeat.");

        if constexpr (A[number1.characters_parsed] == ',') {
            if constexpr (A[number1.characters_parsed + 1] == '}') {
                if constexpr (A[number1.characters_parsed + 2] == '?') {
                    if constexpr (number1.result == 0) {
                        // case: {0,}?
                        return regex_tokenizer<A += (number1.characters_parsed + 3), 0, Tokens..., lazy_many_token>();
                    } else if constexpr (number1.result == 1) {
                        // case: {1,}?
                        return regex_tokenizer<A += (number1.characters_parsed + 3), 0, Tokens..., lazy_some_token>();
                    } else {
                        // case: {N,}?
                        return regex_tokenizer<A += (number1.characters_parsed + 3), 0, Tokens...,
                            match_repeat_token<number1.result, std::string_view::npos, true>>();
                    }
                } else { 
                    if constexpr (number1.result == 0) {
                        // case: {0,}
                        return regex_tokenizer<A += (number1.characters_parsed + 2), 0, Tokens..., many_token>();
                    } else if constexpr (number1.result == 1) {
                        // case: {1,}
                        return regex_tokenizer<A += (number1.characters_parsed + 2), 0, Tokens..., some_token>();
                    } else {
                        // case: {N,}
                        return regex_tokenizer<A += (number1.characters_parsed + 2), 0, Tokens...,
                            match_repeat_token<number1.result, std::string_view::npos, false>>();
                    }
                }
            } else {
                constexpr auto number2 = regex_number_parser<A += (number1.characters_parsed + 1)>();
                static_assert(number2.characters_parsed != 0, "Invalid repeat range.");
                static_assert(A[number1.characters_parsed + number2.characters_parsed + 1] == '}', "Missing closing '}' in repeat.");
                static_assert(number1.result <= number2.result, "The first number must be smaller or equal to the second number in a repeat.");

                if constexpr (A[number1.characters_parsed + number2.characters_parsed + 2] == '?') {
                    if constexpr (number1.result == 0 && number2.result == 1) {
                        // case: {0,1}?, equivalent to '??'
                        return regex_tokenizer<A += (number1.characters_parsed + number2.characters_parsed + 3), 0, Tokens..., lazy_optional_token>();
                    } else {
                        // case: {N,M}?
                        return regex_tokenizer<A += (number1.characters_parsed + number2.characters_parsed + 3), 0, Tokens..., 
                            match_repeat_token<number1.result, number2.result, true>>();
                    }
                } else { 
                    if constexpr (number1.result == 0 && number2.result == 1) {
                        // case: {0,1}, equivalent to '?'
                        return regex_tokenizer<A += (number1.characters_parsed + number2.characters_parsed + 2), 0, Tokens..., optional_token>();
                    } else {
                        // case: {N,M}
                        return regex_tokenizer<A += (number1.characters_parsed + number2.characters_parsed + 2), 0, Tokens...,
                            match_repeat_token<number1.result, number2.result, false>>();
                    }
                }
            }
        } else {
            static_assert(A[number1.characters_parsed] == '}', "Missing closing '}' in repeat.");

            if constexpr (A[number1.characters_parsed + 1] == '?') { 
                // case: {N}?
                return regex_tokenizer<A += (number1.characters_parsed + 2), 0, Tokens..., 
                    match_repeat_token<number1.result, number1.result, true>>();
            } else { 
                // case: {N}
                return regex_tokenizer<A += (number1.characters_parsed + 1), 0, Tokens...,
                    match_repeat_token<number1.result, number1.result, false>>();
            }
        }
    }

    /** The regex tokenizer, converts a regex literal into a stream of tokens.

        @tparam A           the regex string literal.
        @tparam C           a counter that keeps track of number of tokens since
                            a user-defined character class opened. This is necessary
                            for specific tokenizing cases.
        @tparam ...Tokens   the resulting list of tokens.

        @returns a token_stream type containing all tokens.
     */
    template<regex_literal A, int C, class ...Tokens>
    consteval static auto regex_tokenizer() {
        constexpr bool InsideClass = C > 0; // Inside a user-defined character class?
        constexpr int Next = InsideClass ? C + 1 : 0; // Next index in user-defined character class.
        // It keeps track of the index in the user-defined character class because
        // a character class cannot be empty, so it will consume a ']' as a character
        // if it's still empty, instead of closing the class.

        if constexpr (A.empty()) return std::type_identity<token_stream<Tokens...>>{};
        else if constexpr (!InsideClass && A[0] == '{') {
            return regex_match_repeat_tokenizer<A += 1, Tokens...>();
        }
        else if constexpr (InsideClass && A.size() > 2 && A[1] == '-' && A[2] != ']') {
            return regex_tokenizer<A += 3, Next, Tokens..., character_range_parser<A[0], A[2]>>();
        }
        else if constexpr (!InsideClass && A[0] == '*') {
            if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_many_token>();
            else if constexpr (A[1] == '+') static_assert(A[1] != '+', "Possessive quantifiers not supported!");
            else return regex_tokenizer<A += 1, 0, Tokens..., many_token>();
        }
        else if constexpr (!InsideClass && A[0] == '+') {
            if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_some_token>();
            else if constexpr (A[1] == '+') static_assert(A[1] != '+', "Possessive quantifiers not supported!");
            else return regex_tokenizer<A += 1, 0, Tokens..., some_token>();
        }
        else if constexpr (!InsideClass && A[0] == '?') {
            if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_optional_token>();
            else if constexpr (A[1] == '+') static_assert(A[1] != '+', "Possessive quantifiers not supported!");
            else return regex_tokenizer<A += 1, 0, Tokens..., optional_token>();
        }
        else if constexpr (!InsideClass && A[0] == '.') return regex_tokenizer<A += 1, 0, Tokens..., wildcard_character_parser>();
        else if constexpr (!InsideClass && A[0] == '^') return regex_tokenizer<A += 1, 0, Tokens..., boundary_assertion<'^'>>();
        else if constexpr (!InsideClass && A[0] == '$') return regex_tokenizer<A += 1, 0, Tokens..., boundary_assertion<'$'>>();
        else if constexpr (!InsideClass && A[0] == '|') return regex_tokenizer<A += 1, 0, Tokens..., disjunction_token>();
        else if constexpr (!InsideClass && A[0] == ')') return regex_tokenizer<A += 1, 0, Tokens..., right_parenthesis_token>();
        else if constexpr (!InsideClass && A[0] == '[') {
            if constexpr (A[1] == '^') return regex_tokenizer<A += 2, 1, Tokens..., begin_negated_character_class_token>();
            else return regex_tokenizer<A += 1, 1, Tokens..., begin_character_class_token>();
        }
        else if constexpr (C > 1 && A[0] == ']') return regex_tokenizer<A += 1, 0, Tokens..., end_character_class_token>();
        else if constexpr (!InsideClass && A[0] == '(') {
            if constexpr (A[1] == '?') {
                if constexpr (A[2] == ':') return regex_tokenizer<A += 3, 0, Tokens..., nested_graph_token<unit_token>>();
                else if constexpr (A[2] == '>') static_assert(A[1] != '>', "Atomic groups not supported!");
                else if constexpr (A[2] == '|') static_assert(A[1] != '|', "Duplicate/reset subpattern group number group not supported!");
                else if constexpr (A[2] == '#') static_assert(A[1] != '#', "Comment group not supported!");
                else if constexpr (A[2] == '=') return regex_tokenizer<A += 3, 0, Tokens..., nested_graph_token<lookahead_assertion>>();
                else if constexpr (A[2] == '!') return regex_tokenizer<A += 3, 0, Tokens..., nested_graph_token<negative_lookahead_assertion>>();
                else if constexpr (A[2] == '<' && A[3] == '=') return regex_tokenizer<A += 4, 0, Tokens..., nested_graph_token<lookbehind_assertion>>();
                else if constexpr (A[2] == '<' && A[3] == '!') return regex_tokenizer<A += 4, 0, Tokens..., nested_graph_token<negative_lookbehind_assertion>>();
                else static_assert(A[1] != '?', "Unsupported group type!");
                // TODO: potentially add named groups.
            } else {
                return regex_tokenizer<A += 1, 0, Tokens..., capture_group_token>();
            }
        }
        else if constexpr (A[0] == '\\') {
            if constexpr (A[1] == 'a') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\a'>>();
            else if constexpr (A[1] == 'A') return regex_tokenizer<A += 2, Next, Tokens..., boundary_assertion<'A'>>();
            else if constexpr (A[1] == 'b') {
                if constexpr (InsideClass) regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\b'>>();
                else return regex_tokenizer<A += 2, 0, Tokens..., boundary_assertion<'b'>>();
            }
            else if constexpr (A[1] == 'B') {
                static_assert(!InsideClass, "\\B may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., boundary_assertion<'B'>>();
            }
            else if constexpr (A[1] == 'c') static_assert(A[1] != 'c', "Control characters not supported!");
            else if constexpr (A[1] == 'C') {
                static_assert(!InsideClass, "\\C may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'C'>>();
            }
            else if constexpr (A[1] == 'd') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'d'>>();
            else if constexpr (A[1] == 'D') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'D'>>();
            else if constexpr (A[1] == 'f') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\f'>>();
            else if constexpr (A[1] == 'g') static_assert(A[1] != 'g', "Subpattern match not supported!");
            else if constexpr (A[1] == 'G') static_assert(A[1] != 'G', "Start of match not supported!");
            else if constexpr (A[1] == 'h') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'h'>>();
            else if constexpr (A[1] == 'H') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'H'>>();
            else if constexpr (A[1] == 'k') static_assert(A[1] != 'k', "Subpattern match not supported!");
            else if constexpr (A[1] == 'K') static_assert(A[1] != 'K', "Reset Match not supported!");
            else if constexpr (A[1] == 'n') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\n'>>();
            else if constexpr (A[1] == 'N') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'N'>>();
            else if constexpr (A[1] == 'p') static_assert(A[1] != 'p', "Unicode property not supported!");
            else if constexpr (A[1] == 'P') static_assert(A[1] != 'P', "Unicode property not supported!");
            else if constexpr (A[1] == 'Q') static_assert(A[1] != 'Q', "Quotes not supported!");
            else if constexpr (A[1] == 'r') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\r'>>();
            else if constexpr (A[1] == 'R') {
                static_assert(!InsideClass, "\\R may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'R'>>();
            }
            else if constexpr (A[1] == 's') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'s'>>();
            else if constexpr (A[1] == 'S') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'S'>>();
            else if constexpr (A[1] == 't') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\t'>>();
            else if constexpr (A[1] == 'v') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'v'>>();
            else if constexpr (A[1] == 'V') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'V'>>();
            else if constexpr (A[1] == 'w') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'w'>>();
            else if constexpr (A[1] == 'W') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'W'>>();
            else if constexpr (A[1] == 'x') static_assert(A[1] != 'x', "Hex characters not supported!");
            else if constexpr (A[1] == 'X') {
                static_assert(!InsideClass, "\\X may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'X'>>();
            }
            else if constexpr (A[1] == 'z') {
                static_assert(!InsideClass, "\\z may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., boundary_assertion<'z'>>();
            }
            else if constexpr (A[1] == 'Z') {
                static_assert(!InsideClass, "\\Z may not be used inside a character class!");
                return regex_tokenizer<A += 2, Next, Tokens..., boundary_assertion<'Z'>>();
            }
            else if constexpr (A[1] == '0') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\0'>>();
            else if constexpr (A[1] == '#') {
                if constexpr (InsideClass) return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'#'>>();
                else static_assert(A[1] != '#', "Subpattern match not supported!");
            }
            else if constexpr (A[1] == '.') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'.'>>();
            else if constexpr (A[1] == '^') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'^'>>();
            else if constexpr (A[1] == '$') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'$'>>();
            else if constexpr (A[1] == '+') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'+'>>();
            else if constexpr (A[1] == '*') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'*'>>();
            else if constexpr (A[1] == '?') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'?'>>();
            else if constexpr (A[1] == '(') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'('>>();
            else if constexpr (A[1] == ')') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<')'>>();
            else if constexpr (A[1] == '[') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'['>>();
            else if constexpr (A[1] == ']') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<']'>>();
            else if constexpr (A[1] == '{') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'{'>>();
            else if constexpr (A[1] == '}') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'}'>>();
            else if constexpr (A[1] == '\\') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\\'>>();
            else if constexpr (A[1] == '|') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'|'>>();
            else return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<A[1]>>();
        } else {
            return regex_tokenizer<A += 1, Next, Tokens..., single_character_parser<A[0]>>();
        }
    }

    /**
        Tokenize the regex string literal. Resuls in a token_steam of tokens.

        @tparam A           the regex string literal.
     */
    template<regex_literal A>
    using regex_tokenizer_t = decltype(regex_tokenizer<A>())::type;

    // ------------------------------------------------

    /**
        Compile the regex literal into a constexpr regex parser.

        @tparam A           the regex string literal.
     */
    template<regex_literal A>
    using regex = typename regex_parser<regex_parser_metadata<>, regex_tokenizer_t<A>>::type;

    // ------------------------------------------------

}

// ------------------------------------------------
