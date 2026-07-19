# lucks

`lucks` is a small simulation sandbox for studying how strongly a player's
outcome depends on its initial conditions.

The MVP generates a layered **stack** map, lets players act autonomously, orders
each turn under a configurable wealth-order rule, applies greedy items to local path choices,
ranks players that reach the goal, and writes the final state to JSON.

## Architecture

- `src/core`: game state, invariants, turn order, decisions, and ranking
- `src/map`: replaceable map/profile generation
- `src/persistence`: JSON result boundary
- `src/ui`: raylib/raygui visualizer; it only observes and advances the core
- `src/app`: executable wiring and command-line options

This split is deliberately small. The model owns game rules, while presentation,
generation, and storage do not depend on one another.

## Build on Windows

Requirements: CMake, Ninja, a C++20 compiler, and Git. The intended toolchain is
MSYS2 UCRT64. A local raylib/nlohmann-json package is used when found; otherwise
CMake downloads the dependency.

```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\bin\lucks.exe
```

Controls:

- `Space`: pause or run
- `Right arrow`: advance one turn
- `1x`, `4x`, `>>`: change simulation speed
- hover a player to inspect its inventory and action speed

The completed run is saved to `results/latest.json`.

## Headless/reproducible runs

```powershell
.\build\bin\lucks.exe --headless --seed 42
.\build\bin\lucks.exe --headless --seed 42 --max-turns 1000 --output results/seed-42.json
.\build\bin\lucks.exe --headless --seed 42 --turn-order probabilistic --output results/probabilistic-42.json
ctest --test-dir build --output-on-failure
```

## Turn-order treatments

- `neutral`: a uniform random order each turn; emeralds do not affect priority.
- `probabilistic`: players are drawn without replacement each turn, with weight
  `max(emeralds, 0) + 1`; wealth improves but never guarantees early priority.
- `deterministic` (default): players are ordered by emerald wealth, richest first.

The selected treatment is stored as `simulation.turn_order_mode` in every result JSON.
