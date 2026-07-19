#pragma once

#include "core/model.hpp"

#include <functional>
#include <random>
#include <span>
#include <vector>

namespace lucks {

class Simulation {
public:
    using EventSink = std::function<void(const GameEvent&)>;

    explicit Simulation(GameState initial_state);

    void step_turn();
    void run_to_completion();
    void set_event_sink(EventSink sink);

    [[nodiscard]] const GameState& state() const { return state_; }
    [[nodiscard]] std::span<const GameEvent> recent_events() const { return recent_events_; }

private:
    GameState state_;
    std::mt19937_64 random_;
    EventSink event_sink_;
    std::vector<GameEvent> recent_events_;

    void act(Player& player);
    void collect_item(Player& player);
    void finish_player(Player& player);
    void finish_game();
    void emit(EventKind kind, const Player& player, std::string message);

    [[nodiscard]] std::vector<Position> legal_neighbors(Position from) const;
    [[nodiscard]] Position choose_destination(const Player& player);
    [[nodiscard]] std::optional<Position> choose_greedy_target(const Player& player) const;
    [[nodiscard]] std::vector<int> ordered_active_player_ids() const;
};

}  // namespace lucks
