#pragma once

#include <boost/algorithm/string.hpp>

#include "../core/types.hpp"
#include "../logger.hpp"
#include "GalluzGrammar.h"

/**
 * @brief Safe expression converting to string
 *
 * @param exp expression for converting
 * @return std::string converted string expression
 **/
auto safe_expr_to_string(const Exp& exp) -> std::string {
    switch (exp.type) {
        case ExpType::LIST: {
            if (exp.list.empty()) {
                return "[]";
            }

            std::string s = "(";
            for (const auto& e : exp.list) {
                s += safe_expr_to_string(e) + " ";
            }
            s.pop_back();
            s += ")";

            if (s.length() > 120) {
                return s.substr(0, 117) + "...";
            }
            return s;
        }
        case ExpType::SYMBOL:
            return exp.string;
        case ExpType::NUMBER:
            return std::to_string(exp.number);
        case ExpType::FRACTIONAL:
            return std::to_string(exp.fractional);
        case ExpType::STRING: {
            auto str1 = "\"" + exp.string + "\"";
            boost::replace_all(str1, "\n", "\\n");
            return str1;
        }
        default:
            return "<?>";
    }
}

auto llvm_type_to_string(llvm::Type* type) -> std::string {
    std::string type_str;
    llvm::raw_string_ostream rso(type_str);
    type->print(rso);
    return type_str;
}

/**
 * @brief Add expression to traceback expressions stack.
 *
 * @param exp expression for adding
 **/
void add_expression_to_traceback_stack(const Exp& exp) {
    std::string context = "expr";
    std::string const EXPR_STR = safe_expr_to_string(exp);

    if (EXPR_STR == "<?>") {
        return;
    }

    if (exp.type == ExpType::LIST || exp.type == ExpType::SYMBOL || exp.type == ExpType::NUMBER
        || exp.type == ExpType::FRACTIONAL || exp.type == ExpType::STRING)
    {
        if (exp.type == ExpType::LIST && !exp.list.empty()) {
            if (exp.list[0].type == ExpType::SYMBOL) {
                context = exp.list[0].string;
            } else {
                context = "list";
            }
        } else {
            switch (exp.type) {
                case ExpType::SYMBOL:
                    context = "symbol";
                    break;
                case ExpType::NUMBER:
                    context = "number";
                    break;
                case ExpType::FRACTIONAL:
                    context = "fractional";
                    break;
                case ExpType::STRING:
                    context = "string";
                    break;
                default:
                    context = "value";
            }
        }
    }

    PUSH_EXPR_STACK(context, EXPR_STR);
}
