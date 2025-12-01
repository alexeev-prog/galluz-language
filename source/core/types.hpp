#pragma once

#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include "../parser/GalluzGrammar.h"

namespace galluz::core {

    struct VariableInfo {
        llvm::Value* value;
        llvm::Type* type;
        bool is_global;
        std::string name;
    };

    struct Scope {
        std::unordered_map<std::string, VariableInfo> variables;
        Scope* parent;

        Scope(Scope* p = nullptr)
            : parent(p) {}

        auto find_variable(const std::string& name) -> VariableInfo* {
            auto it = variables.find(name);
            if (it != variables.end()) {
                return &it->second;
            }
            if (parent) {
                return parent->find_variable(name);
            }
            return nullptr;
        }
    };

    struct CompilationContext {
        llvm::LLVMContext& m_CTX;
        llvm::Module& m_MODULE;
        llvm::IRBuilder<>& m_BUILDER;
        llvm::Function* m_CURRENT_FUNCTION;
        std::unordered_map<std::string, llvm::GlobalVariable*> globals;
        std::stack<std::unique_ptr<Scope>> scopes;
        Scope* current_scope;

        CompilationContext(llvm::LLVMContext& ctx,
                           llvm::Module& module,
                           llvm::IRBuilder<>& builder,
                           llvm::Function* current_function)
            : m_CTX(ctx)
            , m_MODULE(module)
            , m_BUILDER(builder)
            , m_CURRENT_FUNCTION(current_function)
            , current_scope(nullptr) {
            push_scope();
        }

        auto push_scope() -> void {
            auto new_scope = std::make_unique<Scope>(current_scope);
            current_scope = new_scope.get();
            scopes.push(std::move(new_scope));
        }

        auto pop_scope() -> void {
            if (!scopes.empty()) {
                scopes.pop();
                current_scope = scopes.empty() ? nullptr : scopes.top().get();
            }
        }

        auto get_current_scope() -> Scope* { return current_scope; }

        auto find_variable(const std::string& name) -> VariableInfo* {
            if (current_scope) {
                return current_scope->find_variable(name);
            }
            return nullptr;
        }

        auto add_variable(const std::string& name,
                          llvm::Value* value,
                          llvm::Type* type,
                          bool is_global = false) -> void {
            if (current_scope) {
                current_scope->variables[name] = {value, type, is_global, name};
            }
        }

        auto update_variable(const std::string& name, llvm::Value* new_value) -> bool {
            auto* var_info = find_variable(name);
            if (var_info) {
                var_info->value = new_value;
                return true;
            }
            return false;
        }
    };

    class ICodeGenerator {
      public:
        virtual ~ICodeGenerator() = default;

        virtual auto can_handle(const Exp& ast_node) const -> bool = 0;
        virtual auto generate(const Exp& ast_node, CompilationContext& context) -> llvm::Value* = 0;
        virtual auto get_priority() const -> int = 0;
    };

}    // namespace galluz::core
