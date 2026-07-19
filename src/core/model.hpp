#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lucks {

struct Position {
    int x{};
    int y{};

    friend bool operator==(const Position&, const Position&) = default;
};

enum class Tile : std::uint8_t {
    floor,
    wall,
    goal,
};

enum class ItemKind : std::uint8_t {
    emerald,
    greedy,
};

struct Item {
    ItemKind kind{ItemKind::emerald};
    double amount{};
    int level{};
};

struct Cell {
    Tile tile{Tile::floor};
    std::optional<Item> item;
};

struct Player {
    int id{};
    std::string name;
    Position position;
    double emeralds{};
    int greedy_level{};
    int inventory_capacity{3};
    int max_actions_per_turn{1};
    bool active{true};
    std::optional<int> rank;
    int turns_taken{};
    int items_collected{};

    [[nodiscard]] int used_inventory_slots() const {
        return (emeralds != 0.0 ? 1 : 0) + (greedy_level > 0 ? 1 : 0);
    }
};

struct RankingEntry {
    int player_id{};
    int rank{};
    int turn{};
    double emeralds{};
    int greedy_level{};
};

struct GameConfig {
    int width{31};
    int height{19};
    int wall_layers{5};
    int player_count{12};
    int emerald_count{28};
    int greedy_count{14};
    int max_turns{700};
    std::uint64_t seed{20260719};
};

struct GameState {
    GameConfig config;
    std::vector<Cell> cells;
    std::vector<Player> players;
    std::vector<RankingEntry> rankings;
    Position goal;
    int turn{};
    bool finished{};

    [[nodiscard]] bool inside(Position p) const {
        return p.x >= 0 && p.y >= 0 && p.x < config.width && p.y < config.height;
    }

    [[nodiscard]] int index(Position p) const {
        return p.y * config.width + p.x;
    }

    [[nodiscard]] const Cell& cell(Position p) const {
        return cells.at(static_cast<std::size_t>(index(p)));
    }

    [[nodiscard]] Cell& cell(Position p) {
        return cells.at(static_cast<std::size_t>(index(p)));
    }
};

enum class EventKind : std::uint8_t {
    moved,
    collected,
    finished,
    eliminated,
};

struct GameEvent {
    EventKind kind{EventKind::moved};
    int turn{};
    int player_id{};
    std::string message;
};

[[nodiscard]] inline const char* item_kind_name(ItemKind kind) {
    return kind == ItemKind::emerald ? "emerald" : "greedy";
}

}  // namespace lucks
