#include "core/simulation.hpp"
#include "map/stack_generator.hpp"

#include <cassert>
#include <iostream>
#include <set>

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

    std::cout << "core invariants passed\n";
    return 0;
}
