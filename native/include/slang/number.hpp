#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace slang {
// Decimal coefficient arithmetic: no source number passes through a float or
// a fixed-width integer. Digits are canonical, unsigned, and most-significant first.
class Number {
public:
    static Number parse(std::string_view text, bool decimal = false);
    static Number integer(std::size_t value);
    std::string str() const;
    bool is_integer() const { return !decimal_; }
    bool whole() const { return scale_ == 0; }
    bool zero() const { return digits_ == "0"; }
    bool negative() const { return negative_; }
    Number as_decimal() const;
    Number as_integer() const;
    Number negate() const;
    Number rounded(std::size_t places) const;
    Number floored() const;
    Number ceiled() const;
    Number square_root(std::size_t places = 28) const;
    std::size_t size_value(std::size_t maximum) const;
    static int compare(const Number& a, const Number& b);
    static Number add(const Number& a, const Number& b);
    static Number subtract(const Number& a, const Number& b);
    static Number multiply(const Number& a, const Number& b);
    static Number divide(const Number& a, const Number& b, std::size_t places = 28);
    static Number remainder(const Number& a, const Number& b);
private:
    std::string digits_ = "0";
    std::size_t scale_ = 0;
    bool negative_ = false;
    bool decimal_ = false;
    void normalize();
};
} // namespace slang
