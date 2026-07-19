#include "map/stack_generator.hpp"
#include "persistence/result_writer.hpp"
#include "ui/visualizer.hpp"

#include <charconv>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

struct Options {
    bool headless{};
    lucks::GameConfig config;
    std::filesystem::path output{"results/latest.json"};
};

void print_usage() {
    std::cout
        << "Usage: lucks [--headless] [--seed N] [--turn-order MODE] [--max-turns N] [--output PATH]\n"
        << "  --headless     run without opening the visualizer\n"
        << "  --seed N       choose a reproducible initial condition\n"
        << "  --turn-order MODE  neutral, probabilistic, or deterministic (default)\n"
        << "  --max-turns N  override the simulation turn limit\n"
        << "  --output PATH  choose the final JSON result path\n";
}

bool parse_turn_order_mode(std::string_view text, lucks::TurnOrderMode& mode) {
    if (text == "neutral") {
        mode = lucks::TurnOrderMode::neutral;
    } else if (text == "probabilistic") {
        mode = lucks::TurnOrderMode::probabilistic_advantage;
    } else if (text == "deterministic") {
        mode = lucks::TurnOrderMode::deterministic_privilege;
    } else {
        return false;
    }
    return true;
}

template <typename Number>
bool parse_number(std::string_view text, Number& value) {
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument == "--headless") {
            options.headless = true;
        } else if (argument == "--help" || argument == "-h") {
            print_usage();
            std::exit(0);
        } else if (argument == "--seed" && i + 1 < argc) {
            if (!parse_number(std::string_view(argv[++i]), options.config.seed)) {
                throw std::invalid_argument("invalid --seed value");
            }
        } else if (argument == "--max-turns" && i + 1 < argc) {
            if (!parse_number(std::string_view(argv[++i]), options.config.max_turns)
                || options.config.max_turns < 1) {
                throw std::invalid_argument("invalid --max-turns value");
            }
        } else if (argument == "--turn-order" && i + 1 < argc) {
            if (!parse_turn_order_mode(argv[++i], options.config.turn_order_mode)) {
                throw std::invalid_argument("invalid --turn-order value (use neutral, probabilistic, or deterministic)");
            }
        } else if (argument == "--output" && i + 1 < argc) {
            options.output = argv[++i];
        } else {
            throw std::invalid_argument("unknown or incomplete option: " + std::string(argument));
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        lucks::StackMapGenerator generator;
        lucks::GameState initial_state = generator.generate(options.config);

        if (options.headless) {
            lucks::Simulation simulation(std::move(initial_state));
            simulation.run_to_completion();
            lucks::write_result_json(simulation.state(), options.output);
            std::cout << "Simulation completed in " << simulation.state().turn
                      << " turns; " << simulation.state().rankings.size()
                      << " player(s) reached the goal.\n"
                      << "Result: " << options.output.string() << '\n';
            return 0;
        }

        lucks::Visualizer visualizer(std::move(initial_state), options.output);
        return visualizer.run();
    } catch (const std::exception& error) {
        std::cerr << "lucks: " << error.what() << '\n';
        return 1;
    }
}
