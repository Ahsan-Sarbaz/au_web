#include <cassert>
#include <iostream>
#include <vector>

#include "path.hpp"

// Simple test helper to compare vectors of string_views
bool compare_segments(const std::vector<std::string>& actual, const std::vector<std::string>& expected)
{
    if (actual.size() != expected.size()) return false;
    for (size_t i = 0; i < actual.size(); ++i)
    {
        if (actual[i] != expected[i]) return false;
    }
    return true;
}

void run_tests()
{
    std::cout << "Running Path class tests...\n";

    // Normal Cases
    {
        std::cout << "Test 1: Root path '/'\n";
        Path p = Path::from_string("/");
        p.parse();
        assert(compare_segments(p.get_segments(), {})); // Empty segments for root
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 2: Simple path '/index.html'\n";
        Path p = Path::from_string("/index.html");
        p.parse();
        assert(compare_segments(p.get_segments(), {"index.html"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 3: Multi-segment path '/users/123/profile'\n";
        Path p = Path::from_string("/users/123/profile");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123", "profile"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 4: Path with query '/search?q=example'\n";
        Path p = Path::from_string("/search?q=example");
        p.parse();
        assert(compare_segments(p.get_segments(), {"search"}));
        assert(p.get_query() == "q=example");
    }

    // Edge Cases
    {
        std::cout << "Test 6: Empty string ''\n";
        Path p = Path::from_string("");
        p.parse();
        assert(compare_segments(p.get_segments(), {}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 7: Multiple slashes '/users//123'\n";
        Path p = Path::from_string("/users//123");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 8: Trailing slash '/users/123/'\n";
        Path p = Path::from_string("/users/123/");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 9: Only query '/?q=example'\n";
        Path p = Path::from_string("/?q=example");
        p.parse();
        assert(compare_segments(p.get_segments(), {}));
        assert(p.get_query() == "q=example");
    }

    {
        std::cout << "Test 11: Multiple delimiters '/path?a=b&c=d'\n";
        Path p = Path::from_string("/path?a=b&c=d#frag");
        p.parse();
        assert(compare_segments(p.get_segments(), {"path"}));
        assert(p.get_query() == "a=b&c=d");
    }

    {
        std::cout << "Test 12: No leading slash 'users/123'\n";
        Path p = Path::from_string("users/123");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 13: Multiple slashes '/users/////123'\n";
        Path p = Path::from_string("/users/////123");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 14: Multiple starting slashes '///users/123'\n";
        Path p = Path::from_string("///users/123");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", "123"}));
        assert(p.get_query().empty());
    }

    std::cout << "All tests passed!\n";
}

void run_percent_encoding_tests()
{
    std::cout << "Running Path class tests with percent-encoding...\n";

    // Normal Cases
    {
        std::cout << "Test 1: Root path '/'\n";
        Path p = Path::from_string("/");
        p.parse();
        assert(compare_segments(p.get_segments(), {}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 2: Simple path '/index.html'\n";
        Path p = Path::from_string("/index.html");
        p.parse();
        assert(compare_segments(p.get_segments(), {"index.html"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 3: Path with query '/search?q=example'\n";
        Path p = Path::from_string("/search?q=example");
        p.parse();
        assert(compare_segments(p.get_segments(), {"search"}));
        assert(p.get_query() == "q=example");
    }

    // Percent-Encoding Cases
    {
        std::cout << "Test 4: Percent-encoded segment '/path%20with%20spaces'\n";
        Path p = Path::from_string("/path%20with%20spaces");
        p.parse();
        assert(compare_segments(p.get_segments(), {"path with spaces"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 5: Percent-encoded query '/search?q=hello%20world'\n";
        Path p = Path::from_string("/search?q=hello%20world");
        p.parse();
        assert(compare_segments(p.get_segments(), {"search"}));
        assert(p.get_query() == "q=hello world");
    }

    {
        std::cout << "Test 7: Mixed encoding '/users%2F123?name%3Djohn'\n";
        Path p = Path::from_string("/users%2F123?name%3Djohn");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users/123"}));
        assert(p.get_query() == "name=john");
    }

    // Edge Cases
    {
        std::cout << "Test 8: Partial encoding '/path%2'\n";
        Path p = Path::from_string("/path%2");
        p.parse();
        assert(compare_segments(p.get_segments(), {"path%2"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 9: Encoded delimiter '/path%3Fquery'\n";
        Path p = Path::from_string("/path%3Fquery");
        p.parse();
        assert(compare_segments(p.get_segments(), {"path?query"}));
        assert(p.get_query().empty());
    }

    {
        std::cout << "Test 10: Multiple slashes with encoding '/users//%20test'\n";
        Path p = Path::from_string("/users//%20test");
        p.parse();
        assert(compare_segments(p.get_segments(), {"users", " test"}));
        assert(p.get_query().empty());
    }

    std::cout << "All percent-encoding tests passed!\n";
}

int main()
{
    run_tests();
    run_percent_encoding_tests();
    return 0;
}