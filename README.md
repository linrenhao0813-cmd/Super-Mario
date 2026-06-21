# Super Mario Bros - C++ Terminal Edition

A faithful recreation of the classic Super Mario Bros using C++ and ncurses for terminal rendering.

## Features

- **80 Levels** across 20 worlds with procedural generation
- **4 Unique Themes**:
  - 🌿 Grass Land (Worlds 1-5): Pipes, bricks, and classic obstacles
  - 🕳️ Underground (Worlds 6-10): Dark caverns with ceiling tiles
  - ☁️ Sky World (Worlds 11-15): Floating platforms and daring jumps
  - 🔥 Bowser Castle (Worlds 16-20): Hardcore challenges with hard blocks

- **Classic Mario Mechanics**:
  - Variable jump height (hold W to jump higher)
  - Running acceleration and skidding
  - Stomp enemies to defeat them
  - Break bricks when powered up
  - Hit question blocks for coins and power-ups

- **Enemies**:
  - 🍄 Goombas - Stomp to defeat
  - 🐢 Koopas - Stomp to get shell, kick shell to defeat others

- **Power-ups**:
  - 🍄 Mushroom - Grow big
  - 🔥 Fire Flower - Shoot fireballs
  - ⭐ Star - Temporary invincibility

## Controls

| Key | Action |
|-----|--------|
| `A` | Move Left |
| `D` | Move Right |
| `W` | Jump (hold for higher jump) |
| `S` | Duck |
| `Space` | Shoot Fireball (when Fire Mario) |
| `X` | Run |
| `P` | Pause |
| `Q` | Quit |

## Building

### Prerequisites
- C++17 compatible compiler (GCC 7+ or Clang 5+)
- ncurses library

### Compile
```bash
make
```

### Run
```bash
./super_mario
```

## Project Structure

```
super_mario/
├── Makefile            # Build configuration
├── README.md           # This file
├── super_mario         # Compiled binary
└── src/
    ├── defs.h          # Constants, enums, data structures
    ├── game.h          # Function declarations
    ├── game.cpp        # Core game logic (physics, rendering, AI)
    ├── level_data.h    # Procedural level generator
    └── main.cpp        # Entry point and input handling
```

## Gameplay Tips

1. **Jump Timing**: Press W and release quickly for short hops, hold for maximum height
2. **Running**: Hold X while moving to run faster and jump further
3. **Fireballs**: Collect Fire Flower from question blocks, then press Space to shoot
4. **Enemies**: Jump on top of enemies to defeat them, avoid touching from the side
5. **Koopa Shells**: Stomp a Koopa to get a shell, then kick it into other enemies
6. **Power-ups**: Duck under low passages as small Mario to avoid damage

## Level Themes

### Grass Land (Worlds 1-5)
Classic overworld with green ground, pipes, and brick formations. Good for learning the controls.

### Underground (Worlds 6-10)
Dark underground with ceiling tiles. More challenging platforming and tighter spaces.

### Sky World (Worlds 11-15)
Floating platforms with no ground below. One wrong jump means falling into the abyss.

### Bowser Castle (Worlds 16-20)
The ultimate challenge! Hard block mazes, fewer power-ups, and dense enemy placement.

## Technical Details

- **Language**: C++17
- **Graphics**: ncurses terminal library
- **Physics**: Custom 2D platformer engine with gravity, acceleration, and collision detection
- **Level Generation**: Seeded random number generator for consistent yet unique levels
- **Frame Rate**: 60 FPS target

## Controls (Alternative)

Arrow keys and other keys also work:
- Arrow Left/Right: Move
- Arrow Up / Z / Space: Jump
- Arrow Down: Duck
- X: Run/Fire

## License

This is a fan-made tribute project for educational purposes.
