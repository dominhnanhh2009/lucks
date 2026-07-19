#include "persistence/result_writer.hpp"

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace lucks {
namespace {

nlohmann::json player_json(const Player& player) {
    return {
        {"id", player.id},
        {"name", player.name},
        {"status", player.rank ? "ranked" : "eliminated"},
        {"rank", player.rank ? nlohmann::json(*player.rank) : nlohmann::json(nullptr)},
        {"position", {{"x", player.position.x}, {"y", player.position.y}}},
        {"inventory", {
            {"emeralds", player.emeralds},
            {"greedy_level", player.greedy_level},
            {"used_slots", player.used_inventory_slots()},
            {"capacity", player.inventory_capacity},
        }},
        {"max_actions_per_turn", player.max_actions_per_turn},
        {"turns_taken", player.turns_taken},
        {"items_collected", player.items_collected},
    };
}

}  // namespace

void write_result_json(const GameState& state, const std::filesystem::path& path) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }

    nlohmann::json document{
        {"schema_version", 1},
        {"simulation", {
            {"profile", "stack"},
            {"seed", state.config.seed},
            {"turns_played", state.turn},
            {"max_turns", state.config.max_turns},
            {"completed", state.finished},
            {"map", {
                {"width", state.config.width},
                {"height", state.config.height},
                {"wall_layers", state.config.wall_layers},
                {"goal", {{"x", state.goal.x}, {"y", state.goal.y}}},
            }},
        }},
        {"rankings", nlohmann::json::array()},
        {"players", nlohmann::json::array()},
    };

    for (const RankingEntry& entry : state.rankings) {
        document["rankings"].push_back({
            {"rank", entry.rank},
            {"player_id", entry.player_id},
            {"turn", entry.turn},
            {"emeralds", entry.emeralds},
            {"greedy_level", entry.greedy_level},
        });
    }
    for (const Player& player : state.players) {
        document["players"].push_back(player_json(player));
    }

    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not open result file: " + path.string());
    }
    output << std::setw(2) << document << '\n';
}

}  // namespace lucks
