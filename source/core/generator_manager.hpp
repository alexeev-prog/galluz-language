#pragma once

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../parser/utils.hpp"
#include "types.hpp"

namespace galluz::core {

    class GeneratorManager {
        std::vector<std::unique_ptr<ICodeGenerator>> m_GENERATORS;

      public:
        GeneratorManager() = default;

        GeneratorManager(const GeneratorManager&) = delete;
        auto operator=(const GeneratorManager&) -> GeneratorManager& = delete;

        GeneratorManager(GeneratorManager&&) = default;
        auto operator=(GeneratorManager&&) -> GeneratorManager& = default;

        auto register_generator(std::unique_ptr<ICodeGenerator> generator) -> void {
            if (!generator) {
                throw std::invalid_argument("Cannot register null generator");
            }

            m_GENERATORS.push_back(std::move(generator));

            std::sort(m_GENERATORS.begin(),
                      m_GENERATORS.end(),
                      [](const std::unique_ptr<ICodeGenerator>& a, const std::unique_ptr<ICodeGenerator>& b)
                      { return a->get_priority() > b->get_priority(); });
        }

        auto find_generator(const Exp& ast_node) -> ICodeGenerator* {
            add_expression_to_traceback_stack(ast_node);

            for (const auto& generator : m_GENERATORS) {
                if (generator->can_handle(ast_node)) {
                    return generator.get();
                }
            }
            return nullptr;
        }

        auto generate_code(const Exp& ast_node, CompilationContext& context) -> llvm::Value* {
            auto* generator = find_generator(ast_node);

            if (generator == nullptr) {
                throw std::runtime_error("No generator found for AST node of type "
                                         + std::to_string(static_cast<int>(ast_node.type)));
            }

            return generator->generate(ast_node, context);
        }

        auto has_generator_for(const Exp& ast_node) const -> bool {
            for (const auto& generator : m_GENERATORS) {
                if (generator->can_handle(ast_node)) {
                    return true;
                }
            }
            return false;
        }

        auto get_generator_count() const -> size_t { return m_GENERATORS.size(); }

        auto clear_generators() -> void { m_GENERATORS.clear(); }

        auto has_generators() const -> bool { return !m_GENERATORS.empty(); }

        auto get_generator_priorities() const -> std::vector<int> {
            std::vector<int> priorities;
            priorities.reserve(m_GENERATORS.size());

            for (const auto& generator : m_GENERATORS) {
                priorities.push_back(generator->get_priority());
            }

            return priorities;
        }

        auto get_generator_info() const -> std::vector<std::string> {
            std::vector<std::string> info;
            info.reserve(m_GENERATORS.size());

            for (size_t i = 0; i < m_GENERATORS.size(); ++i) {
                info.push_back("Generator[" + std::to_string(i)
                               + "] priority: " + std::to_string(m_GENERATORS[i]->get_priority()));
            }

            return info;
        }
    };

}    // namespace galluz::core
