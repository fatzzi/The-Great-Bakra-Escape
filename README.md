
# The Great Bakra Escape


## 🎮 Game Description

"The Great Bakra Escape" is a whimsical multi-genre arcade game developed in C++ using the Raylib library. In this unique adventure, you find yourself in prison for the rather peculiar crime of "kidnapping a Qurbani Ka Bakra." Your only path to freedom is to conquer a series of diverse and challenging levels, each inspired by classic arcade genres. Will you "ESCAPE" or "SUFFER" in your cell?

The game features a humorous narrative, intuitive controls, and a progression system that keeps you engaged through varied gameplay mechanics across different levels.

## ✨ Features

* **Multi-Genre Gameplay**: Experience distinct gameplay styles in different levels:
    * **Maze Level**: Navigate dynamically generated labyrinths, collect coins, and find the exit.
    * **Space Invaders Level**: A classic top-down shooter where you defend against waves of alien invaders.
    * **Flappy Level**: A physics-based challenge requiring precise timing to guide a character through constantly moving pipes.
    * **Obstacle Course Level**: A platformer requiring agile movement and jumping to traverse obstacles and collect items.
* **Modular Level System**: Designed with an abstract `Levels` base class, allowing for easy expansion and integration of new game types.
* **Dynamic Generation**: The Maze Level generates a unique layout every time, and the Flappy Level features procedurally placed pipes.
* **Robust Memory Management**: Extensive use of `std::unique_ptr` for automatic memory deallocation and `Load`/`Unload` methods for efficient resource handling per level.
* **Frame-Rate Independent Logic**: Most game physics and movement are scaled by `deltaTime` (time elapsed between frames) to ensure consistent gameplay across different hardware (with a small note on Maze Level's `playerSpeed` implementation, see Technical Details).
* **Simple Controls**: Easy-to-learn keyboard controls for all levels.

## 🕹️ How to Play

### Global Controls:

* **Mouse Left Click**: Used for button interactions (e.g., "ESCAPE", "SUFFER", "Ready!").
* **Enter Key**: To return to the title screen from Game Over/Game Won screens.

### Level-Specific Controls:

* **Maze Level:**
    * **Arrow Keys (Left, Right, Up, Down)**: Move your character through the maze.
    * **Objective**: Collect all coins and reach the green exit.

* **Space Invaders Level:**
    * **Arrow Keys (Left, Right)**: Move your spaceship horizontally.
    * **Spacebar**: Fire bullets upwards.
    * **Objective**: Destroy all incoming invaders before they reach the bottom or you run out of lives.

* **Flappy Level:**
    * **Spacebar**: Make your character flap upwards.
    * **Objective**: Navigate through pipe gaps, avoid collisions, and reach a target score (e.g., 10 points). Watch your health!

* **Obstacle Course Level:**
    * **Arrow Keys (Left, Right)**: Move your character horizontally.
    * **Spacebar**: Jump.
    * **Objective**: Collect all coins and reach the exit door by traversing platforms and obstacles.

## 🔧 Technical Details

* **Language**: C++
* **Game Library**: [Raylib](https://www.raylib.com/) (version used in code snippet: v5.0, though specific version in your setup might vary).
* **Architecture**:
    * **Global State Machine**: The `main` loop uses a `GameScreen` enum (`TITLE_SCREEN_GLOBAL`, `PLAYING_LEVEL`, `LEVEL_TRANSITION`, `GAME_OVER_GLOBAL`, `GAME_WON_GLOBAL`) to manage the overall game flow.
    * **Polymorphic Levels**: An abstract `Levels` class provides a common interface (`Load`, `Unload`, `Update`, `Draw`, `IsComplete`, `GetName`, `GetInstructions`) for all game levels, enabling modular design.
    * **Level Queue**: `std::queue<std::unique_ptr<Levels>>` is used to define and manage the sequential order of levels in the game.
* **Memory Management**: Heavily relies on `std::unique_ptr` for automatic memory deallocation of game objects (levels, characters, projectiles, obstacles, coins), preventing memory leaks. Each level correctly implements `Load()` and `Unload()` methods to manage its specific resources.
* **Physics & Collision**:
    * **Delta Time (`GetFrameTime()`)**: Used to ensure consistent movement and physics simulations regardless of varying frame rates (applied to gravity, velocity-based movement).
    * **Raylib Collision Functions**: Utilizes `CheckCollisionRecs` and `CheckCollisionCircleRec` for efficient collision detection between bounding boxes and circles/rectangles.
    * **Directional Collision Handling**: The Obstacle Level features advanced collision logic to handle player interactions with platforms from top, bottom, left, and right, crucial for platformer physics.
* **Randomization**: `<random>` library is used for generating unique maze layouts, random invader firing patterns, and varied pipe gap positions.



## 🚀 Future Enhancements

* **Improved Graphics & Animations**: Add textures, more detailed sprites, and smoother animations.
* **Sound Effects & Music**: Implement background music and specific sound effects for player actions, collisions, etc.
* **User Interface Polishing**: Enhance menus, add a pause screen, and possibly a settings menu.
* **Score Persistence**: Implement saving and loading high scores.
* **More Levels**: Design and integrate new, unique level types.
* **Power-ups/Collectibles**: Introduce new items to enhance gameplay.
* **Difficulty Scaling**: Adjust game parameters based on player performance.


## 🙏 Acknowledgements

* Developed with [Raylib](https://www.raylib.com/), a fantastic open-source game programming library.
* Inspired by classic arcade games like Maze, Space Invaders, and Flappy Bird.

---
Enjoy your escape! 🐐
