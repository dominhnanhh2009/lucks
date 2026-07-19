#include "ui/visualizer.hpp"

#include "persistence/result_writer.hpp"

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#ifdef USE_LIBTYPE_SHARED
#undef USE_LIBTYPE_SHARED
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "raygui.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace lucks {
namespace {

constexpr int screen_width = 1320;
constexpr int screen_height = 820;
constexpr int sidebar_width = 380;
constexpr int margin = 24;

constexpr Color background{12, 15, 22, 255};
constexpr Color panel{21, 26, 36, 255};
constexpr Color panel_edge{48, 58, 75, 255};
constexpr Color floor_color{31, 38, 50, 255};
constexpr Color wall_color{79, 89, 106, 255};
constexpr Color goal_color{54, 190, 132, 255};
constexpr Color emerald_color{74, 222, 154, 255};
constexpr Color greedy_color{246, 184, 73, 255};
constexpr Color text_primary{230, 235, 243, 255};
constexpr Color text_muted{139, 151, 170, 255};

const char* turn_order_label(TurnOrderMode mode) {
    switch (mode) {
    case TurnOrderMode::neutral: return "neutral order";
    case TurnOrderMode::probabilistic_advantage: return "wealth-weighted order";
    case TurnOrderMode::deterministic_privilege: return "wealth-first order";
    }
    return "unknown order";
}

constexpr std::array<Color, 12> player_colors{{
    {111, 177, 255, 255}, {255, 117, 133, 255}, {184, 126, 255, 255},
    {255, 158, 86, 255}, {76, 210, 204, 255}, {245, 111, 203, 255},
    {153, 214, 88, 255}, {255, 221, 91, 255}, {116, 138, 255, 255},
    {234, 141, 84, 255}, {92, 198, 242, 255}, {197, 207, 222, 255},
}};

struct MapLayout {
    float cell_size{};
    float left{};
    float top{};
};

MapLayout map_layout(const GameState& state) {
    const float available_width = static_cast<float>(screen_width - sidebar_width - margin * 2);
    const float available_height = static_cast<float>(screen_height - margin * 2);
    const float cell_size = std::floor(std::min(
        available_width / state.config.width,
        available_height / state.config.height));
    const float map_width = cell_size * state.config.width;
    const float map_height = cell_size * state.config.height;
    return {
        cell_size,
        sidebar_width + margin + (available_width - map_width) * 0.5F,
        margin + (available_height - map_height) * 0.5F,
    };
}

Rectangle cell_rectangle(MapLayout layout, Position position) {
    return {
        layout.left + position.x * layout.cell_size,
        layout.top + position.y * layout.cell_size,
        layout.cell_size,
        layout.cell_size,
    };
}

const Player* hovered_player(const GameState& state, MapLayout layout, Vector2 mouse) {
    for (const Player& player : state.players) {
        if (player.active && CheckCollisionPointRec(mouse, cell_rectangle(layout, player.position))) {
            return &player;
        }
    }
    return nullptr;
}

void draw_map(const GameState& state) {
    const MapLayout layout = map_layout(state);
    DrawRectangle(
        static_cast<int>(layout.left - 2),
        static_cast<int>(layout.top - 2),
        static_cast<int>(layout.cell_size * state.config.width + 4),
        static_cast<int>(layout.cell_size * state.config.height + 4),
        panel_edge);

    for (int y = 0; y < state.config.height; ++y) {
        for (int x = 0; x < state.config.width; ++x) {
            const Position position{x, y};
            const Cell& cell = state.cell(position);
            const Rectangle rectangle = cell_rectangle(layout, position);
            Color color = floor_color;
            if (cell.tile == Tile::wall) {
                color = wall_color;
            } else if (cell.tile == Tile::goal) {
                color = goal_color;
            }
            DrawRectangleRec(rectangle, color);
            DrawRectangleLinesEx(rectangle, 1.0F, Color{42, 50, 64, 170});

            if (cell.tile == Tile::goal) {
                DrawText("GOAL", static_cast<int>(rectangle.x + 2),
                         static_cast<int>(rectangle.y + rectangle.height / 2 - 5),
                         std::max(8, static_cast<int>(layout.cell_size * 0.25F)),
                         Color{9, 47, 34, 255});
            }
            if (cell.item) {
                const bool emerald = cell.item->kind == ItemKind::emerald;
                const Color item_color = emerald ? emerald_color : greedy_color;
                const char* label = emerald ? "E" : "G";
                DrawCircleV(
                    {rectangle.x + rectangle.width / 2, rectangle.y + rectangle.height / 2},
                    std::max(3.0F, rectangle.width * 0.25F),
                    item_color);
                const int font_size = std::max(8, static_cast<int>(layout.cell_size * 0.36F));
                DrawText(label,
                         static_cast<int>(rectangle.x + rectangle.width / 2
                             - MeasureText(label, font_size) / 2.0F),
                         static_cast<int>(rectangle.y + rectangle.height / 2 - font_size / 2.0F),
                         font_size, Color{21, 26, 36, 255});
            }
        }
    }

    for (const Player& player : state.players) {
        if (!player.active) {
            continue;
        }
        const Rectangle rectangle = cell_rectangle(layout, player.position);
        const Color color = player_colors.at(
            static_cast<std::size_t>(player.id) % player_colors.size());
        const Vector2 center{
            rectangle.x + rectangle.width / 2,
            rectangle.y + rectangle.height / 2,
        };
        DrawCircleV(center, std::max(5.0F, rectangle.width * 0.37F), color);
        DrawCircleLinesV(center, std::max(5.0F, rectangle.width * 0.37F),
                         Color{245, 248, 252, 210});
        char id[8];
        std::snprintf(id, sizeof(id), "%d", player.id + 1);
        const int font_size = std::max(9, static_cast<int>(layout.cell_size * 0.42F));
        DrawText(id, static_cast<int>(center.x - MeasureText(id, font_size) / 2.0F),
                 static_cast<int>(center.y - font_size / 2.0F), font_size,
                 Color{13, 17, 24, 255});
    }

    const Player* hovered = hovered_player(state, layout, GetMousePosition());
    if (hovered) {
        const Vector2 mouse = GetMousePosition();
        const float width = 210.0F;
        const float height = 91.0F;
        const float x = std::min(mouse.x + 14.0F, static_cast<float>(screen_width) - width - 8.0F);
        const float y = std::min(mouse.y + 14.0F, static_cast<float>(screen_height) - height - 8.0F);
        DrawRectangleRounded({x, y, width, height}, 0.08F, 5, Color{18, 23, 32, 245});
        DrawRectangleRoundedLines({x, y, width, height}, 0.08F, 5, panel_edge);
        DrawText(hovered->name.c_str(), static_cast<int>(x + 12), static_cast<int>(y + 10),
                 17, text_primary);
        DrawText(TextFormat("%.1f emerald", hovered->emeralds),
                 static_cast<int>(x + 12), static_cast<int>(y + 34), 14, emerald_color);
        DrawText(TextFormat("greedy %d  |  %d actions/turn",
                            hovered->greedy_level, hovered->max_actions_per_turn),
                 static_cast<int>(x + 12), static_cast<int>(y + 57), 14, text_muted);
    }
}

void draw_player_table(const GameState& state) {
    std::vector<const Player*> players;
    for (const Player& player : state.players) {
        players.push_back(&player);
    }
    std::stable_sort(players.begin(), players.end(), [](const Player* left, const Player* right) {
        if (left->active != right->active) {
            return left->active > right->active;
        }
        if (left->rank && right->rank) {
            return *left->rank < *right->rank;
        }
        return left->emeralds > right->emeralds;
    });

    DrawText("PLAYERS", 24, 204, 13, text_muted);
    DrawText(turn_order_label(state.config.turn_order_mode), 24, 223, 12, Color{102, 115, 135, 255});
    int y = 250;
    for (const Player* player : players) {
        const Color color = player_colors.at(
            static_cast<std::size_t>(player->id) % player_colors.size());
        DrawCircle(32, y + 8, 6, color);
        DrawText(player->name.c_str(), 46, y, 14,
                 player->active ? text_primary : text_muted);
        const char* status = player->rank ? TextFormat("#%d", *player->rank)
                                          : (player->active ? "active" : "out");
        DrawText(status, 137, y, 13, player->rank ? goal_color : text_muted);
        DrawText(TextFormat("%6.1f E", player->emeralds), 188, y, 13, emerald_color);
        DrawText(TextFormat("G%d", player->greedy_level), 261, y, 13, greedy_color);
        DrawText(TextFormat("%dx", player->max_actions_per_turn), 309, y, 13, text_muted);
        y += 23;
    }
}

void draw_events(const Simulation& simulation) {
    DrawText("RECENT EVENTS", 24, 548, 13, text_muted);
    int y = 571;
    const auto events = simulation.recent_events();
    const std::size_t begin = events.size() > 7 ? events.size() - 7 : 0;
    for (std::size_t i = begin; i < events.size(); ++i) {
        const GameEvent& event = events[i];
        if (event.kind == EventKind::moved) {
            continue;
        }
        Color color = text_muted;
        if (event.kind == EventKind::finished) color = goal_color;
        if (event.kind == EventKind::collected) color = greedy_color;
        DrawText(TextFormat("T%03d", event.turn), 24, y, 12, Color{91, 104, 123, 255});
        std::string message = event.message;
        if (message.size() > 38) {
            message.resize(35);
            message += "...";
        }
        DrawText(message.c_str(), 65, y, 12, color);
        y += 20;
    }
}

}  // namespace

Visualizer::Visualizer(GameState initial_state, std::filesystem::path result_path)
    : simulation_(std::move(initial_state)), result_path_(std::move(result_path)) {}

void Visualizer::save_if_finished() {
    if (simulation_.state().finished && !result_saved_) {
        write_result_json(simulation_.state(), result_path_);
        result_saved_ = true;
        paused_ = true;
    }
}

void Visualizer::update() {
    if (IsKeyPressed(KEY_SPACE)) {
        paused_ = !paused_;
    }
    if (IsKeyPressed(KEY_RIGHT) && !simulation_.state().finished) {
        simulation_.step_turn();
    }

    if (!paused_ && !simulation_.state().finished) {
        accumulated_time_ += GetFrameTime();
        const float interval = 1.0F / turns_per_second_;
        int safety_limit = 0;
        while (accumulated_time_ >= interval && safety_limit++ < 100) {
            simulation_.step_turn();
            accumulated_time_ -= interval;
        }
    }
    save_if_finished();
}

void Visualizer::draw() {
    const GameState& state = simulation_.state();
    BeginDrawing();
    ClearBackground(background);
    DrawRectangle(0, 0, sidebar_width, screen_height, panel);
    DrawLine(sidebar_width - 1, 0, sidebar_width - 1, screen_height, panel_edge);

    DrawText("LUCKS", 24, 20, 29, text_primary);
    DrawText("initial conditions sandbox", 25, 53, 14, text_muted);
    DrawText(TextFormat("STACK  /  seed %llu",
                        static_cast<unsigned long long>(state.config.seed)),
             24, 82, 12, Color{107, 124, 150, 255});

    DrawText(TextFormat("TURN %d / %d", state.turn, state.config.max_turns),
             24, 115, 19, text_primary);
    DrawText(TextFormat("%d ranked  |  %d active",
                        static_cast<int>(state.rankings.size()),
                        static_cast<int>(std::ranges::count_if(state.players,
                            [](const Player& player) { return player.active; }))),
             24, 141, 14, text_muted);

    const char* pause_label = paused_ ? "Run  [Space]" : "Pause  [Space]";
    if (GuiButton({24, 169, 124, 28}, pause_label)) {
        paused_ = !paused_;
    }
    if (GuiButton({156, 169, 76, 28}, "Step  [>]") && !state.finished) {
        simulation_.step_turn();
    }
    if (GuiButton({240, 169, 43, 28}, "1x")) turns_per_second_ = 4.0F;
    if (GuiButton({289, 169, 43, 28}, "4x")) turns_per_second_ = 16.0F;
    if (GuiButton({338, 169, 34, 28}, ">>")) turns_per_second_ = 240.0F;

    draw_player_table(state);
    draw_events(simulation_);
    draw_map(state);

    DrawText("E emerald     G greedy item     # ranked", 24, 785, 12, text_muted);
    if (state.finished) {
        const Rectangle banner{
            static_cast<float>(sidebar_width + 155),
            static_cast<float>(screen_height - 76),
            590.0F, 50.0F,
        };
        DrawRectangleRounded(banner, 0.12F, 6, Color{20, 64, 49, 245});
        DrawRectangleRoundedLines(banner, 0.12F, 6, Color{61, 202, 142, 255});
        const std::string message = "Simulation complete - saved to " + result_path_.string();
        DrawText(message.c_str(), static_cast<int>(banner.x + 18),
                 static_cast<int>(banner.y + 16), 15, Color{184, 244, 215, 255});
    }

    EndDrawing();
}

int Visualizer::run() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(screen_width, screen_height, "lucks - initial conditions sandbox");
    SetTargetFPS(120);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    GuiSetStyle(DEFAULT, BORDER_WIDTH, 1);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, 0x293243FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, 0x354359FF);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, 0x435572FF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x4A5870FF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x7E9AC6FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xE1E7F0FF);

    while (!WindowShouldClose()) {
        update();
        draw();
    }

    CloseWindow();
    return 0;
}

}  // namespace lucks
