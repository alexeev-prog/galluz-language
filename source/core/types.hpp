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

    enum class TypeKind
    {
        INT,
        DOUBLE,
        STRING,
        BOOL,
        VOID,
        UNKNOWN
    };

    struct TypeInfo {
        TypeKind kind;
        llvm::Type* llvm_type;
        std::string name;
        bool is_reference = false;
    };

    class TypeSystem {
      private:
        std::unordered_map<std::string, TypeInfo> type_registry;
        llvm::LLVMContext& context;

      public:
        explicit TypeSystem(llvm::LLVMContext& ctx)
            : context(ctx) {}

        auto register_type(const std::string& name, TypeKind kind, llvm::Type* type) -> void {
            type_registry[name] = {kind, type, name, false};
        }

        auto get_type(const std::string& name) -> TypeInfo* {
            auto it = type_registry.find(name);
            if (it != type_registry.end()) {
                return &it->second;
            }
            return nullptr;
        }

        auto get_llvm_type(const std::string& name) -> llvm::Type* {
            auto* info = get_type(name);
            return info ? info->llvm_type : nullptr;
        }

        auto type_from_string(const std::string& type_str) -> TypeInfo* {
            if (type_str.empty() || type_str[0] != '!') {
                return nullptr;
            }
            return get_type(type_str.substr(1));
        }

        auto parse_type_spec(const Exp& type_exp) -> TypeInfo* {
            if (type_exp.type != ExpType::SYMBOL || type_exp.string.empty() || type_exp.string[0] != '!') {
                return nullptr;
            }
            return get_type(type_exp.string.substr(1));
        }
    };

    struct VariableInfo {
        llvm::Value* value;
        llvm::Type* type;
        TypeInfo* type_info;
        bool is_global;
        std::string name;
    };

    struct FunctionInfo {
        llvm::Function* function;
        TypeInfo* return_type;
        std::vector<VariableInfo> parameters;
        bool is_external;
    };

    struct Scope {
        std::unordered_map<std::string, VariableInfo> variables;
        std::unordered_map<std::string, FunctionInfo> functions;
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

        auto find_function(const std::string& name) -> FunctionInfo* {
            auto it = functions.find(name);
            if (it != functions.end()) {
                return &it->second;
            }
            if (parent) {
                return parent->find_function(name);
            }
            return nullptr;
        }
    };

    struct CompilationContext {
        llvm::LLVMContext& m_CTX;
        llvm::Module& m_MODULE;
        llvm::IRBuilder<>& m_BUILDER;
        llvm::Function* m_CURRENT_FUNCTION;
        TypeSystem* type_system;
        std::unordered_map<std::string, llvm::GlobalVariable*> globals;
        std::stack<std::unique_ptr<Scope>> scopes;
        Scope* current_scope;

        CompilationContext(llvm::LLVMContext& ctx,
                           llvm::Module& module,
                           llvm::IRBuilder<>& builder,
                           llvm::Function* current_function,
                           TypeSystem* ts)
            : m_CTX(ctx)
            , m_MODULE(module)
            , m_BUILDER(builder)
            , m_CURRENT_FUNCTION(current_function)
            , type_system(ts)
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

        auto find_function(const std::string& name) -> FunctionInfo* {
            Scope* scope = current_scope;
            while (scope) {
                auto it = scope->functions.find(name);
                if (it != scope->functions.end()) {
                    return &it->second;
                }
                scope = scope->parent;
            }
            return nullptr;
        }

        auto add_variable(const std::string& name,
                          llvm::Value* value,
                          llvm::Type* type,
                          TypeInfo* type_info,
                          bool is_global = false) -> void {
            if (current_scope) {
                current_scope->variables[name] = {value, type, type_info, is_global, name};
            }
        }

        auto add_function(const std::string& name,
                          llvm::Function* func,
                          TypeInfo* return_type,
                          const std::vector<VariableInfo>& params,
                          bool is_external = false) -> void {
            if (current_scope) {
                current_scope->functions[name] = {func, return_type, params, is_external};
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
