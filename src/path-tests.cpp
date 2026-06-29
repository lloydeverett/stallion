#include <iostream>
#include <string>
#include <vector>

#include "path.hpp"
#include "tests.hpp"

static std::vector<std::string_view> parts_as_vector(const Path& p) {
    return {p.parts().begin(), p.parts().end()};
}

static void test_basic_split() {
    std::cout << "[TEST] Basic split..." << std::endl;
    Path p("foo/bar/baz", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "bar");
    assert(parts[2] == "baz");
    assert(p.str() == "foo/bar/baz");
    assert(p.sv() == "foo/bar/baz");
    assert(p.delimiter() == '/');
    std::cout << "  -> Passed!" << std::endl;
}

static void test_single_segment() {
    std::cout << "[TEST] Single segment (no delimiter)..." << std::endl;
    Path p("foo", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 1);
    assert(parts[0] == "foo");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_empty_string() {
    std::cout << "[TEST] Empty string..." << std::endl;
    Path p("", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 1);
    assert(parts[0] == "");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_leading_delimiter() {
    std::cout << "[TEST] Leading delimiter..." << std::endl;
    Path p("/foo/bar", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 3);
    assert(parts[0] == "");
    assert(parts[1] == "foo");
    assert(parts[2] == "bar");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_trailing_delimiter() {
    std::cout << "[TEST] Trailing delimiter..." << std::endl;
    Path p("foo/bar/", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "bar");
    assert(parts[2] == "");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_delimiter_only() {
    std::cout << "[TEST] Delimiter-only string..." << std::endl;
    Path p("/", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 2);
    assert(parts[0] == "");
    assert(parts[1] == "");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_consecutive_delimiters() {
    std::cout << "[TEST] Consecutive delimiters..." << std::endl;
    Path p("foo//bar", '/');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "");
    assert(parts[2] == "bar");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_non_slash_delimiter() {
    std::cout << "[TEST] Non-slash delimiter..." << std::endl;
    Path p("com.example.app", '.');
    auto parts = parts_as_vector(p);
    assert(parts.size() == 3);
    assert(parts[0] == "com");
    assert(parts[1] == "example");
    assert(parts[2] == "app");
    assert(p.delimiter() == '.');
    std::cout << "  -> Passed!" << std::endl;
}

static void test_copy_constructor() {
    std::cout << "[TEST] Copy constructor (views valid after copy)..." << std::endl;
    Path original("foo/bar/baz", '/');
    Path copy(original);
    auto parts = parts_as_vector(copy);
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "bar");
    assert(parts[2] == "baz");
    assert(copy.str() == "foo/bar/baz");
    // Verify the copy is independent
    assert(copy.str().data() != original.str().data());
    std::cout << "  -> Passed!" << std::endl;
}

static void test_move_constructor() {
    std::cout << "[TEST] Move constructor (views valid after move)..." << std::endl;
    Path original("foo/bar/baz", '/');
    Path moved(std::move(original));
    auto parts = parts_as_vector(moved);
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "bar");
    assert(parts[2] == "baz");
    assert(moved.str() == "foo/bar/baz");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_copy_assignment() {
    std::cout << "[TEST] Copy assignment (views valid after copy-assign)..." << std::endl;
    Path a("one/two", '/');
    Path b("x/y/z", '/');
    b = a;
    auto parts = parts_as_vector(b);
    assert(parts.size() == 2);
    assert(parts[0] == "one");
    assert(parts[1] == "two");
    assert(b.str() == "one/two");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_move_assignment() {
    std::cout << "[TEST] Move assignment (views valid after move-assign)..." << std::endl;
    Path a("one/two", '/');
    Path b("x/y/z", '/');
    b = std::move(a);
    auto parts = parts_as_vector(b);
    assert(parts.size() == 2);
    assert(parts[0] == "one");
    assert(parts[1] == "two");
    assert(b.str() == "one/two");
    std::cout << "  -> Passed!" << std::endl;
}

static void test_views_point_into_storage() {
    std::cout << "[TEST] string_views point into internal storage..." << std::endl;
    Path p("foo/bar", '/');
    const char* storage_begin = p.str().data();
    assert(p.parts()[0].data() == storage_begin);
    assert(p.parts()[1].data() == storage_begin + 4);
    std::cout << "  -> Passed!" << std::endl;
}

struct path_tests {
    path_tests() { g_tests.push_back(path_tests::test); }
    static void test() {
        test_basic_split();
        test_single_segment();
        test_empty_string();
        test_leading_delimiter();
        test_trailing_delimiter();
        test_delimiter_only();
        test_consecutive_delimiters();
        test_non_slash_delimiter();
        test_copy_constructor();
        test_move_constructor();
        test_copy_assignment();
        test_move_assignment();
        test_views_point_into_storage();
        std::cout << "\nAll path tests passed!\n";
    }
} g_path_tests;
