#pragma once

#include "../generators/arithmetic_generator.hpp"
#include "../generators/comparison_generator.hpp"
#include "../generators/fractional_generator.hpp"
#include "../generators/function_call_generator.hpp"
#include "../generators/function_generator.hpp"
#include "../generators/list_generator.hpp"
#include "../generators/number_generator.hpp"
#include "../generators/print_generator.hpp"
#include "../generators/scope_generator.hpp"
#include "../generators/set_generator.hpp"
#include "../generators/string_generator.hpp"
#include "../generators/symbol_generator.hpp"
#include "../generators/variable_generator.hpp"
#include "generator_manager.hpp"

namespace galluz::core {

    class GeneratorFactory {
      public:
        static auto register_default_generators(GeneratorManager& manager) -> void {
            manager.register_generator(std::make_unique<generators::NumberGenerator>());
            manager.register_generator(std::make_unique<generators::FractionalGenerator>());
            manager.register_generator(std::make_unique<generators::StringGenerator>());
            manager.register_generator(std::make_unique<generators::SymbolGenerator>());
            manager.register_generator(std::make_unique<generators::VariableGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::SetGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::ScopeGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::ArithmeticGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::ComparisonGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::PrintGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::ListGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::FunctionGenerator>(&manager));
            manager.register_generator(std::make_unique<generators::FunctionCallGenerator>(&manager));
        }
    };

}    // namespace galluz::core
