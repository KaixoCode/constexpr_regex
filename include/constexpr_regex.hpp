
// ------------------------------------------------

#pragma once

// ------------------------------------------------

#include <algorithm>
#include <utility>
#include <type_traits>
#include <concepts>
#include <string_view>
#include <optional>

// ------------------------------------------------

namespace kaixo {

    // ------------------------------------------------

    /**
        This constexpr regex parser only returns a single string view. 
        It doesn't support groups, it just tries to find a single match in the input string.
     */
    using parse_result = std::optional<std::string_view>;

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

        constexpr char previous() const {
            if (value.size() == original.size()) return '\0';
            return original[original.size() - value.size() - 1];
		}

        constexpr char next() const {
            if (value.empty()) return '\0';
            return value.front();
		}

        // ------------------------------------------------

        // @returns zero-width parse result if at start of input, otherwise nullopt.
        constexpr parse_result beginning_boundary() {
            if (value.size() != original.size()) return std::nullopt;
            if (value.empty()) return std::nullopt;
            return value.substr(0, 0);
        }

        // @returns zero-width parse result if at end of input, otherwise nullopt.
        constexpr parse_result end_boundary() {
            if (value.size() != 0) return std::nullopt;
            return value.substr(0, 0);
        }

        // @returns zero-width parse result if at word boundary, otherwise nullopt.
        constexpr parse_result word_boundary() {
			bool was_word = is_word_char(previous());
			bool is_word = is_word_char(next());
			if (was_word != is_word) return value.substr(0, 0);
            return std::nullopt;
        }

        // @returns zero-width parse result if not at word boundary, otherwise nullopt.
        constexpr parse_result non_word_boundary() {
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
        constexpr parse_result consume(char c) {
            if (value.empty() || value.front() != c) return std::nullopt;
            std::string_view result = value.substr(0, 1);
            value = value.substr(1);
            return result;
        }

        /** Tries to consume one character.

            @param c            character.

            @returns a single parsed character if the input is not empty, otherwise nullopt.
         */
        constexpr parse_result consume_one() {
            if (value.empty()) return std::nullopt;
			std::string_view result = value.substr(0, 1);
			value = value.substr(1);
            return result;
        }

        /** Tries to consume one of the characters in the input string view.

            @param chars        list of whitelisted characters.

            @returns the parsed character if success, otherwise nullopt.
         */
        constexpr parse_result consume_one_of(std::string_view chars) {
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
        constexpr parse_result consume_one_not_of(std::string_view chars) {
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
        constexpr parse_result consume_one_of(Lambda functor) {
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
        constexpr parse_result consume_one_not_of(Lambda functor) {
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
            constexpr parse_result revert() {
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
        Regex length struct is used during parsing to determine the minimum
        and maximum possible length of a result from a particular regex.
        This is used to determine how far to backtrack during a lookbehind.
        And is also used to determine whether to disallow a variable-length 
        lookbehind.

        @tparam Min         minimum possible length of a match.
        @tparam Max         maximum possible length of a match.
     */
    template<std::size_t Min, std::size_t Max>
    struct regex_length {
		constexpr static std::size_t min = Min;
		constexpr static std::size_t max = Max;

        /**
            Combine lengths, takes the min/max of both.

            @tparam Other       Other regex_length.
         */
        template<class Other>
		using combine = regex_length<
              min == std::string_view::npos ? min
            : Other::min == std::string_view::npos ? Other::min
            : std::min(min, Other::min),
			  max == std::string_view::npos ? max 
            : Other::max == std::string_view::npos ? Other::max
            : std::max(max, Other::max)
        >;

        /**
            Add lengths together.

            @tparam Other       Other regex_length.
         */
        template<class Other>
		using add = regex_length<
              min == std::string_view::npos ? min 
            : Other::min == std::string_view::npos ? Other::min
            : min + Other::min,
			  max == std::string_view::npos ? max 
            : Other::max == std::string_view::npos ? Other::max
            : max + Other::max
        >;

        /**
            Special case for an optional token. This is evaluated after
            the token that's optional is already added. So it has to remove
            its minimum length, as it's now optional.

            @tparam Token       A token that's now marked as optional.
         */
        template<class Token>
		using add_optional = regex_length< // Subtract min length, as this was added before we knew it was 'many'.
            min - (Token::min == std::string_view::npos ? 0 : Token::min),
            max
        >;

        /**
            Special case for a '*' token. This is evaluated after
            the token that's now '*' is already added. So it has to remove
            its minimum length, as it can now potentially match 0 times.
            And the maximum length is now infinity.

            @tparam Token       A token that's now marked as '*'.
         */
        template<class Token>
		using add_many = regex_length< // Subtract min length, as this was added before we knew it was 'many'.
            min - (Token::min == std::string_view::npos ? 0 : Token::min), 
			std::string_view::npos
        >;
        
        /**
            Special case for a '+' token. This is evaluated after
            the token that's now '+' is already added. The maximum 
            length is now infinity.

            @tparam Token       A token that's now marked as '+'.
         */
        template<class Token>
		using add_some = regex_length<
            min, // Token length was already added before we knew it was 'some'.
			std::string_view::npos
        >;
    };

    // ------------------------------------------------

    /**
        Parses any single character.
     */
    struct wildcard_character_parser {
		using length = regex_length<1, 1>;

        static constexpr parse_result parse(context& ctx) {
            return ctx.consume_one();
        }
    };
    
    /**
        Parses a specific character.

        @tparam C           character.
     */
    template<char C>
    struct single_character_parser {
		using length = regex_length<1, 1>;

        static constexpr parse_result parse(context& ctx) {
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
		using length = regex_length<1, 1>;

        static constexpr parse_result parse(context& ctx) {
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
        }
    };

    /**
        Parses a single character that's within the character range.

        @tparam A           start of range; inclusive.
        @tparam B           end of range; inclusive.
     */
    template<char A, char B>
    struct character_range_parser {
		using length = regex_length<1, 1>;

        static constexpr parse_result parse(context& ctx) {
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
		using length = regex_length<0, 0>;

        static constexpr parse_result parse(context& ctx) {
            if constexpr (C == 'b') return ctx.word_boundary(); 
            else if constexpr (C == 'B') return ctx.non_word_boundary();
            else if constexpr (C == '^') return ctx.beginning_boundary();
            else if constexpr (C == '$') return ctx.end_boundary();
        }
    };

    /**
        Lookahead assertion, looks ahead for a match.

        @tparam A           nested NFA graph.
     */
    template<class A>
    struct lookahead_assertion {
		using length = regex_length<0, 0>;

        static constexpr parse_result parse(context& ctx) {
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
		using length = regex_length<0, 0>;

        static constexpr parse_result parse(context& ctx) {
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
        static_assert(A::length::min == A::length::max, "Lookbehind assertions must have a fixed length");
		using length = regex_length<0, 0>;

        static constexpr parse_result parse(context& ctx) {
            auto backtrack = ctx.backtrack(A::length::min);
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
		static_assert(A::length::min == A::length::max, "Lookbehind assertions must have a fixed length");
        using length = regex_length<0, 0>;

        static constexpr parse_result parse(context& ctx) {
            auto backtrack = ctx.backtrack(A::length::min);
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
		static_assert(((Args::length::min == 1 && Args::length::max == 1) && ...), 
            "All elements of a character class must consume a single character");

        using length = regex_length<1, 1>;

        static constexpr parse_result parse(context& ctx) {
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
        user-defined character class.

        @tparam A           the character class parser that's negated.
     */
    template<class A>
    struct negated_character_class_parser {
		using length = typename A::length;

        static constexpr parse_result parse(context& ctx) {
			auto _ = ctx.backup();
			if (A::parse(ctx)) return _.revert();
            return ctx.consume_one();
		}
    };

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
        The start node for every NFA graph. Dummy class.
     */
    struct graph_start_node {
		using length = regex_length<0, 0>;
    };

    /**
        A node in an NFA graph. Contains an operation and
        the relative indices of the next possible nodes.

        @tparam Operation   the actual parser in this node.
        @tparam ...Outs     the relative indices of the next possible nodes.
     */
    template<class Operation, std::size_t ...Outs>
    struct nfa_graph_node {
        using operation = Operation;
		using outs = std::index_sequence<Outs...>;

        /** 
            Append a possible next node to the list. This means it will
            have lower precedence than existing connections. Does not
            add if it's already a connection.

            @tparam A       relative node index.
         */
        template<std::size_t A>
		using append = std::conditional_t<((A == Outs) || ...)
            , nfa_graph_node<Operation, Outs...>
            , nfa_graph_node<Operation, Outs..., A>>;

        /**
            Prepend a possible next node to the list. This means it will
            have a higher precedence than existing connections. Does not
            add if it's already a connection.

            @tparam A       relative node index.
         */
        template<std::size_t A>
		using prepend = std::conditional_t<((A == Outs) || ...)
            , nfa_graph_node<Operation, Outs...>
            , nfa_graph_node<Operation, A, Outs...>>;
    };

    /** Test if type is an NFA graph.
    
        @tparam Ty          type to test.
     */
    template<class Ty>
	concept is_nfa_graph = requires { typename Ty::is_nfa_graph_specifier; };

    /**
        Parent graph wrapper type, used when recursing to nested NFA graphs
        to remember the parent graph's type and index in there. This is necessary
        for recursing to the next node that comes after the final node in a
        nested graph. 

        @tparam Self        when constructing, the current NFA graph type.
        @tparam I           when constructing, the index of the nested graph we're recursing to.
        @tparam Parent      when constructing, the current NFA graph's parent_wrapper.
     */
    template<class Self, std::size_t I, class Parent>
    struct parent_wrapper {
        constexpr static std::size_t index = I;
        using parent = Parent;
		using self = Self;
    };
    
    /**
        Constexpr NFA graph, with nfa_graph_nodes as template parameters.

        @tparam Length      a regex_length constructed during parsing that tells our min/max result length.
        @tparam ...Nodes    all the nfa_graph_nodes in this graph.
     */
    template<class Length, class ...Nodes>
    struct nfa_graph {
        using length = Length;
        using is_nfa_graph_specifier = int; // used in the is_nfa_graph concept.

        using nodes = std::tuple<Nodes...>;

        /** Try to find a match in a string view.
            
            @param str      input string.

            @returns parse result.
         */
        constexpr static parse_result parse(std::string_view str) {
            context ctx{ str };

            for (std::size_t i = 0; i <= str.size(); i++) {
                std::string_view sub_str = str.substr(i);
                context ctx{ str, sub_str };
                if (auto result = parse<parent_wrapper<void, 0, void>, 0>(ctx)) return result;
            }

            return std::nullopt;
        }

        /** Parse method used by lookaround assertions to evaluate this sub NFA graph.
        
            @param ctx      parse context.

            @returns parse result.
         */
        constexpr static parse_result parse(context& ctx) {
			return parse<parent_wrapper<void, 0, void>, 0>(ctx);
        }

        /** Try to parse one of the possible followup nodes.
            
            @tparam Parent  parent_wrapper instance.
            @tparam I       index of the node we're parsing.

            @param ctx      parse context.

            @returns parse result.
         */
        template<class Parent, std::size_t I>
        constexpr static parse_result parse_followup(context& ctx) {
            using node = std::tuple_element_t<I, nodes>;
            using outs = typename node::outs;

            return [&]<std::size_t ...Is>(std::index_sequence<Is...>) {
                parse_result final_result{};

                (([&] { // Find first followup that succeeds
                    auto result = parse<Parent, I + Is>(ctx);
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
        template<class Parent, std::size_t I>
        constexpr static parse_result parse(context& ctx) {
            if constexpr (I >= sizeof...(Nodes)) {
				constexpr std::size_t parent_index = Parent::index;
                using parent = typename Parent::self;
                using parent_parent = typename Parent::parent;

                if constexpr (std::same_as<parent, void>) {
                    return ctx.value.substr(0, 0); // End of expression, no parent.
                } else { // Otherwise, recurse to followup of parent graph.
					return parent::template parse_followup<parent_parent, parent_index>(ctx);
                }
            } else {
                using node = std::tuple_element_t<I, nodes>;
                using operation = typename node::operation;

			    if constexpr (std::same_as<graph_start_node, operation>) {
					return parse_followup<Parent, I>(ctx); // Start node, just move on to followups
				} else if constexpr (is_nfa_graph<operation>) { // Nested graph, handles recursion and followups itself
                    return operation::template parse<parent_wrapper<nfa_graph, I, Parent>, 0>(ctx);
                } else {
                    auto _ = ctx.backup();

                    auto result = operation::parse(ctx);
                    if (!result) return _.revert();

                    parse_result followup = parse_followup<Parent, I>(ctx);
                    if (!followup) return _.revert();

                    return std::string_view{
                        result.value().data(),
                        result.value().size() + followup.value().size()
                    };
                }
            }
        }
    };
    
    // ------------------------------------------------

    /** 
        List of dummy classes that are used as tokens during parsing.
     */

    struct many_token {};
    struct some_token {};
    struct optional_token {};
    struct lazy_many_token {};
    struct lazy_some_token {};
    struct lazy_optional_token {};
    struct disjunction_token {};
    struct right_parenthesis_token {};
    struct begin_character_class_token {};
    struct begin_negated_character_class_token {};
    struct end_character_class_token {};

    template<template<class> class Nested>
    struct nested_graph_token {};

    template<class Arg>
	using unit_token = Arg;

    // ------------------------------------------------

    template<class ...Tokens>
    struct token_stream;

    // ------------------------------------------------

    /**
        Converts a token_stream to an nga_graph type. Because of how
        the parser works, it needs to reverse the types of the token_stream.

        @tparam Length      the regex_length for the resulting NFA graph.
        @tparam Nodes       a token_stream of graph nodes.
        @tparam Result      the nfa_graph we're building.
     */
    template<class Length, class Nodes, class Result = nfa_graph<Length>>
    struct to_nfa_graph;

    template<class Length, class ...Result>
    struct to_nfa_graph<Length, token_stream<>, nfa_graph<Length, Result...>> {
        using type = nfa_graph<Length, Result...>;
    };

    template<class Length, class Node, class ...Nodes, class ...Result>
    struct to_nfa_graph<Length, token_stream<Node, Nodes...>, nfa_graph<Length, Result...>> {
        using type = typename to_nfa_graph<
              Length
            , token_stream<Nodes...>
			, nfa_graph<Length, Node, Result...>
        >::type;
    };

    template<class Length, class ...Nodes>
    using to_nfa_graph_t = typename to_nfa_graph<Length, token_stream<Nodes...>>::type;

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

        @tparam Length      the regex_length instance that we're keeping track of for this nfa_graph.
        @tparam Stream      the stream of tokens we're parsing from.
        @tparam Result      the stream of graph nodes we're constructing.
     */
    template<class Length, class Stream, class Result = token_stream<nfa_graph_node<graph_start_node>>>
	struct regex_parser;

    // Base case when the token stream runs out.
	template<class Length, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<>, token_stream<A, Nodes...>> {
        using type = to_nfa_graph_t<Length, typename A::template append<1>, Nodes...>;
        using remainder = token_stream<>;
    };

    // Simple conjunction case, wrap the next token in a graph node, and 
    // add a connection to this new node from the previous node.
	template<class Length, class Token, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<Token, Tokens...>, token_stream<A, Nodes...>> {
        using parser = regex_parser<
			  typename Length::template add<typename Token::length>
            , token_stream<Tokens...>
            , token_stream<nfa_graph_node<Token>
                         , typename A::template append<1> // Make connection to next node
                         , Nodes...>
        >;

		using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '*', make the previous node we parsed recursive, and
    // add a connection to the node before it such that it can optionally skip it.
	template<class Length, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<many_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_many<typename B::operation::length>
            , token_stream<Tokens...>
            , token_stream<typename B::template append<0> // Make the node recurse
                         , typename A::template append<2> // Make the next node optional
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '*?', make the previous node we parsed recursive, but give
    // parsing the next node precedence, to ensure the shortest possible match.
    // Also add a connection to the node before it such that it can optionally skip it,
    // prepend to give that case a higher precedence.
	template<class Length, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<lazy_many_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_many<typename B::operation::length>
            , token_stream<Tokens...>
            , token_stream<typename B::template append<1>::template append<0> // Make the node recurse, but prefer the non-recursive path
                         , typename A::template prepend<2> // Make the next node optional, but make that the first path
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '+', make the previous node we parsed recursive.
	template<class Length, class ...Tokens, class Top, class ...Nodes>
    struct regex_parser<Length, token_stream<some_token, Tokens...>, token_stream<Top, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_some<typename Top::operation::length>
            , token_stream<Tokens...>
            , token_stream<typename Top::template append<0> // Make the node recurse
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '+?', make the previous node we parsed recursive, but give
    // parsing the next node precedence, to ensure the shortest possible match.
	template<class Length, class ...Tokens, class Top, class ...Nodes>
    struct regex_parser<Length, token_stream<lazy_some_token, Tokens...>, token_stream<Top, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_some<typename Top::operation::length>
            , token_stream<Tokens...>
			, token_stream<typename Top::template append<1>::template append<0> // Make the node recurse, but prefer the non-recursive path
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '?', add a connection to the node before the previous one such
    // that it can skip it, making it optional.
	template<class Length, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<optional_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_optional<typename B::operation::length>
            , token_stream<Tokens...>
            , token_stream<B
                         , typename A::template append<2> // Make the next node optional
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '??', add a connection to the node before the previous one such
    // that it can skip it, making it optional. Prepend this, to give it a higher
    // precedence, to ensure shortest possible match.
    template<class Length, class ...Tokens, class B, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<lazy_optional_token, Tokens...>, token_stream<B, A, Nodes...>> {
        using parser = regex_parser<
              typename Length::template add_optional<typename B::operation::length>
            , token_stream<Tokens...>
            , token_stream<B
                         , typename A::template prepend<2> // Make the next node optional, and prefer no match
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for '|', takes everything parsed up to this point, and wraps it in a sub-graph.
    // And then keeps parsing the rest to make another sub-graph. It then combines these 
    // 2 sub-graphs in such a way that it parses either one or the other, and continues
    // parsing with any remaining tokens.
	template<class Length, class Token, class ...Tokens, class Node, class ...Nodes>
    struct regex_parser<Length, token_stream<disjunction_token, Token, Tokens...>, token_stream<Node, Nodes...>> {
        using nested = regex_parser<regex_length<0, 0>, token_stream<Token, Tokens...>>;

        using a = to_nfa_graph_t<Length, typename Node::template append<1>, Nodes...>;
		using b = typename nested::type;

        using combined_length = typename a::length::template combine<typename b::length>;

        using parser = regex_parser<
              combined_length
            , typename nested::remainder
            , token_stream<nfa_graph_node<nfa_graph<combined_length
                                                  , nfa_graph_node<graph_start_node, 1, 2>
                                                  , nfa_graph_node<a, 2>
                                                  , nfa_graph_node<b, 1>>>
                         , nfa_graph_node<graph_start_node, 1>>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for nested expression; lookaround assertions, or a group capture.
    // Just keeps parsing until it finds the end of the nested expression, and wraps it
    // in a sub-graph.
	template<class Length, template<class> class Nested, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<nested_graph_token<Nested>, Tokens...>, token_stream<A, Nodes...>> {
        using nested = regex_parser<regex_length<0, 0>, token_stream<Tokens...>>;

        using parser = regex_parser<
              typename Length::template add<typename Nested<typename nested::type>::length>
            , typename nested::remainder
            , token_stream<nfa_graph_node<Nested<typename nested::type>>
			             , typename A::template append<1> // Make connection to next node
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // End-case for nested expressions. Stops parsing and sets the remainder.
	template<class Length, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<right_parenthesis_token, Tokens...>, token_stream<A, Nodes...>> {
        using type = to_nfa_graph_t<Length, typename A::template append<1>, Nodes...>;
        using remainder = token_stream<Tokens...>;
    };

    // Case for a user-defined character class. Uses the character_class_regex_parser to 
    // parse the character class, and then continues like normal.
	template<class Length, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<begin_character_class_token, Tokens...>, token_stream<A, Nodes...>> {
        using nested = character_class_regex_parser<token_stream<Tokens...>>;

        using parser = regex_parser<
			  typename Length::template add<typename nested::type::length>
            , typename nested::remainder
            , token_stream<nfa_graph_node<typename nested::type>
			             , typename A::template append<1> // Make connection to next node
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // Case for a negated user-defined character class, works the same as the normal one,
    // but wraps it in a negated_character_class_parser.
	template<class Length, class ...Tokens, class A, class ...Nodes>
    struct regex_parser<Length, token_stream<begin_negated_character_class_token, Tokens...>, token_stream<A, Nodes...>> {
        using nested = character_class_regex_parser<token_stream<Tokens...>>;

        using parser = regex_parser<
              typename Length::template add<typename nested::type::length>
            , typename nested::remainder
            , token_stream<nfa_graph_node<negated_character_class_parser<typename nested::type>>
			             , typename A::template append<1> // Make connection to next node
                         , Nodes...>
        >;

        using type = typename parser::type;
        using remainder = typename parser::remainder;
    };

    // ------------------------------------------------

    /**
        The regex tokenizer, converts a regex literal into a stream of tokens.

        @tparam A           the regex string literal.
        @tparam C           a counter that keeps track of number of tokens since 
                            a user-defined character class opened. This is necessary
                            for specific tokenizing cases.
        @tparam ...Tokens   the resulting list of tokens.
     */
    template<regex_literal A, int C = 0, class ...Tokens>
    struct regex_tokenizer {
        consteval static auto tokenize() {
            constexpr bool InsideClass = C > 0; // Inside a user-defined character class?
            constexpr int Next = InsideClass ? C + 1 : 0; // Next index in user-defined character class.
            // It keeps track of the index in the user-defined character class because
            // a character class cannot be empty, so it will consume a ']' as a character
            // if it's still empty, instead of closing the class.

            if constexpr (A.empty()) return std::type_identity<token_stream<Tokens...>>{};
            else if constexpr (InsideClass && A.size() > 2 && A[1] == '-') {
                return regex_tokenizer<A += 3, Next, Tokens..., character_range_parser<A[0], A[2]>>{};
            }
            else if constexpr (!InsideClass && A[0] == '*') {
                if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_many_token>{};
                else return regex_tokenizer<A += 1, 0, Tokens..., many_token>{};
            }
            else if constexpr (!InsideClass && A[0] == '+') {
                if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_some_token>{};
                else return regex_tokenizer<A += 1, 0, Tokens..., some_token>{};
            }
            else if constexpr (!InsideClass && A[0] == '?') {
                if constexpr (A[1] == '?') return regex_tokenizer<A += 2, 0, Tokens..., lazy_optional_token>{};
                else return regex_tokenizer<A += 1, 0, Tokens..., optional_token>{};
            }
            else if constexpr (!InsideClass && A[0] == '.') return regex_tokenizer<A += 1, 0, Tokens..., wildcard_character_parser>{};
            else if constexpr (!InsideClass && A[0] == '^') return regex_tokenizer<A += 1, 0, Tokens..., boundary_assertion<'^'>>{};
            else if constexpr (!InsideClass && A[0] == '$') return regex_tokenizer<A += 1, 0, Tokens..., boundary_assertion<'$'>>{};
            else if constexpr (!InsideClass && A[0] == '|') return regex_tokenizer<A += 1, 0, Tokens..., disjunction_token>{};
            else if constexpr (!InsideClass && A[0] == ')') return regex_tokenizer<A += 1, 0, Tokens..., right_parenthesis_token>{};
            else if constexpr (!InsideClass && A[0] == '[') {
				if constexpr (A[1] == '^') return regex_tokenizer<A += 2, 1, Tokens..., begin_negated_character_class_token>{};
                else return regex_tokenizer<A += 1, 1, Tokens..., begin_character_class_token>{};
            }
			else if constexpr (C > 1 && A[0] == ']') return regex_tokenizer<A += 1, 0, Tokens..., end_character_class_token>{};
            else if constexpr (!InsideClass && A[0] == '(') {
				if constexpr (A[1] == '?') {
                     if constexpr (A[2] == '=') return regex_tokenizer<A += 3, 0, Tokens..., nested_graph_token<lookahead_assertion>>{};
                     else if constexpr (A[2] == '!') return regex_tokenizer<A += 3, 0, Tokens..., nested_graph_token<negative_lookahead_assertion>>{};
                     else if constexpr (A[2] == '<' && A[3] == '=') return regex_tokenizer<A += 4, 0, Tokens..., nested_graph_token<lookbehind_assertion>>{};
                     else if constexpr (A[2] == '<' && A[3] == '!') return regex_tokenizer<A += 4, 0, Tokens..., nested_graph_token<negative_lookbehind_assertion>>{};
                } else {
					return regex_tokenizer<A += 1, 0, Tokens..., nested_graph_token<unit_token>>{};
                }
            } else if constexpr (A[0] == '\\') {
                if constexpr (A[1] == 'b') {
                    if constexpr (InsideClass) regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\b'>>{};
                    else return regex_tokenizer<A += 2, 0, Tokens..., boundary_assertion<'b'>>{};
                }
                else if constexpr (A[1] == 'B') return regex_tokenizer<A += 2, Next, Tokens..., boundary_assertion<'B'>>{};
                else if constexpr (A[1] == 's') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'s'>>{};
                else if constexpr (A[1] == 'S') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'S'>>{};
                else if constexpr (A[1] == 'd') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'d'>>{};
                else if constexpr (A[1] == 'D') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'D'>>{};
                else if constexpr (A[1] == 'w') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'w'>>{};
                else if constexpr (A[1] == 'W') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'W'>>{};
                else if constexpr (A[1] == 'X') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'X'>>{};
                else if constexpr (A[1] == 'C') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'C'>>{};
                else if constexpr (A[1] == 'R') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'R'>>{};
                else if constexpr (A[1] == 'N') return regex_tokenizer<A += 2, Next, Tokens..., meta_character_parser<'N'>>{};
                else if constexpr (A[1] == 'n') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\n'>>{};
                else if constexpr (A[1] == 't') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\t'>>{};
                else if constexpr (A[1] == '0') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\0'>>{};
                else if constexpr (A[1] == 'r') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\r'>>{};
                else if constexpr (A[1] == 'f') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\f'>>{};
                else if constexpr (A[1] == 'a') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\a'>>{};
                else if constexpr (A[1] == 'v') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\v'>>{};
                else if constexpr (A[1] == '.') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'.'>>{};
                else if constexpr (A[1] == '^') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'^'>>{};
                else if constexpr (A[1] == '$') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'$'>>{};
                else if constexpr (A[1] == '+') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'+'>>{};
                else if constexpr (A[1] == '*') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'*'>>{};
                else if constexpr (A[1] == '?') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'?'>>{};
                else if constexpr (A[1] == '(') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'('>>{};
                else if constexpr (A[1] == ')') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<')'>>{};
                else if constexpr (A[1] == '[') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'['>>{};
                else if constexpr (A[1] == ']') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<']'>>{};
                else if constexpr (A[1] == '{') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'{'>>{};
                else if constexpr (A[1] == '}') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'}'>>{};
                else if constexpr (A[1] == '\\') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'\\'>>{};
                else if constexpr (A[1] == '|') return regex_tokenizer<A += 2, Next, Tokens..., single_character_parser<'|'>>{};
            } else {
                return regex_tokenizer<A += 1, Next, Tokens..., single_character_parser<A[0]>>{};
            }
        }

        using type = decltype(tokenize())::type;
    };

    // ------------------------------------------------

    /**
        Compile the regex literal into a constexpr regex parser.

        @tparam A           the regex string literal.
     */
    template<regex_literal A>
    using regex = typename regex_parser<regex_length<0, 0>, typename regex_tokenizer<A>::type>::type;

    // ------------------------------------------------

}

// ------------------------------------------------
