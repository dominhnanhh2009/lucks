#include "core/simulation.hpp"
#include "map/stack_generator.hpp"

#include <cassert>
#include <iostream>
#include <set>
#include <vector>

namespace {

std::vector<int> first_turn_order(lucks::TurnOrderMode mode) {
    lucks::GameState state;
    state.config.width = 5;
    state.config.height = 5;
    state.config.max_turns = 1;
    state.config.seed = 42;
    state.config.turn_order_mode = mode;
    state.cells.resize(25);
    state.goal = {4, 4};
    state.cell(state.goal).tile = lucks::Tile::goal;
    state.players = {
        {0, "Low", {1, 1}, 1.0},
        {1, "High", {3, 1}, 100.0},
        {2, "Middle", {1, 3}, 5.0},
    };

    std::vector<int> order;
    lucks::Simulation simulation(std::move(state));
    simulation.set_event_sink([&](const lucks::GameEvent& event) {
        if (event.kind == lucks::EventKind::moved) {
            order.push_back(event.player_id);
        }
    });
    simulation.step_turn();
    return order;
}

}  // namespace

int main() {
    lucks::GameConfig config;
    config.seed = 42;
    config.player_count = 8;
    config.max_turns = 300;

    lucks::StackMapGenerator generator;
    lucks::GameState first = generator.generate(config);
    lucks::GameState second = generator.generate(config);

    assert(first.goal == second.goal);
    assert(first.players.size() == static_cast<std::size_t>(config.player_count));
    assert(first.cells.size() == static_cast<std::size_t>(config.width * config.height));

    std::set<std::pair<int, int>> occupied;
    for (const lucks::Player& player : first.players) {
        assert(first.cell(player.position).tile == lucks::Tile::floor);
        assert(occupied.emplace(player.position.x, player.position.y).second);
        assert(player.inventory_capacity >= player.used_inventory_slots());
    }

    lucks::Simulation simulation(std::move(first));
    simulation.run_to_completion();
    assert(simulation.state().finished);
    assert(simulation.state().turn <= config.max_turns);
    assert(!simulation.state().rankings.empty());
    for (std::size_t i = 0; i < simulation.state().rankings.size(); ++i) {
        assert(simulation.state().rankings[i].rank == static_cast<int>(i + 1));
    }

    const std::vector<int> deterministic = first_turn_order(
        lucks::TurnOrderMode::deterministic_privilege);
    assert((deterministic == std::vector<int>{1, 2, 0}));

    const std::vector<int> neutral = first_turn_order(lucks::TurnOrderMode::neutral);
    const std::vector<int> probabilistic = first_turn_order(
        lucks::TurnOrderMode::probabilistic_advantage);
    assert(neutral.size() == 3 && probabilistic.size() == 3);
    assert(std::set<int>(neutral.begin(), neutral.end()).size() == 3);
    assert(std::set<int>(probabilistic.begin(), probabilistic.end()).size() == 3);

    std::cout << "core invariants passed\n";
    return 0;
}
