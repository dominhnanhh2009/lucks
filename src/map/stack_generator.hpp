#pragma once

#include "core/model.hpp"

namespace lucks {

class StackMapGenerator {
public:
    [[nodiscard]] GameState generate(const GameConfig& config) const;
};

}  // namespace lucks
