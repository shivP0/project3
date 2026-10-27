// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

static int precedence(CalcToken::Kind kind) {
    using Kind = CalcToken::Kind;
    if (kind == Kind::Plus || kind == Kind::Minus) return 1;
    if (kind == Kind::Times || kind == Kind::Divide) return 2;
    return 0;
}

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out,
                   std::string* error) {
    using Kind = CalcToken::Kind;
    std::vector<double> values;
    std::vector<Kind> ops;
    bool expecting_value = true;

    auto invalid = [&]() {
        *error = "invalid expression";
        return false;
    };
    auto apply = [&]() {
        if (ops.empty() || values.size() < 2) return invalid();
        const Kind op = ops.back();
        ops.pop_back();
        const double right = values.back();
        values.pop_back();
        const double left = values.back();
        values.pop_back();
        double result;
        switch (op) {
        case Kind::Plus: result = left + right; break;
        case Kind::Minus: result = left - right; break;
        case Kind::Times: result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default: return invalid();
        }
        values.push_back(result);
        return true;
    };

    for (const auto& token : tokens) {
        if (token.kind == Kind::Number) {
            if (!expecting_value) return invalid();
            values.push_back(token.value);
            expecting_value = false;
        } else if (token.kind == Kind::LParen) {
            if (!expecting_value) return invalid();
            ops.push_back(token.kind);
        } else if (token.kind == Kind::RParen) {
            if (expecting_value) return invalid();
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply()) return false;
            }
            if (ops.empty()) return invalid();
            ops.pop_back();
        } else {
            if (expecting_value || precedence(token.kind) == 0) return invalid();
            while (!ops.empty() && precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply()) return false;
            }
            ops.push_back(token.kind);
            expecting_value = true;
        }
    }
    if (expecting_value) return invalid();
    while (!ops.empty()) {
        if (!apply()) return false;
    }
    if (values.size() != 1) return invalid();
    *out = values.back();
    return true;
}
