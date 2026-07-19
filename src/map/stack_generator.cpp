#include "map/stack_generator.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace lucks {
namespace {

std::uint64_t position_key(Position p) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(p.y)) << 32U)
        | static_cast<std::uint32_t>(p.x);
}

}  // namespace

GameState StackMapGenerator::generate(const GameConfig& config) const {
    if (config.width < 15 || config.height < 11) {
        throw std::invalid_argument("stack map must be at least 15x11");
    }
    if (config.wall_layers < 1 || config.wall_layers > config.width / 3) {
        throw std::invalid_argument("wall_layers does not fit the map");
    }
    if (config.player_count < 1 || config.max_turns < 1) {
        throw std::invalid_argument("player_count and max_turns must be positive");
    }

    GameState state;
    state.config = config;
    state.cells.resize(static_cast<std::size_t>(config.width * config.height));

    for (int y = 0; y < config.height; ++y) {
        for (int x = 0; x < config.width; ++x) {
            if (x == 0 || y == 0 || x == config.width - 1 || y == config.height - 1) {
                state.cell({x, y}).tile = Tile::wall;
            }
        }
    }

    int first_wall_x = config.width;
    for (int layer = 0; layer < config.wall_layers; ++layer) {
        const int x = (layer + 1) * (config.width - 2) / (config.wall_layers + 1) + 1;
        first_wall_x = std::min(first_wall_x, x);
        const int upper_gap = 2 + (layer * 3) % std::max(1, config.height / 3);
        const int lower_gap = config.height - 3 - (layer * 3) % std::max(1, config.height / 3);
        const int gap_y = layer % 2 == 0 ? lower_gap : upper_gap;

        for (int y = 1; y < config.height - 1; ++y) {
            if (std::abs(y - gap_y) <= 1) {
                continue;
            }
            state.cell({x, y}).tile = Tile::wall;
        }
    }

    state.goal = {config.width - 2, config.height / 2};
    state.cell(state.goal).tile = Tile::goal;

    std::mt19937_64 random(config.seed);
    std::unordered_set<std::uint64_t> occupied;
    occupied.insert(position_key(state.goal));

    auto random_free_position = [&](int min_x, int max_x) {
        std::uniform_int_distribution<int> x_distribution(min_x, max_x);
        std::uniform_int_distribution<int> y_distribution(1, config.height - 2);
        for (int attempt = 0; attempt < config.width * config.height * 4; ++attempt) {
            Position candidate{x_distribution(random), y_distribution(random)};
            if (state.cell(candidate).tile == Tile::floor
                && !occupied.contains(position_key(candidate))) {
                occupied.insert(position_key(candidate));
                return candidate;
            }
        }
        throw std::runtime_error("map does not have enough free positions");
    };

    std::normal_distribution<double> starting_money(7.0, 6.0);
    std::uniform_int_distribution<int> action_count(1, 3);
    for (int id = 0; id < config.player_count; ++id) {
        Player player;
        player.id = id;
        player.name = "Player " + std::to_string(id + 1);
        player.position = random_free_position(1, std::max(1, first_wall_x - 1));
        player.inventory_capacity = 2 + static_cast<int>(random() % 3);
        player.max_actions_per_turn = action_count(random);
        player.emeralds = std::round(starting_money(random) * 10.0) / 10.0;
        if (id % 4 == 0) {
            // The first cohort deliberately spans strong to weak information.
            // At least one fully informed player keeps the MVP outcome observable.
            player.greedy_level = std::clamp(10 - id, 1, 10);
        }
        state.players.push_back(std::move(player));
    }

    auto place_item = [&](Item item) {
        Position position = random_free_position(1, config.width - 2);
        state.cell(position).item = item;
    };

    std::normal_distribution<double> emerald_value(8.0, 7.0);
    for (int i = 0; i < config.emerald_count; ++i) {
        const double amount = std::round(emerald_value(random) * 10.0) / 10.0;
        place_item(Item{ItemKind::emerald, amount, 0});
    }

    std::normal_distribution<double> greedy_level(4.0, 2.0);
    for (int i = 0; i < config.greedy_count; ++i) {
        const int level = std::clamp(static_cast<int>(std::lround(greedy_level(random))), 1, 10);
        place_item(Item{ItemKind::greedy, 0.0, level});
    }

    return state;
}

}  // namespace lucks
