#pragma once

#include "core/model.hpp"

#include <filesystem>

namespace lucks {

void write_result_json(const GameState& state, const std::filesystem::path& path);

}  // namespace lucks
