#pragma once

#include "core/simulation.hpp"

#include <filesystem>

namespace lucks {

class Visualizer {
public:
    Visualizer(GameState initial_state, std::filesystem::path result_path);
    int run();

private:
    Simulation simulation_;
    std::filesystem::path result_path_;
    bool paused_{false};
    bool result_saved_{false};
    float turns_per_second_{8.0F};
    float accumulated_time_{};

    void update();
    void draw();
    void save_if_finished();
};

}  // namespace lucks
