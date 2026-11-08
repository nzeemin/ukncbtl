/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

// Preserve the existing Microsoft test cases while GoogleTest runs them.
// Only the assertions and attributes used by this repository are provided.
#include <cwchar>
#include <exception>
#include <string>
#include <type_traits>
#include <source_location>
#include <utility>
#include <gtest/gtest.h>

namespace Microsoft::VisualStudio::CppUnitTestFramework {
class AssertionFailure : public std::exception
{
public:
    AssertionFailure(std::string message, std::source_location location)
        : message_(std::move(message)), location_(location) {}
    const char* what() const noexcept override { return message_.c_str(); }
    const std::source_location& location() const { return location_; }
private:
    std::string message_;
    std::source_location location_;
};

struct Assert
{
    [[noreturn]] static void Fail(const wchar_t* message = nullptr,
        std::source_location location = std::source_location::current())
    {
        throw AssertionFailure(message ? testing::PrintToString(message) : "assertion failed", location);
    }
    static void IsTrue(bool value, const wchar_t* message = nullptr,
        std::source_location location = std::source_location::current())
    { if (!value) Fail(message, location); }
    static void IsFalse(bool value, const wchar_t* message = nullptr,
        std::source_location location = std::source_location::current())
    { if (value) Fail(message, location); }
    template<class T> static void IsNotNull(T* value, const wchar_t* message = nullptr,
        std::source_location location = std::source_location::current())
    { if (!value) Fail(message, location); }
    template<class A, class B> static void AreEqual(const A& expected, const B& actual,
        const wchar_t* message = nullptr, std::source_location location = std::source_location::current())
    {
        bool equal;
        if constexpr (std::is_convertible_v<A, const wchar_t*> && std::is_convertible_v<B, const wchar_t*>)
        {
            const wchar_t* lhs = expected;
            const wchar_t* rhs = actual;
            equal = lhs && rhs && std::wcscmp(lhs, rhs) == 0;
        }
        else
            equal = expected == actual;
        if (!equal)
            throw AssertionFailure("Expected: " + testing::PrintToString(expected) +
                "\nActual: " + testing::PrintToString(actual) + "\n" +
                (message ? testing::PrintToString(message) : ""), location);
    }
};
}

#define TEST_CLASS(name) class name
#define TEST_METHOD(name) void name()
#define BEGIN_TEST_METHOD_ATTRIBUTE(name)
#define TEST_IGNORE()
#define END_TEST_METHOD_ATTRIBUTE()
#define TEST_MODULE_INITIALIZE(name) void name()
#define TEST_MODULE_CLEANUP(name) void name()
