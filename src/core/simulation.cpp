#include "core/simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <numeric>
#include <sstream>

namespace lucks {
namespace {

constexpr std::array<Position, 4> directions{{
    {1, 0},
    {-1, 0},
    {0, 1},
    {0, -1},
}};

Position add(Position left, Position right) {
    return {left.x + right.x, left.y + right.y};
}

}  // namespace

Simulation::Simulation(GameState initial_state)
    : state_(std::move(initial_state)), random_(state_.config.seed) {}

void Simulation::set_event_sink(EventSink sink) {
    event_sink_ = std::move(sink);
}

void Simulation::emit(EventKind kind, const Player& player, std::string message) {
    GameEvent event{kind, state_.turn, player.id, std::move(message)};
    if (kind != EventKind::moved) {
        recent_events_.push_back(event);
        if (recent_events_.size() > 12) {
            recent_events_.erase(recent_events_.begin());
        }
    }
    if (event_sink_) {
        event_sink_(event);
    }
}

std::vector<int> Simulation::ordered_active_player_ids() {
    std::vector<int> ids;
    for (const Player& player : state_.players) {
        if (player.active) {
            ids.push_back(player.id);
        }
    }
    if (state_.config.turn_order_mode == TurnOrderMode::neutral) {
        std::shuffle(ids.begin(), ids.end(), random_);
        return ids;
    }

    if (state_.config.turn_order_mode == TurnOrderMode::probabilistic_advantage) {
        // Draw a complete order without replacement. A player's weight is always
        // positive, while each additional emerald increases its chance of an early turn.
        std::vector<int> order;
        order.reserve(ids.size());
        while (!ids.empty()) {
            double total_weight = 0.0;
            for (int id : ids) {
                total_weight += std::max(0.0, state_.players.at(static_cast<std::size_t>(id)).emeralds) + 1.0;
            }
            std::uniform_real_distribution<double> draw(0.0, total_weight);
            double threshold = draw(random_);
            std::size_t selected = ids.size() - 1;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                threshold -= std::max(0.0,
                    state_.players.at(static_cast<std::size_t>(ids[i])).emeralds) + 1.0;
                if (threshold <= 0.0) {
                    selected = i;
                    break;
                }
            }
            order.push_back(ids[selected]);
            ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(selected));
        }
        return order;
    }

    std::stable_sort(ids.begin(), ids.end(), [&](int left, int right) {
        const Player& a = state_.players.at(static_cast<std::size_t>(left));
        const Player& b = state_.players.at(static_cast<std::size_t>(right));
        if (a.emeralds != b.emeralds) {
            return a.emeralds > b.emeralds;
        }
        return a.id < b.id;
    });
    return ids;
}

std::vector<Position> Simulation::legal_neighbors(Position from) const {
    std::vector<Position> result;
    for (Position direction : directions) {
        Position candidate = add(from, direction);
        if (!state_.inside(candidate) || state_.cell(candidate).tile == Tile::wall) {
            continue;
        }
        const bool occupied = std::ranges::any_of(state_.players, [&](const Player& player) {
            return player.active && player.position == candidate;
        });
        if (!occupied) {
            result.push_back(candidate);
        }
    }
    return result;
}

std::optional<Position> Simulation::choose_greedy_target(const Player& player) const {
    if (player.greedy_level <= 0) {
        return std::nullopt;
    }

    const int cell_count = state_.config.width * state_.config.height;
    std::vector<int> distance(static_cast<std::size_t>(cell_count), -1);
    std::deque<Position> queue;
    distance.at(static_cast<std::size_t>(state_.index(player.position))) = 0;
    queue.push_back(player.position);

    std::optional<Position> best;
    double best_score = -std::numeric_limits<double>::infinity();
    int best_distance = std::numeric_limits<int>::max();

    while (!queue.empty()) {
        const Position current = queue.front();
        queue.pop_front();
        const int current_distance = distance.at(static_cast<std::size_t>(state_.index(current)));
        if (current_distance > player.greedy_level) {
            continue;
        }

        double score = -std::numeric_limits<double>::infinity();
        const Cell& cell = state_.cell(current);
        if (cell.tile == Tile::goal) {
            score = 1'000'000.0;
        } else if (cell.item) {
            score = cell.item->kind == ItemKind::greedy
                ? 10'000.0 + cell.item->level
                : cell.item->amount;
        }
        if (score > best_score || (score == best_score && current_distance < best_distance)) {
            best = current;
            best_score = score;
            best_distance = current_distance;
        }

        if (current_distance == player.greedy_level) {
            continue;
        }
        for (Position direction : directions) {
            const Position next = add(current, direction);
            if (!state_.inside(next) || state_.cell(next).tile == Tile::wall) {
                continue;
            }
            int& next_distance = distance.at(static_cast<std::size_t>(state_.index(next)));
            if (next_distance == -1) {
                next_distance = current_distance + 1;
                queue.push_back(next);
            }
        }
    }
    return best;
}

Position Simulation::choose_destination(const Player& player) {
    std::vector<Position> legal = legal_neighbors(player.position);
    if (legal.empty()) {
        return player.position;
    }

    std::optional<Position> target;
    if (player.greedy_level > 0) {
        // Goal knowledge becomes more reliable with each greedy level. When
        // distracted, the player only evaluates opportunities inside that radius.
        const double goal_focus = std::min(1.0, 0.80 + player.greedy_level * 0.02);
        std::bernoulli_distribution follows_shortest_path(goal_focus);
        target = follows_shortest_path(random_)
            ? std::optional<Position>(state_.goal)
            : choose_greedy_target(player);
    }
    if (target && *target != player.position) {
        const int cell_count = state_.config.width * state_.config.height;
        std::vector<int> distance(static_cast<std::size_t>(cell_count), -1);
        std::deque<Position> queue;
        distance.at(static_cast<std::size_t>(state_.index(*target))) = 0;
        queue.push_back(*target);

        while (!queue.empty()) {
            Position current = queue.front();
            queue.pop_front();
            for (Position direction : directions) {
                const Position next = add(current, direction);
                if (!state_.inside(next) || state_.cell(next).tile == Tile::wall) {
                    continue;
                }
                const bool occupied_by_other = std::ranges::any_of(
                    state_.players, [&](const Player& other) {
                        return other.active && other.id != player.id && other.position == next;
                    });
                if (occupied_by_other) {
                    continue;
                }
                int& next_distance = distance.at(static_cast<std::size_t>(state_.index(next)));
                if (next_distance == -1) {
                    next_distance = distance.at(static_cast<std::size_t>(state_.index(current))) + 1;
                    queue.push_back(next);
                }
            }
        }

        std::stable_sort(legal.begin(), legal.end(), [&](Position left, Position right) {
            const int left_distance = distance.at(static_cast<std::size_t>(state_.index(left)));
            const int right_distance = distance.at(static_cast<std::size_t>(state_.index(right)));
            return left_distance >= 0
                && (right_distance < 0 || left_distance < right_distance);
        });
        return legal.front();
    }

    std::uniform_int_distribution<std::size_t> choice(0, legal.size() - 1);
    return legal.at(choice(random_));
}

bool Simulation::collect_item(Player& player) {
    Cell& cell = state_.cell(player.position);
    if (!cell.item) {
        return false;
    }

    const Item item = *cell.item;
    const bool needs_new_slot = item.kind == ItemKind::emerald
        ? player.emeralds == 0.0 && item.amount != 0.0
        : player.greedy_level == 0;
    if (needs_new_slot && player.used_inventory_slots() >= player.inventory_capacity) {
        return false;
    }

    std::ostringstream message;
    if (item.kind == ItemKind::emerald) {
        player.emeralds += item.amount;
        if (std::abs(player.emeralds) < 0.0001) {
            player.emeralds = 0.0;
        }
        message << player.name << " collected " << item.amount << " emerald";
    } else {
        if (item.level <= player.greedy_level) {
            return false;
        }
        player.greedy_level = item.level;
        message << player.name << " upgraded greedy to " << item.level;
    }

    cell.item.reset();
    ++player.items_collected;
    emit(EventKind::collected, player, message.str());
    return true;
}

void Simulation::finish_player(Player& player) {
    player.active = false;
    player.rank = static_cast<int>(state_.rankings.size()) + 1;
    state_.rankings.push_back({
        player.id,
        *player.rank,
        state_.turn,
        player.emeralds,
        player.greedy_level,
    });
    emit(EventKind::finished, player, player.name + " reached the goal at rank #"
        + std::to_string(*player.rank));
}

void Simulation::act(Player& player) {
    if (!player.active) {
        return;
    }
    if (state_.cell(player.position).item) {
        if (collect_item(player)) {
            return;
        }
    }

    const Position destination = choose_destination(player);
    if (destination != player.position) {
        player.position = destination;
        emit(EventKind::moved, player, player.name + " moved");
    }
    if (state_.cell(player.position).tile == Tile::goal) {
        finish_player(player);
    }
}

void Simulation::finish_game() {
    if (state_.finished) {
        return;
    }
    for (Player& player : state_.players) {
        if (player.active) {
            player.active = false;
            emit(EventKind::eliminated, player, player.name + " was eliminated");
        }
    }
    state_.finished = true;
}

void Simulation::step_turn() {
    if (state_.finished) {
        return;
    }

    ++state_.turn;
    const std::vector<int> order = ordered_active_player_ids();
    for (int id : order) {
        Player& player = state_.players.at(static_cast<std::size_t>(id));
        ++player.turns_taken;
        for (int action = 0; action < player.max_actions_per_turn && player.active; ++action) {
            act(player);
        }
    }

    const bool anyone_active = std::ranges::any_of(state_.players, [](const Player& player) {
        return player.active;
    });
    if (!anyone_active || state_.turn >= state_.config.max_turns) {
        finish_game();
    }
}

void Simulation::run_to_completion() {
    while (!state_.finished) {
        step_turn();
    }
}

}  // namespace lucks
