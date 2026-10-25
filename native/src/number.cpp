#include "slang/number.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace slang {
namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of('0');
    return first == std::string::npos ? "0" : value.substr(first);
}
int cmp(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    return a == b ? 0 : (a < b ? -1 : 1);
}
std::string plus(const std::string& a, const std::string& b) {
    std::string out;
    std::size_t i = a.size(), j = b.size();
    int carry = 0;
    while (i || j || carry) {
        int digit = carry;
        if (i) digit += a[--i] - '0';
        if (j) digit += b[--j] - '0';
        out += static_cast<char>('0' + digit % 10);
        carry = digit / 10;
    }
    std::reverse(out.begin(), out.end());
    return out;
}
// Requires a >= b.
std::string minus(const std::string& a, const std::string& b) {
    std::string out;
    std::size_t i = a.size(), j = b.size();
    int borrow = 0;
    while (i) {
        int digit = a[--i] - '0' - borrow;
        if (j) digit -= b[--j] - '0';
        borrow = digit < 0 ? 1 : 0;
        out += static_cast<char>('0' + digit + borrow * 10);
    }
    std::reverse(out.begin(), out.end());
    return trim(out);
}
std::string times(const std::string& a, const std::string& b) {
    if (a == "0" || b == "0") return "0";
    std::vector<int> digits(a.size() + b.size());
    for (std::size_t i = a.size(); i-- > 0;) {
        for (std::size_t j = b.size(); j-- > 0;) {
            const int product = (a[i] - '0') * (b[j] - '0') + digits[i + j + 1];
            digits[i + j + 1] = product % 10;
            digits[i + j] += product / 10;
        }
    }
    std::string out;
    for (int digit : digits) out += static_cast<char>('0' + digit);
    return trim(out);
}
std::pair<std::string, std::string> divmod(const std::string& a, const std::string& b) {
    if (b == "0") throw std::runtime_error("Cannot divide by zero");
    std::string quotient, rest = "0";
    for (char digit : a) {
        rest = trim(rest + digit);
        int q = 0;
        while (cmp(rest, b) >= 0) { rest = minus(rest, b); ++q; }
        quotient += static_cast<char>('0' + q);
    }
    return {trim(quotient), rest};
}
std::string shifted(const std::string& digits, std::size_t count) {
    return digits == "0" ? digits : digits + std::string(count, '0');
}
std::string rounded_quotient(const std::string& numerator, const std::string& denominator) {
    auto [quotient, rest] = divmod(numerator, denominator);
    const int half = cmp(plus(rest, rest), denominator);
    if (half > 0 || (half == 0 && (quotient.back() - '0') % 2 != 0)) quotient = plus(quotient, "1");
    return quotient;
}
} // namespace

void Number::normalize() {
    digits_ = trim(digits_);
    while (scale_ && digits_.back() == '0' && digits_.size() > 1) { digits_.pop_back(); --scale_; }
    if (digits_ == "0") { scale_ = 0; negative_ = false; }
}
Number Number::parse(std::string_view text, bool decimal) {
    Number result;
    result.decimal_ = decimal;
    std::size_t pos = 0;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) { result.negative_ = text[0] == '-'; ++pos; }
    bool dot = false, digit = false, fraction = false;
    result.digits_.clear();
    for (; pos < text.size(); ++pos) {
        char c = text[pos];
        if (c == '.' && decimal && !dot && digit) { dot = true; continue; }
        if (c < '0' || c > '9') throw std::runtime_error("Expected numeric text using digits and an optional decimal point");
        digit = true;
        result.digits_ += c;
        if (dot) { ++result.scale_; fraction = true; }
    }
    if (!digit || (dot && !fraction)) throw std::runtime_error("Expected numeric text using digits and an optional decimal point");
    result.normalize();
    return result;
}
Number Number::integer(std::size_t value) { return parse(std::to_string(value)); }
std::string Number::str() const {
    auto text = digits_;
    if (scale_) {
        if (text.size() <= scale_) text.insert(0, scale_ + 1 - text.size(), '0');
        text.insert(text.size() - scale_, 1, '.');
    }
    if (negative_) text.insert(0, 1, '-');
    return text;
}
Number Number::as_decimal() const { auto result = *this; result.decimal_ = true; return result; }
Number Number::as_integer() const {
    if (!whole()) throw std::runtime_error("Cannot convert a fractional decimal to int; round explicitly first");
    auto result = *this; result.decimal_ = false; return result;
}
Number Number::negate() const { auto result = *this; if (!zero()) result.negative_ = !negative_; return result; }
Number Number::rounded(std::size_t places) const {
    auto result = as_decimal();
    if (scale_ > places) {
        result.digits_ = rounded_quotient(digits_, "1" + std::string(scale_ - places, '0'));
        result.scale_ = places;
        result.normalize();
    }
    return result;
}
Number Number::floored() const {
    if (whole()) return as_integer();
    auto result = parse(digits_.size() <= scale_ ? "0" : digits_.substr(0, digits_.size() - scale_));
    return negative_ ? subtract(result.negate(), integer(1)) : result;
}
Number Number::ceiled() const { return negate().floored().negate(); }
Number Number::square_root(std::size_t places) const {
    if (negative_) throw std::runtime_error("numbers.sqrt needs a non-negative number");
    // Integer square root of floor(value * 10^(2*places)). No binary float.
    auto scaled = scale_ <= 2 * places ? shifted(digits_, 2 * places - scale_) :
        (digits_.size() <= scale_ - 2 * places ? "0" : digits_.substr(0, digits_.size() - (scale_ - 2 * places)));
    std::string root = "0";
    if (scaled != "0") {
        root = "1" + std::string((scaled.size() + 1) / 2, '0');
        for (;;) {
            auto next = divmod(plus(root, divmod(scaled, root).first), "2").first;
            if (cmp(next, root) >= 0) break;
            root = std::move(next);
        }
    }
    // Compare the original exact value with the midpoint (root + 0.5)^2.
    const auto midpoint = plus(times(root, "2"), "1");
    const int side = cmp(shifted(times(digits_, "4"), 2 * places), shifted(times(midpoint, midpoint), scale_));
    if (side > 0 || (side == 0 && (root.back() - '0') % 2 != 0)) root = plus(root, "1");
    Number result; result.digits_ = std::move(root); result.scale_ = places; result.decimal_ = true; result.normalize();
    return result;
}
std::size_t Number::size_value(std::size_t maximum) const {
    if (!is_integer() || negative_ || compare(*this, integer(maximum)) > 0) throw std::runtime_error("Expected a non-negative int within the supported range");
    std::size_t result = 0;
    for (char digit : digits_) result = result * 10 + static_cast<std::size_t>(digit - '0');
    return result;
}
int Number::compare(const Number& a, const Number& b) {
    if (a.negative_ != b.negative_) return a.negative_ ? -1 : 1;
    const auto scale = std::max(a.scale_, b.scale_);
    const int order = cmp(shifted(a.digits_, scale - a.scale_), shifted(b.digits_, scale - b.scale_));
    return a.negative_ ? -order : order;
}
Number Number::add(const Number& a, const Number& b) {
    Number result;
    result.scale_ = std::max(a.scale_, b.scale_);
    result.decimal_ = a.decimal_ || b.decimal_;
    auto left = shifted(a.digits_, result.scale_ - a.scale_);
    auto right = shifted(b.digits_, result.scale_ - b.scale_);
    if (a.negative_ == b.negative_) { result.digits_ = plus(left, right); result.negative_ = a.negative_; }
    else if (cmp(left, right) >= 0) { result.digits_ = minus(left, right); result.negative_ = a.negative_; }
    else { result.digits_ = minus(right, left); result.negative_ = b.negative_; }
    result.normalize();
    return result;
}
Number Number::subtract(const Number& a, const Number& b) { return add(a, b.negate()); }
Number Number::multiply(const Number& a, const Number& b) {
    Number result;
    result.digits_ = times(a.digits_, b.digits_);
    result.scale_ = a.scale_ + b.scale_;
    result.negative_ = a.negative_ != b.negative_;
    result.decimal_ = a.decimal_ || b.decimal_;
    result.normalize();
    return result;
}
Number Number::divide(const Number& a, const Number& b, std::size_t places) {
    Number result;
    result.decimal_ = true;
    result.scale_ = places;
    result.negative_ = a.negative_ != b.negative_;
    result.digits_ = rounded_quotient(shifted(a.digits_, b.scale_ + places), shifted(b.digits_, a.scale_));
    result.normalize();
    return result;
}
Number Number::remainder(const Number& a, const Number& b) {
    if (!a.is_integer() || !b.is_integer()) throw std::runtime_error("Remainder '%' requires two int values");
    Number result;
    result.digits_ = divmod(a.digits_, b.digits_).second;
    result.negative_ = a.negative_;
    result.normalize();
    return result;
}
} // namespace slang
