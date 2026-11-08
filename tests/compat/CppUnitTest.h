/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#pragma once

// Minimal adapter for running the existing Microsoft C++ unit tests under CTest.
// Only the assertions and attributes used by this repository are provided.
#include <cwchar>
#include <exception>
#include <string>
#include <type_traits>

namespace Microsoft::VisualStudio::CppUnitTestFramework {
class AssertionFailure : public std::exception
{
public:
    explicit AssertionFailure(const wchar_t* message) : message_(message ? message : L"assertion failed") {}
    const char* what() const noexcept override { return "assertion failed"; }
    const std::wstring& message() const { return message_; }
private:
    std::wstring message_;
};

struct Assert
{
    [[noreturn]] static void Fail(const wchar_t* message = nullptr) { throw AssertionFailure(message); }
    static void IsTrue(bool value, const wchar_t* message = nullptr) { if (!value) Fail(message); }
    static void IsFalse(bool value, const wchar_t* message = nullptr) { if (value) Fail(message); }
    template<class T> static void IsNotNull(T* value, const wchar_t* message = nullptr) { if (!value) Fail(message); }
    static void AreEqual(const wchar_t* expected, const wchar_t* actual, const wchar_t* message = nullptr)
    {
        if (!expected || !actual || std::wcscmp(expected, actual) != 0) Fail(message);
    }
    template<class A, class B> static void AreEqual(const A& expected, const B& actual, const wchar_t* message = nullptr)
    {
        if constexpr (std::is_convertible_v<A, const wchar_t*> && std::is_convertible_v<B, const wchar_t*>)
            AreEqual(static_cast<const wchar_t*>(expected), static_cast<const wchar_t*>(actual), message);
        else if (!(expected == actual))
            Fail(message);
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
