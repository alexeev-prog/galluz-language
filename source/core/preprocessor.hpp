#pragma once

#include <cctype>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

namespace galluz::core {

    class Preprocessor {
      private:
        auto is_balanced_parentheses(const std::string& str) -> bool {
            std::stack<char> stack;

            for (char c : str) {
                if (c == '(') {
                    stack.push('(');
                } else if (c == ')') {
                    if (stack.empty() || stack.top() != '(') {
                        return false;
                    }
                    stack.pop();
                }
            }

            return stack.empty();
        }

        auto escape_string(const std::string& str) -> std::string {
            std::string result;
            bool escaped = false;

            for (size_t i = 0; i < str.size(); ++i) {
                char c = str[i];

                if (escaped) {
                    if (c == 'n') {
                        result += '\n';
                    } else if (c == 't') {
                        result += '\t';
                    } else if (c == 'r') {
                        result += '\r';
                    } else if (c == '0') {
                        result += '\0';
                    } else if (c == '"') {
                        result += '"';
                    } else if (c == '\\') {
                        result += '\\';
                    } else {
                        result += c;
                    }
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else {
                    result += c;
                }
            }

            return result;
        }

        auto process_line(const std::string& line) -> std::string {
            bool in_string = false;
            bool escaped = false;
            std::string result;

            for (size_t i = 0; i < line.size(); ++i) {
                char c = line[i];

                if (escaped) {
                    result += c;
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                    result += c;
                } else if (c == '"') {
                    in_string = !in_string;
                    result += c;
                } else {
                    result += c;
                }
            }

            return result;
        }

      public:
        auto preprocess(const std::string& code) -> std::string {
            std::stringstream ss(code);
            std::string line;
            std::string processed_code;
            std::string full_program;
            int line_num = 0;

            while (std::getline(ss, line)) {
                line_num++;

                std::string trimmed;
                bool in_string = false;
                bool escaped = false;

                size_t start = 0;
                while (start < line.size() && std::isspace(line[start]) && !in_string) {
                    start++;
                }

                for (size_t i = start; i < line.size(); ++i) {
                    char c = line[i];

                    if (escaped) {
                        escaped = false;
                        trimmed += c;
                    } else if (c == '\\') {
                        escaped = true;
                        trimmed += c;
                    } else if (c == '"') {
                        in_string = !in_string;
                        trimmed += c;
                    } else {
                        trimmed += c;
                    }
                }

                if (!trimmed.empty()) {
                    processed_code += trimmed + " ";
                }
            }

            if (!is_balanced_parentheses(processed_code)) {
                throw std::runtime_error("Unbalanced parentheses in program");
            }

            int depth = 0;
            std::vector<std::string> expressions;
            std::string current_expr;

            for (size_t i = 0; i < processed_code.size(); ++i) {
                char c = processed_code[i];

                if (c == '(') {
                    depth++;
                    current_expr += c;
                } else if (c == ')') {
                    depth--;
                    current_expr += c;

                    if (depth == 0) {
                        expressions.push_back(current_expr);
                        current_expr.clear();
                    }
                } else if (depth > 0) {
                    current_expr += c;
                } else if (!std::isspace(c)) {
                    throw std::runtime_error("Unexpected character outside expression: " + std::string(1, c));
                }
            }

            if (expressions.empty()) {
                throw std::runtime_error("No expressions found in program");
            }

            if (expressions.size() == 1) {
                full_program = expressions[0];
            } else {
                full_program = "(scope";
                for (const auto& expr : expressions) {
                    full_program += " " + expr;
                }
                full_program += ")";
            }

            return full_program;
        }

        auto postprocess_string(const std::string& str) -> std::string { return escape_string(str); }
    };

}    // namespace galluz::core
