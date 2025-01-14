/*
    __ _____ _____ _____
 __|  |   __|     |   | |  JSON for Modern C++ (test suite)
|  |  |__   |  |  | | | |  version 2.1.1
|_____|_____|_____|_|___|  https://github.com/nlohmann/json

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
Copyright (c) 2013-2017 Niels Lohmann <http://nlohmann.me>.

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "catch.hpp"

#define private public
#include "json.hpp"
using nlohmann::json;

#include <fstream>
#include <list>
#include <cstdio>

namespace
{
struct nocopy
{
    nocopy() = default;
    nocopy(const nocopy&) = delete;

    int val = 0;

    friend void to_json(json& j, const nocopy& n)
    {
        j = {{"val", n.val}};
    }
};
}

TEST_CASE("regression tests")
{
    SECTION("issue #60 - Double quotation mark is not parsed correctly")
    {
        SECTION("escape_dobulequote")
        {
            auto s = "[\"\\\"foo\\\"\"]";
            json j = json::parse(s);
            auto expected = R"(["\"foo\""])"_json;
            CHECK(j == expected);
        }
    }

    SECTION("issue #70 - Handle infinity and NaN cases")
    {
        /*
        SECTION("NAN value")
        {
            CHECK(json(NAN) == json());
            CHECK(json(json::number_float_t(NAN)) == json());
        }

        SECTION("infinity")
        {
            CHECK(json(INFINITY) == json());
            CHECK(json(json::number_float_t(INFINITY)) == json());
        }
        */

        // With 3.0.0, the semantics of this changed: NAN and infinity are
        // stored properly inside the JSON value (no exception or conversion
        // to null), but are serialized as null.
        SECTION("NAN value")
        {
            json j1 = NAN;
            CHECK(j1.is_number_float());
            json::number_float_t f1 = j1;
            CHECK(std::isnan(f1));

            json j2 = json::number_float_t(NAN);
            CHECK(j2.is_number_float());
            json::number_float_t f2 = j2;
            CHECK(std::isnan(f2));
        }

        SECTION("infinity")
        {
            json j1 = INFINITY;
            CHECK(j1.is_number_float());
            json::number_float_t f1 = j1;
            CHECK(not std::isfinite(f1));

            json j2 = json::number_float_t(INFINITY);
            CHECK(j2.is_number_float());
            json::number_float_t f2 = j2;
            CHECK(not std::isfinite(f2));
        }
    }

    SECTION("pull request #71 - handle enum type")
    {
        enum { t = 0, u = 102};
        json j = json::array();
        j.push_back(t);

        // maybe this is not the place to test this?
        json j2 = u;

        auto anon_enum_value = j2.get<decltype(u)>();
        CHECK(u == anon_enum_value);

        // check if the actual value was stored
        CHECK(j2 == 102);

        static_assert(std::is_same<decltype(anon_enum_value), decltype(u)>::value, "");

        j.push_back(json::object(
        {
            {"game_type", t}
        }));
    }

    SECTION("issue #76 - dump() / parse() not idempotent")
    {
        // create JSON object
        json fields;
        fields["one"] = std::string("one");
        fields["two"] = std::string("two three");
        fields["three"] = std::string("three \"four\"");

        // create another JSON object by deserializing the serialization
        std::string payload = fields.dump();
        json parsed_fields = json::parse(payload);

        // check individual fields to match both objects
        CHECK(parsed_fields["one"] == fields["one"]);
        CHECK(parsed_fields["two"] == fields["two"]);
        CHECK(parsed_fields["three"] == fields["three"]);

        // check individual fields to match original input
        CHECK(parsed_fields["one"] == std::string("one"));
        CHECK(parsed_fields["two"] == std::string("two three"));
        CHECK(parsed_fields["three"] == std::string("three \"four\""));

        // check equality of the objects
        CHECK(parsed_fields == fields);

        // check equality of the serialized objects
        CHECK(fields.dump() == parsed_fields.dump());

        // check everything in one line
        CHECK(fields == json::parse(fields.dump()));
    }

    SECTION("issue #82 - lexer::get_number return NAN")
    {
        const auto content = R"(
        {
            "Test":"Test1",
            "Number":100,
            "Foo":42.42
        })";

        std::stringstream ss;
        ss << content;
        json j;
        ss >> j;

        std::string test = j["Test"];
        CHECK(test == "Test1");
        int number = j["Number"];
        CHECK(number == 100);
        float foo = j["Foo"];
        CHECK(foo == Approx(42.42));
    }

    SECTION("issue #89 - nonstandard integer type")
    {
        // create JSON class with nonstandard integer number type
        using custom_json =
            nlohmann::basic_json<std::map, std::vector, std::string, bool, int32_t, uint32_t, float>;
        custom_json j;
        j["int_1"] = 1;
        // we need to cast to int to compile with Catch - the value is int32_t
        CHECK(static_cast<int>(j["int_1"]) == 1);

        // tests for correct handling of non-standard integers that overflow the type selected by the user

        // unsigned integer object creation - expected to wrap and still be stored as an integer
        j = 4294967296U; // 2^32
        CHECK(static_cast<int>(j.type()) == static_cast<int>(custom_json::value_t::number_unsigned));
        CHECK(j.get<uint32_t>() == 0);  // Wrap

        // unsigned integer parsing - expected to overflow and be stored as a float
        j = custom_json::parse("4294967296"); // 2^32
        CHECK(static_cast<int>(j.type()) == static_cast<int>(custom_json::value_t::number_float));
        CHECK(j.get<float>() == 4294967296.0f);

        // integer object creation - expected to wrap and still be stored as an integer
        j = -2147483649LL; // -2^31-1
        CHECK(static_cast<int>(j.type()) == static_cast<int>(custom_json::value_t::number_integer));
        CHECK(j.get<int32_t>() == 2147483647);  // Wrap

        // integer parsing - expected to overflow and be stored as a float with rounding
        j = custom_json::parse("-2147483649"); // -2^31
        CHECK(static_cast<int>(j.type()) == static_cast<int>(custom_json::value_t::number_float));
        CHECK(j.get<float>() == -2147483650.0f);
    }

    SECTION("issue #93 reverse_iterator operator inheritance problem")
    {
        {
            json a = {1, 2, 3};
            json::reverse_iterator rit = a.rbegin();
            ++rit;
            CHECK(*rit == json(2));
            CHECK(rit.value() == json(2));
        }
        {
            json a = {1, 2, 3};
            json::reverse_iterator rit = ++a.rbegin();
            CHECK(*rit == json(2));
            CHECK(rit.value() == json(2));
        }
        {
            json a = {1, 2, 3};
            json::reverse_iterator rit = a.rbegin();
            ++rit;
            json b = {0, 0, 0};
            std::transform(rit, a.rend(), b.rbegin(), [](json el)
            {
                return el;
            });
            CHECK(b == json({0, 1, 2}));
        }
        {
            json a = {1, 2, 3};
            json b = {0, 0, 0};
            std::transform(++a.rbegin(), a.rend(), b.rbegin(), [](json el)
            {
                return el;
            });
            CHECK(b == json({0, 1, 2}));
        }
    }

    SECTION("issue #100 - failed to iterator json object with reverse_iterator")
    {
        json config =
        {
            { "111", 111 },
            { "112", 112 },
            { "113", 113 }
        };

        std::stringstream ss;

        for (auto it = config.begin(); it != config.end(); ++it)
        {
            ss << it.key() << ": " << it.value() << '\n';
        }

        for (auto it = config.rbegin(); it != config.rend(); ++it)
        {
            ss << it.key() << ": " << it.value() << '\n';
        }

        CHECK(ss.str() == "111: 111\n112: 112\n113: 113\n113: 113\n112: 112\n111: 111\n");
    }

    SECTION("issue #101 - binary string causes numbers to be dumped as hex")
    {
        int64_t number = 10;
        std::string bytes{"\x00" "asdf\n", 6};
        json j;
        j["int64"] = number;
        j["binary string"] = bytes;
        // make sure the number is really printed as decimal "10" and not as
        // hexadecimal "a"
        CHECK(j.dump() == "{\"binary string\":\"\\u0000asdf\\n\",\"int64\":10}");
    }

    SECTION("issue #111 - subsequent unicode chars")
    {
        std::string bytes{0x7, 0x7};
        json j;
        j["string"] = bytes;
        CHECK(j["string"] == "\u0007\u0007");
    }

    SECTION("issue #144 - implicit assignment to std::string fails")
    {
        json o = {{"name", "value"}};

        std::string s1 = o["name"];
        CHECK(s1 == "value");

        std::string s2;
        s2 = o["name"];

        CHECK(s2 == "value");
    }

    SECTION("issue #146 - character following a surrogate pair is skipped")
    {
        CHECK(json::parse("\"\\ud80c\\udc60abc\"").get<json::string_t>() == u8"\U00013060abc");
    }

    SECTION("issue #171 - Cannot index by key of type static constexpr const char*")
    {
        json j;

        // Non-const access with key as "char []"
        char array_key[] = "Key1";
        CHECK_NOTHROW(j[array_key] = 1);
        CHECK(j[array_key] == json(1));

        // Non-const access with key as "const char[]"
        const char const_array_key[] = "Key2";
        CHECK_NOTHROW(j[const_array_key] = 2);
        CHECK(j[const_array_key] == json(2));

        // Non-const access with key as "char *"
        char _ptr_key[] = "Key3";
        char* ptr_key = &_ptr_key[0];
        CHECK_NOTHROW(j[ptr_key] = 3);
        CHECK(j[ptr_key] == json(3));

        // Non-const access with key as "const char *"
        const char* const_ptr_key = "Key4";
        CHECK_NOTHROW(j[const_ptr_key] = 4);
        CHECK(j[const_ptr_key] == json(4));

        // Non-const access with key as "static constexpr const char *"
        static constexpr const char* constexpr_ptr_key = "Key5";
        CHECK_NOTHROW(j[constexpr_ptr_key] = 5);
        CHECK(j[constexpr_ptr_key] == json(5));

        const json j_const = j;

        // Const access with key as "char []"
        CHECK(j_const[array_key] == json(1));

        // Const access with key as "const char[]"
        CHECK(j_const[const_array_key] == json(2));

        // Const access with key as "char *"
        CHECK(j_const[ptr_key] == json(3));

        // Const access with key as "const char *"
        CHECK(j_const[const_ptr_key] == json(4));

        // Const access with key as "static constexpr const char *"
        CHECK(j_const[constexpr_ptr_key] == json(5));
    }

    SECTION("issue #186 miloyip/nativejson-benchmark: floating-point parsing")
    {
        json j;

        j = json::parse("-0.0");
        CHECK(j.get<double>() == -0.0);

        j = json::parse("2.22507385850720113605740979670913197593481954635164564e-308");
        CHECK(j.get<double>() == 2.2250738585072009e-308);

        j = json::parse("0.999999999999999944488848768742172978818416595458984374");
        CHECK(j.get<double>() == 0.99999999999999989);

        j = json::parse("1.00000000000000011102230246251565404236316680908203126");
        CHECK(j.get<double>() == 1.00000000000000022);

        j = json::parse("7205759403792793199999e-5");
        CHECK(j.get<double>() == 72057594037927928.0);

        j = json::parse("922337203685477529599999e-5");
        CHECK(j.get<double>() == 9223372036854774784.0);

        j = json::parse("1014120480182583464902367222169599999e-5");
        CHECK(j.get<double>() == 10141204801825834086073718800384.0);

        j = json::parse("5708990770823839207320493820740630171355185151999e-3");
        CHECK(j.get<double>() == 5708990770823838890407843763683279797179383808.0);

        // create JSON class with nonstandard float number type

        // float
        nlohmann::basic_json<std::map, std::vector, std::string, bool, int32_t, uint32_t, float> j_float =
            1.23e25f;
        CHECK(j_float.get<float>() == 1.23e25f);

        // double
        nlohmann::basic_json<std::map, std::vector, std::string, bool, int64_t, uint64_t, double> j_double =
            1.23e35;
        CHECK(j_double.get<double>() == 1.23e35);

        // long double
        nlohmann::basic_json<std::map, std::vector, std::string, bool, int64_t, uint64_t, long double>
        j_long_double = 1.23e45L;
        CHECK(j_long_double.get<long double>() == 1.23e45L);
    }

    SECTION("issue #228 - double values are serialized with commas as decimal points")
    {
        json j1a = 2312.42;
        json j1b = json::parse("2312.42");

        json j2a = 2342e-2;
        //issue #230
        //json j2b = json::parse("2342e-2");

        json j3a = 10E3;
        json j3b = json::parse("10E3");
        json j3c = json::parse("10e3");

        // class to create a locale that would use a comma for decimals
        class CommaDecimalSeparator : public std::numpunct<char>
        {
          protected:
            char do_decimal_point() const override
            {
                return ',';
            }

            char do_thousands_sep() const override
            {
                return '.';
            }

            std::string do_grouping() const override
            {
                return "\03";
            }
        };

        // change locale to mess with decimal points
        auto orig_locale = std::locale::global(std::locale(std::locale(), new CommaDecimalSeparator));

        CHECK(j1a.dump() == "2312.42");
        CHECK(j1b.dump() == "2312.42");

        // check if locale is properly reset
        std::stringstream ss;
        ss.imbue(std::locale(std::locale(), new CommaDecimalSeparator));
        ss << 4712.11;
        CHECK(ss.str() == "4.712,11");
        ss << j1a;
        CHECK(ss.str() == "4.712,112312.42");
        ss << 47.11;
        CHECK(ss.str() == "4.712,112312.4247,11");

        CHECK(j2a.dump() == "23.42");
        //issue #230
        //CHECK(j2b.dump() == "23.42");

        CHECK(j3a.dump() == "10000.0");
        CHECK(j3b.dump() == "10000.0");
        CHECK(j3c.dump() == "10000.0");
        //CHECK(j3b.dump() == "1E04"); // roundtrip error
        //CHECK(j3c.dump() == "1e04"); // roundtrip error

        std::locale::global(orig_locale);
    }

    SECTION("issue #378 - locale-independent num-to-str")
    {
        setlocale(LC_NUMERIC, "de_DE.UTF-8");

        // verify that dumped correctly with '.' and no grouping
        const json j1 = 12345.67;
        CHECK(json(12345.67).dump() == "12345.67");
        setlocale(LC_NUMERIC, "C");
    }

    SECTION("issue #379 - locale-independent str-to-num")
    {
        setlocale(LC_NUMERIC, "de_DE.UTF-8");

        // verify that parsed correctly despite using strtod internally
        CHECK(json::parse("3.14").get<double>() == 3.14);

        // check a different code path
        CHECK(json::parse("1.000000000000000000000000000000000000000000000000000000000000000000000000").get<double>() == 1.0);
    }

    SECTION("issue #233 - Can't use basic_json::iterator as a base iterator for std::move_iterator")
    {
        json source = {"a", "b", "c"};
        json expected = {"a", "b"};
        json dest;

        std::copy_n(std::make_move_iterator(source.begin()), 2, std::back_inserter(dest));

        CHECK(dest == expected);
    }

    SECTION("issue #235 - ambiguous overload for 'push_back' and 'operator+='")
    {
        json data = {{"key", "value"}};
        data.push_back({"key2", "value2"});
        data += {"key3", "value3"};

        CHECK(data == json({{"key", "value"}, {"key2", "value2"}, {"key3", "value3"}}));
    }

    SECTION("issue #269 - diff generates incorrect patch when removing multiple array elements")
    {
        json doc = R"( { "arr1": [1, 2, 3, 4] } )"_json;
        json expected = R"( { "arr1": [1, 2] } )"_json;

        // check roundtrip
        CHECK(doc.patch(json::diff(doc, expected)) == expected);
    }

    SECTION("issue #283 - value() does not work with _json_pointer types")
    {
        json j =
        {
            {"object", {{"key1", 1}, {"key2", 2}}},
        };

        int at_integer = j.at("/object/key2"_json_pointer);
        int val_integer = j.value("/object/key2"_json_pointer, 0);

        CHECK(at_integer == val_integer);
    }

    SECTION("issue #304 - Unused variable warning")
    {
        // code triggered a "warning: unused variable" warning and is left
        // here to avoid the warning in the future
        json object;
        json patch = json::array();
        object = object.patch(patch);
    }

    SECTION("issue #306 - Parsing fails without space at end of file")
    {
        for (auto filename :
                {
                    "test/data/regression/broken_file.json",
                    "test/data/regression/working_file.json"
                })
        {
            CAPTURE(filename);
            json j;
            std::ifstream f(filename);
            CHECK_NOTHROW(f >> j);
        }
    }

    SECTION("issue #310 - make json_benchmarks no longer working in 2.0.4")
    {
        for (auto filename :
                {
                    "test/data/regression/floats.json",
                    "test/data/regression/signed_ints.json",
                    "test/data/regression/unsigned_ints.json"
                })
        {
            CAPTURE(filename);
            json j;
            std::ifstream f(filename);
            CHECK_NOTHROW(f >> j);
        }
    }

    SECTION("issue #323 - add nested object capabilities to pointers")
    {
        json j;
        j["/this/that/2"_json_pointer] = 27;
        CHECK(j == json({{"this", {{"that", {nullptr, nullptr, 27}}}}}));
    }

    SECTION("issue #329 - serialized value not always can be parsed")
    {
        CHECK_THROWS_AS(json::parse("22e2222"), json::out_of_range&);
        CHECK_THROWS_WITH(json::parse("22e2222"),
                          "[json.exception.out_of_range.406] number overflow parsing '22e2222'");
    }

    SECTION("issue #366 - json::parse on failed stream gets stuck")
    {
        std::ifstream f("file_not_found.json");
        CHECK_THROWS_AS(json::parse(f), json::parse_error&);
        CHECK_THROWS_WITH(json::parse(f), "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
    }

    SECTION("issue #367 - calling stream at EOF")
    {
        std::stringstream ss;
        json j;
        ss << "123";
        CHECK_NOTHROW(ss >> j);

        // see https://github.com/nlohmann/json/issues/367#issuecomment-262841893:
        // ss is not at EOF; this yielded an error before the fix
        // (threw basic_string::append). No, it should just throw
        // a parse error because of the EOF.
        CHECK_THROWS_AS(ss >> j, json::parse_error&);
        CHECK_THROWS_WITH(ss >> j,
                          "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
    }

    SECTION("issue #367 - behavior of operator>> should more closely resemble that of built-in overloads")
    {
        SECTION("(empty)")
        {
            std::stringstream ss;
            json j;
            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("(whitespace)")
        {
            std::stringstream ss;
            ss << "   ";
            json j;
            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("one value")
        {
            std::stringstream ss;
            ss << "111";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 111);

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("one value + whitespace")
        {
            std::stringstream ss;
            ss << "222 \t\n";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 222);

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("whitespace + one value")
        {
            std::stringstream ss;
            ss << "\n\t 333";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 333);

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("three values")
        {
            std::stringstream ss;
            ss << " 111 \n222\n \n  333";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 111);
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 222);
            CHECK_NOTHROW(ss >> j);
            CHECK(j == 333);

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("literals without whitespace")
        {
            std::stringstream ss;
            ss << "truefalsenull\"\"";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == true);
            CHECK_NOTHROW(ss >> j);
            CHECK(j == false);
            CHECK_NOTHROW(ss >> j);
            CHECK(j == nullptr);
            CHECK_NOTHROW(ss >> j);
            CHECK(j == "");

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("example from #529")
        {
            std::stringstream ss;
            ss << "{\n    \"one\"   : 1,\n    \"two\"   : 2\n}\n{\n    \"three\" : 3\n}";
            json j;
            CHECK_NOTHROW(ss >> j);
            CHECK(j == json({{"one", 1}, {"two", 2}}));
            CHECK_NOTHROW(ss >> j);
            CHECK(j == json({{"three", 3}}));

            CHECK_THROWS_AS(ss >> j, json::parse_error&);
            CHECK_THROWS_WITH(ss >> j,
                              "[json.exception.parse_error.101] parse error at 1: syntax error - unexpected end of input; expected '[', '{', or a literal");
        }

        SECTION("second example from #529")
        {
            std::string str = "{\n\"one\"   : 1,\n\"two\"   : 2\n}\n{\n\"three\" : 3\n}";

            {
                std::ofstream file("test.json");
                file << str;
            }

            std::ifstream stream("test.json", std::ifstream::in);
            json val;

            size_t i = 0;
            while (stream.peek() != EOF)
            {
                CAPTURE(i);
                CHECK_NOTHROW(stream >> val);

                CHECK(i < 2);

                if (i == 0)
                {
                    CHECK(val == json({{"one", 1}, {"two", 2}}));
                }

                if (i == 1)
                {
                    CHECK(val == json({{"three", 3}}));
                }

                ++i;
            }

            std::remove("test.json");
        }
    }

    SECTION("issue #389 - Integer-overflow (OSS-Fuzz issue 267)")
    {
        // original test case
        json j1 = json::parse("-9223372036854775808");
        CHECK(j1.is_number_integer());
        CHECK(j1.get<json::number_integer_t>() == INT64_MIN);

        // edge case (+1; still an integer)
        json j2 = json::parse("-9223372036854775807");
        CHECK(j2.is_number_integer());
        CHECK(j2.get<json::number_integer_t>() == INT64_MIN + 1);

        // edge case (-1; overflow -> floats)
        json j3 = json::parse("-9223372036854775809");
        CHECK(j3.is_number_float());
    }

    SECTION("issue #380 - bug in overflow detection when parsing integers")
    {
        json j = json::parse("166020696663385964490");
        CHECK(j.is_number_float());
        CHECK(j.dump() == "1.66020696663386e+20");
    }

    SECTION("issue #405 - Heap-buffer-overflow (OSS-Fuzz issue 342)")
    {
        // original test case
        std::vector<uint8_t> vec {0x65, 0xf5, 0x0a, 0x48, 0x21};
        CHECK_THROWS_AS(json::from_cbor(vec), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec),
                          "[json.exception.parse_error.110] parse error at 6: unexpected end of input");
    }

    SECTION("issue #407 - Heap-buffer-overflow (OSS-Fuzz issue 343)")
    {
        // original test case: incomplete float64
        std::vector<uint8_t> vec1 {0xcb, 0x8f, 0x0a};
        CHECK_THROWS_AS(json::from_msgpack(vec1), json::parse_error&);
        CHECK_THROWS_WITH(json::from_msgpack(vec1),
                          "[json.exception.parse_error.110] parse error at 4: unexpected end of input");

        // related test case: incomplete float32
        std::vector<uint8_t> vec2 {0xca, 0x8f, 0x0a};
        CHECK_THROWS_AS(json::from_msgpack(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_msgpack(vec2),
                          "[json.exception.parse_error.110] parse error at 4: unexpected end of input");

        // related test case: incomplete Half-Precision Float (CBOR)
        std::vector<uint8_t> vec3 {0xf9, 0x8f};
        CHECK_THROWS_AS(json::from_cbor(vec3), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec3),
                          "[json.exception.parse_error.110] parse error at 3: unexpected end of input");

        // related test case: incomplete Single-Precision Float (CBOR)
        std::vector<uint8_t> vec4 {0xfa, 0x8f, 0x0a};
        CHECK_THROWS_AS(json::from_cbor(vec4), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec4),
                          "[json.exception.parse_error.110] parse error at 4: unexpected end of input");

        // related test case: incomplete Double-Precision Float (CBOR)
        std::vector<uint8_t> vec5 {0xfb, 0x8f, 0x0a};
        CHECK_THROWS_AS(json::from_cbor(vec5), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec5),
                          "[json.exception.parse_error.110] parse error at 4: unexpected end of input");
    }

    SECTION("issue #408 - Heap-buffer-overflow (OSS-Fuzz issue 344)")
    {
        // original test case
        std::vector<uint8_t> vec1 {0x87};
        CHECK_THROWS_AS(json::from_msgpack(vec1), json::parse_error&);
        CHECK_THROWS_WITH(json::from_msgpack(vec1),
                          "[json.exception.parse_error.110] parse error at 2: unexpected end of input");

        // more test cases for MessagePack
        for (auto b :
                {
                    0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, // fixmap
                    0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, // fixarray
                    0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // fixstr
                    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf
                })
        {
            std::vector<uint8_t> vec(1, static_cast<uint8_t>(b));
            CHECK_THROWS_AS(json::from_msgpack(vec), json::parse_error&);
        }

        // more test cases for CBOR
        for (auto b :
                {
                    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
                    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, // UTF-8 string
                    0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
                    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, // array
                    0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
                    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7 // map
                })
        {
            std::vector<uint8_t> vec(1, static_cast<uint8_t>(b));
            CHECK_THROWS_AS(json::from_cbor(vec), json::parse_error&);
        }

        // special case: empty input
        std::vector<uint8_t> vec2;
        CHECK_THROWS_AS(json::from_cbor(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec2),
                          "[json.exception.parse_error.110] parse error at 1: unexpected end of input");
        CHECK_THROWS_AS(json::from_msgpack(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_msgpack(vec2),
                          "[json.exception.parse_error.110] parse error at 1: unexpected end of input");
    }

    SECTION("issue #411 - Heap-buffer-overflow (OSS-Fuzz issue 366)")
    {
        // original test case: empty UTF-8 string (indefinite length)
        std::vector<uint8_t> vec1 {0x7f};
        CHECK_THROWS_AS(json::from_cbor(vec1), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec1),
                          "[json.exception.parse_error.110] parse error at 2: unexpected end of input");

        // related test case: empty array (indefinite length)
        std::vector<uint8_t> vec2 {0x9f};
        CHECK_THROWS_AS(json::from_cbor(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec2),
                          "[json.exception.parse_error.110] parse error at 2: unexpected end of input");

        // related test case: empty map (indefinite length)
        std::vector<uint8_t> vec3 {0xbf};
        CHECK_THROWS_AS(json::from_cbor(vec3), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec3),
                          "[json.exception.parse_error.110] parse error at 2: unexpected end of input");
    }

    SECTION("issue #412 - Heap-buffer-overflow (OSS-Fuzz issue 367)")
    {
        // original test case
        std::vector<uint8_t> vec
        {
            0xab, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
            0x98, 0x98, 0x98, 0x98, 0x98, 0x00, 0x00, 0x00,
            0x60, 0xab, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98,
            0x98, 0x98, 0x98, 0x98, 0x98, 0x00, 0x00, 0x00,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0xa0, 0x9f,
            0x9f, 0x97, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
            0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60
        };
        CHECK_THROWS_AS(json::from_cbor(vec), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec),
                          "[json.exception.parse_error.113] parse error at 2: expected a CBOR string; last byte: 0x98");

        // related test case: nonempty UTF-8 string (indefinite length)
        std::vector<uint8_t> vec1 {0x7f, 0x61, 0x61};
        CHECK_THROWS_AS(json::from_cbor(vec1), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec1),
                          "[json.exception.parse_error.110] parse error at 4: unexpected end of input");

        // related test case: nonempty array (indefinite length)
        std::vector<uint8_t> vec2 {0x9f, 0x01};
        CHECK_THROWS_AS(json::from_cbor(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec2),
                          "[json.exception.parse_error.110] parse error at 3: unexpected end of input");

        // related test case: nonempty map (indefinite length)
        std::vector<uint8_t> vec3 {0xbf, 0x61, 0x61, 0x01};
        CHECK_THROWS_AS(json::from_cbor(vec3), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec3),
                          "[json.exception.parse_error.110] parse error at 5: unexpected end of input");
    }

    SECTION("issue #414 - compare with literal 0)")
    {
#define CHECK_TYPE(v) \
    CHECK((json(v) == v));\
    CHECK((v == json(v)));\
    CHECK_FALSE((json(v) != v));\
    CHECK_FALSE((v != json(v)));

        CHECK_TYPE(nullptr);
        CHECK_TYPE(0);
        CHECK_TYPE(0u);
        CHECK_TYPE(0L);
        CHECK_TYPE(0.0);
        CHECK_TYPE("");

#undef CHECK_TYPE
    }

    SECTION("issue #416 - Use-of-uninitialized-value (OSS-Fuzz issue 377)")
    {
        // original test case
        std::vector<uint8_t> vec1
        {
            0x94, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa,
            0x3a, 0x96, 0x96, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
            0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0x71,
            0xb4, 0xb4, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0x3a,
            0x96, 0x96, 0xb4, 0xb4, 0xfa, 0x94, 0x94, 0x61,
            0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0xfa
        };
        CHECK_THROWS_AS(json::from_cbor(vec1), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec1),
                          "[json.exception.parse_error.113] parse error at 13: expected a CBOR string; last byte: 0xB4");

        // related test case: double-precision
        std::vector<uint8_t> vec2
        {
            0x94, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa,
            0x3a, 0x96, 0x96, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4,
            0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0xb4, 0x71,
            0xb4, 0xb4, 0xfa, 0xfa, 0xfa, 0xfa, 0xfa, 0x3a,
            0x96, 0x96, 0xb4, 0xb4, 0xfa, 0x94, 0x94, 0x61,
            0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0xfb
        };
        CHECK_THROWS_AS(json::from_cbor(vec2), json::parse_error&);
        CHECK_THROWS_WITH(json::from_cbor(vec2),
                          "[json.exception.parse_error.113] parse error at 13: expected a CBOR string; last byte: 0xB4");
    }

    SECTION("issue #452 - Heap-buffer-overflow (OSS-Fuzz issue 585)")
    {
        std::vector<uint8_t> vec = {'-', '0', '1', '2', '2', '7', '4'};
        CHECK_THROWS_AS(json::parse(vec), json::parse_error&);
    }

    SECTION("issue #454 - doubles are printed as integers")
    {
        json j = R"({"bool_value":true,"double_value":2.0,"int_value":10,"level1":{"list_value":[3,"hi",false],"tmp":5.0},"string_value":"hello"})"_json;
        CHECK(j["double_value"].is_number_float());
    }

    SECTION("issue #464 - VS2017 implicit to std::string conversion fix")
    {
        json v = "test";
        std::string test;
        test = v;
        CHECK(v == "test");
    }

    SECTION("issue #465 - roundtrip error while parsing 1000000000000000010E5")
    {
        json j1 = json::parse("1000000000000000010E5");
        std::string s1 = j1.dump();
        json j2 = json::parse(s1);
        std::string s2 = j2.dump();
        CHECK(s1 == s2);
    }

    SECTION("issue #473 - inconsistent behavior in conversion to array type")
    {
        json j_array = {1, 2, 3, 4};
        json j_number = 42;
        json j_null = nullptr;

        SECTION("std::vector")
        {
            auto create = [](const json & j)
            {
                std::vector<int> v = j;
            };

            CHECK_NOTHROW(create(j_array));
            CHECK_THROWS_AS(create(j_number), json::type_error&);
            CHECK_THROWS_WITH(create(j_number), "[json.exception.type_error.302] type must be array, but is number");
            CHECK_THROWS_AS(create(j_null), json::type_error&);
            CHECK_THROWS_WITH(create(j_null), "[json.exception.type_error.302] type must be array, but is null");
        }

        SECTION("std::list")
        {
            auto create = [](const json & j)
            {
                std::list<int> v = j;
            };

            CHECK_NOTHROW(create(j_array));
            CHECK_THROWS_AS(create(j_number), json::type_error&);
            CHECK_THROWS_WITH(create(j_number), "[json.exception.type_error.302] type must be array, but is number");
            CHECK_THROWS_AS(create(j_null), json::type_error&);
            CHECK_THROWS_WITH(create(j_null), "[json.exception.type_error.302] type must be array, but is null");
        }

        SECTION("std::forward_list")
        {
            auto create = [](const json & j)
            {
                std::forward_list<int> v = j;
            };

            CHECK_NOTHROW(create(j_array));
            CHECK_THROWS_AS(create(j_number), json::type_error&);
            CHECK_THROWS_WITH(create(j_number), "[json.exception.type_error.302] type must be array, but is number");
            CHECK_THROWS_AS(create(j_null), json::type_error&);
            CHECK_THROWS_WITH(create(j_null), "[json.exception.type_error.302] type must be array, but is null");
        }
    }

    SECTION("issue #486 - json::value_t can't be a map's key type in VC++ 2015")
    {
        // the code below must compile with MSVC
        std::map<json::value_t, std::string> jsonTypes ;
        jsonTypes[json::value_t::array] = "array";
    }

    SECTION("issue #494 - conversion from vector<bool> to json fails to build")
    {
        std::vector<bool> boolVector = {false, true, false, false};
        json j;
        j["bool_vector"] = boolVector;

        CHECK(j["bool_vector"].dump() == "[false,true,false,false]");
    }

    /* NOTE: m_line_buffer is not used any more
    SECTION("issue #495 - fill_line_buffer incorrectly tests m_stream for eof but not fail or bad bits")
    {
        SECTION("setting failbit")
        {
            std::stringstream ss;
            ss << "[1,2,3\n,4,5]";

            json::lexer l(ss);

            CHECK(l.m_line_buffer == "[1,2,3\n");

            l.m_stream->setstate(std::ios_base::failbit);

            CHECK_THROWS_AS(l.fill_line_buffer(), json::parse_error&);
            CHECK_THROWS_WITH(l.fill_line_buffer(), "[json.exception.parse_error.111] parse error: bad input stream");
        }

        SECTION("setting badbit")
        {
            std::stringstream ss;
            ss << "[1,2,3\n,4,5]";

            json::lexer l(ss);

            CHECK(l.m_line_buffer == "[1,2,3\n");

            l.m_stream->setstate(std::ios_base::badbit);

            CHECK_THROWS_AS(l.fill_line_buffer(), json::parse_error&);
            CHECK_THROWS_WITH(l.fill_line_buffer(), "[json.exception.parse_error.111] parse error: bad input stream");
        }
    }
     */

    SECTION("issue #504 - assertion error (OSS-Fuzz 856)")
    {
        std::vector<uint8_t> vec1 = {0xf9, 0xff, 0xff, 0x4a, 0x3a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x37, 0x02, 0x38};
        json j1 = json::from_cbor(vec1, false);

        // step 2: round trip
        std::vector<uint8_t> vec2 = json::to_cbor(j1);

        // parse serialization
        json j2 = json::from_cbor(vec2);

        // NaN is dumped to "null"
        CHECK(j2.is_number_float());
        CHECK(std::isnan(j2.get<json::number_float_t>()));
        CHECK(j2.dump() == "null");

        // check if serializations match
        CHECK(json::to_cbor(j2) == vec2);
    }

    SECTION("issue #512 - use of overloaded operator '<=' is ambiguous")
    {
        json j;
        j["a"] = 5;

        // json op scalar
        CHECK(j["a"] == 5);
        CHECK(j["a"] != 4);

        CHECK(j["a"] <= 7);
        CHECK(j["a"] <  7);
        CHECK(j["a"] >= 3);
        CHECK(j["a"] >  3);


        CHECK(not(j["a"] <= 4));
        CHECK(not(j["a"] <  4));
        CHECK(not(j["a"] >= 6));
        CHECK(not(j["a"] >  6));

        // scalar op json
        CHECK(5 == j["a"]);
        CHECK(4 != j["a"]);

        CHECK(7 >= j["a"]);
        CHECK(7 >  j["a"]);
        CHECK(3 <= j["a"]);
        CHECK(3 <  j["a"]);

        CHECK(not(4 >= j["a"]));
        CHECK(not(4 >  j["a"]));
        CHECK(not(6 <= j["a"]));
        CHECK(not(6 <  j["a"]));
    }

    SECTION("issue #575 - heap-buffer-overflow (OSS-Fuzz 1400)")
    {
        std::vector<uint8_t> vec = {'"', '\\', '"', 'X', '"', '"'};
        CHECK_THROWS_AS(json::parse(vec), json::parse_error&);
    }

    SECTION("issue #600 - how does one convert a map in Json back to std::map?")
    {
        SECTION("example 1")
        {
            // create a map
            std::map<std::string, int> m1 {{"key", 1}};

            // create and print a JSON from the map
            json j = m1;

            // get the map out of JSON
            std::map<std::string, int> m2 = j;

            // make sure the roundtrip succeeds
            CHECK(m1 == m2);
        }

        SECTION("example 2")
        {
            // create a map
            std::map<std::string, std::string> m1 {{"key", "val"}};

            // create and print a JSON from the map
            json j = m1;

            // get the map out of JSON
            std::map<std::string, std::string> m2 = j;

            // make sure the roundtrip succeeds
            CHECK(m1 == m2);
        }
    }

    SECTION("issue #602 - BOM not skipped when using json:parse(iterator)")
    {
        std::string i = "\xef\xbb\xbf{\n   \"foo\": true\n}";
        CHECK_NOTHROW(json::parse(i.begin(), i.end()));
    }

    SECTION("issue #702 - conversion from valarray<double> to json fails to build")
    {
        SECTION("original example")
        {
            std::valarray<double> v;
            nlohmann::json j;
            j["test"] = v;
        }

        SECTION("full example")
        {
            std::valarray<double> v = {1.2, 2.3, 3.4, 4.5};
            json j = v;
            std::valarray<double> vj = j;

            CHECK(j == json(vj));
            CHECK(v.size() == vj.size());
            for (size_t i = 0; i < v.size(); ++i)
            {
                CHECK(v[i] == vj[i]);
                CHECK(v[i] == j[i]);
            }

            CHECK_THROWS_AS(json().get<std::valarray<double>>(), json::type_error&);
            CHECK_THROWS_WITH(json().get<std::valarray<double>>(),
                              "[json.exception.type_error.302] type must be array, but is null");
        }
    }

    SECTION("issue #367 - Behavior of operator>> should more closely resemble that of built-in overloads.")
    {
        SECTION("example 1")
        {
            std::istringstream i1_2_3( "{\"first\": \"one\" }{\"second\": \"two\"}3" );
            json j1, j2, j3;
            i1_2_3 >> j1;
            i1_2_3 >> j2;
            i1_2_3 >> j3;

            std::map<std::string, std::string> m1 = j1;
            std::map<std::string, std::string> m2 = j2;
            int i3 = j3;

            CHECK( m1 == ( std::map<std::string, std::string> {{ "first",  "one" }} ));
            CHECK( m2 == ( std::map<std::string, std::string> {{ "second", "two" }} ));
            CHECK( i3 == 3 );
        }
    }

    SECTION("issue #714 - throw std::ios_base::failure exception when failbit set to true")
    {
        {
            std::ifstream is;
            is.exceptions(
                is.exceptions()
                | std::ios_base::failbit
                | std::ios_base::badbit
            ); // handle different exceptions as 'file not found', 'permission denied'

            is.open("test/data/regression/working_file.json");
            CHECK_NOTHROW(nlohmann::json::parse(is));
        }

        {
            std::ifstream is;
            is.exceptions(
                is.exceptions()
                | std::ios_base::failbit
                | std::ios_base::badbit
            ); // handle different exceptions as 'file not found', 'permission denied'

            is.open("test/data/json_nlohmann_tests/all_unicode.json.cbor",
                    std::ios_base::in | std::ios_base::binary);
            CHECK_NOTHROW(nlohmann::json::from_cbor(is));
        }
    }

    SECTION("issue #805 - copy constructor is used with std::initializer_list constructor.")
    {
        nocopy n;
        json j;
        j = {{"nocopy", n}};
        CHECK(j["nocopy"]["val"] == 0);
    }

    SECTION("issue #838 - incorrect parse error with binary data in keys")
    {
        uint8_t key1[] = { 103, 92, 117, 48, 48, 48, 55, 92, 114, 215, 126, 214, 95, 92, 34, 174, 40, 71, 38, 174, 40, 71, 38, 223, 134, 247, 127 };
        std::string key1_str(key1, key1 + sizeof(key1) / sizeof(key1[0]));
        json j = key1_str;
        CHECK_THROWS_AS(j.dump(), json::type_error);
        CHECK_THROWS_WITH(j.dump(), "[json.exception.type_error.316] invalid UTF-8 byte at index 10: 0x7E");
    }

    SECTION("issue #843 - converting to array not working")
    {
        json j;
        std::array<int, 4> ar = {{1, 1, 1, 1}};
        j = ar;
        ar = j;
    }
}
