#pragma once

#include <string>
#include <unordered_map>

#include <llvm/IR/Type.h>

#include "parser/GalluzGrammar.h"

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
        explicit TypeSystem(llvm::LLVMContext& ctx);

        auto register_type(const std::string& name, TypeKind kind, llvm::Type* type) -> void;
        auto get_type(const std::string& name) -> TypeInfo*;
        auto get_llvm_type(const std::string& name) -> llvm::Type*;
        auto type_from_string(const std::string& type_str) -> TypeInfo*;
        auto parse_type_spec(const Exp& type_exp) -> TypeInfo*;
    };
}    // namespace galluz::core
