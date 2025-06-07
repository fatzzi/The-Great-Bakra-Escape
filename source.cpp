#include "raylib.h"
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <memory>
#include <queue>
#include <iostream>
#include <functional>

const int GLOBAL_SCREEN_WIDTH = 1280;
const int GLOBAL_SCREEN_HEIGHT = 720;

// Variables for the starting screen's "suffer" message
static bool showSufferMessage = false;
static float sufferMessageTimer = 0.0f;
const float SUFFER_MESSAGE_DISPLAY_TIME = 2.0f; // How long the message sticks around


// The base class for all our game levels. Each level will inherit from this!
class Levels {
public:
    Levels(int screenW, int screenH) : screenWidth(screenW), screenHeight(screenH) {}
    virtual ~Levels() = default; // Important for proper cleanup of derived classes

    virtual void Load() = 0;             // Get level-specific stuff ready
    virtual void Unload() = 0;           // Clean up level-specific stuff
    virtual void Update(float deltaTime) = 0; // Update game logic for the level
    virtual void Draw() = 0;             // Draw everything in the level
    virtual bool IsComplete() = 0;       // Check if the level is done (won or lost)
    virtual std::string GetName() const = 0; 
    virtual std::string GetInstructions() const = 0; 

protected:
    int screenWidth;
    int screenHeight;
};

// function to keep values within a certain range
std::function<float(float, float, float)> minmax = [](float value, float min_val, float max_val) -> float {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
};

// Constants specific to the Maze Level
const int MAZE_BASE_WIDTH_CELLS = 35;
const int MAZE_BASE_HEIGHT_CELLS = 21;

const Color MAZE_WALL_COLOR = { 128, 0, 128, 255 }; // Walls are purple
const Color MAZE_PATH_COLOR = { 0, 0, 0, 255 };     // Paths are black
const Color MAZE_PLAYER_COLOR = { 255, 215, 0, 255 }; // Player is gold
const Color MAZE_PLAYER_EYE_COLOR = { 0, 0, 0, 255 }; // Player eyes are black
const Color MAZE_COIN_COLOR = { 255, 193, 7, 255 };   // Coins are orange-yellow
const Color MAZE_START_COLOR = { 0, 0, 0, 255 };     // Start is black
const Color MAZE_END_COLOR = { 50, 205, 50, 255 };   // End is green
const Color MAZE_TEXT_COLOR = { 245, 245, 245, 255 }; // Text is off-white

// first level: the Maze
class MazeLevel : public Levels {
public:
    MazeLevel(int screenW, int screenH);
    ~MazeLevel() override;

    void Load() override;
    void Unload() override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsComplete() override;
    std::string GetName() const override { return "Maze Level"; }
    std::string GetInstructions() const override { return "Navigate the maze using ARROW keys. \n \n Collect all coins and reach the green exit to win."; }

    void GenerateNewMazeStructure();

private:
    std::vector<std::vector<bool>> mazeGrid; // True means wall, false means path
    int mazeWidthCells;
    int mazeHeightCells;
    float cellSizePixels;
    int startCol, startRow;
    int endCol, endRow;

    float playerX, playerY;
    float playerSize;
    float playerSpeed;

    std::vector<Vector2> coins;
    int totalInitialCoins;
    int collectedCoins;
    float coinSize;
    const float COIN_SPAWN_CHANCE = 0.3f; // Chance for a coin to appear in a path cell

    bool levelWon;
    bool mazeGeneratedForPreview; 

    void InitMazeGrid();
    void RecursiveGenerateMaze(int r, int c); 
    bool CheckWallCollision(float px, float py, float pSize, float dx, float dy);
    void ResetPlayerAndCoins();
    void CalculateMazeDimensions(); 
};

// Random number generators for the maze
static std::random_device s_maze_rd;
static std::mt19937 s_maze_gen(s_maze_rd());
static std::uniform_real_distribution<> s_maze_dis(0.0, 1.0);

MazeLevel::MazeLevel(int screenW, int screenH)
    : Levels(screenW, screenH),
      mazeWidthCells(0), mazeHeightCells(0), cellSizePixels(0.0f),
      startCol(0), startRow(0), endCol(0), endRow(0),
      playerX(0), playerY(0), playerSize(0), playerSpeed(3.0f),
      coinSize(0), totalInitialCoins(0), collectedCoins(0),
      levelWon(false), mazeGeneratedForPreview(false)
{
    CalculateMazeDimensions();
    InitMazeGrid();
}

MazeLevel::~MazeLevel() {
    Unload();
}

void MazeLevel::CalculateMazeDimensions() {
    mazeWidthCells = MAZE_BASE_WIDTH_CELLS;
    if (mazeWidthCells % 2 == 0) mazeWidthCells++; // Making sure it's odd for maze generation
    mazeHeightCells = MAZE_BASE_HEIGHT_CELLS;
    if (mazeHeightCells % 2 == 0) mazeHeightCells++; 

    float cellWidthByScreen = (float)screenWidth / mazeWidthCells;
    float cellHeightByScreen = (float)screenHeight / mazeHeightCells;

    cellSizePixels = std::floor(std::min(cellWidthByScreen, cellHeightByScreen));

    mazeWidthCells = (int)(screenWidth / cellSizePixels);
    if (mazeWidthCells % 2 == 0) mazeWidthCells--;
    if (mazeWidthCells <= 0) mazeWidthCells = 1;

    mazeHeightCells = (int)(screenHeight / cellSizePixels);
    if (mazeHeightCells % 2 == 0) mazeHeightCells--;
    if (mazeHeightCells <= 0) mazeHeightCells = 1;

    if (mazeWidthCells < 3) mazeWidthCells = 3; // Minimum maze size
    if (mazeHeightCells < 3) mazeHeightCells = 3;

    startCol = 1; // Starting point for the player
    startRow = 1;
    endCol = mazeWidthCells - 2; // Ending point for the player
    endRow = mazeHeightCells - 2;

    playerSize = cellSizePixels * 0.6f;
    coinSize = cellSizePixels * 0.3f;
}

void MazeLevel::InitMazeGrid() {
    mazeGrid.clear();
    mazeGrid.resize(mazeHeightCells, std::vector<bool>(mazeWidthCells, true)); // All cells start as walls
}

void MazeLevel::RecursiveGenerateMaze(int r, int c) {
    mazeGrid[r][c] = false; // Mark current cell as path

    int dr[] = {-2, 0, 2, 0}; // Directions for moving two cells at a time (skipping a wall)
    int dc[] = {0, 2, 0, -2};

    std::vector<int> directions = {0, 1, 2, 3};
    std::shuffle(directions.begin(), directions.end(), s_maze_gen); // Randomize directions

    for (int dir : directions) {
        int nextR = r + dr[dir];
        int nextC = c + dc[dir];
        int wallR = r + dr[dir] / 2; // Wall in between current and next cell
        int wallC = c + dc[dir] / 2;

        // If next cell is within bounds and is still a wall, make a path
        if (nextR >= 0 && nextR < mazeHeightCells && nextC >= 0 && nextC < mazeWidthCells && mazeGrid[nextR][nextC]) {
            mazeGrid[wallR][wallC] = false; // the wall
            RecursiveGenerateMaze(nextR, nextC); 
        }
    }
}

void MazeLevel::GenerateNewMazeStructure() {
    CalculateMazeDimensions();
    InitMazeGrid();
    RecursiveGenerateMaze(startRow, startCol);
    mazeGeneratedForPreview = true;
}

void MazeLevel::ResetPlayerAndCoins() {
    // Put player back at the start
    playerX = startCol * cellSizePixels + (cellSizePixels - playerSize) / 2;
    playerY = startRow * cellSizePixels + (cellSizePixels - playerSize) / 2;
    levelWon = false;

    coins.clear();
    collectedCoins = 0;
    totalInitialCoins = 0;

    // Distribute coins randomly in path cells
    for (int r = 0; r < mazeHeightCells; ++r) {
        for (int c = 0; c < mazeWidthCells; ++c) {
            // If it's a path cell and not the start/end
            if (!mazeGrid[r][c] && !(r == startRow && c == startCol) && !(r == endRow && c == endCol)) {
                if (s_maze_dis(s_maze_gen) < COIN_SPAWN_CHANCE) {
                    float coinX = c * cellSizePixels + cellSizePixels / 2;
                    float coinY = r * cellSizePixels + cellSizePixels / 2;
                    coins.push_back({coinX, coinY});
                    totalInitialCoins++;
                }
            }
        }
    }
}

void MazeLevel::Load() {
    CalculateMazeDimensions();
    if (!mazeGeneratedForPreview) {
        GenerateNewMazeStructure();
    }
    ResetPlayerAndCoins(); // Set up player and coins for the current maze
}

void MazeLevel::Unload() {
    mazeGrid.clear();
    coins.clear();
    mazeGeneratedForPreview = false; // Reset for next time we load a maze
}

bool MazeLevel::CheckWallCollision(float px, float py, float pSize, float dx, float dy) {
    float newPlayerX = px + dx;
    float newPlayerY = py + dy;
    Rectangle playerRect = {newPlayerX, newPlayerY, pSize, pSize};

    // Calculate which maze cells the player is currently overlapping
    int minCol = static_cast<int>(playerRect.x / cellSizePixels);
    int maxCol = static_cast<int>((playerRect.x + playerRect.width - 1) / cellSizePixels);
    int minRow = static_cast<int>(playerRect.y / cellSizePixels);
    int maxRow = static_cast<int>((playerRect.y + playerRect.height - 1) / cellSizePixels);

    // Clamp to ensure we don't go out of bounds of the maze grid
    minCol = minmax(minCol, 0, mazeWidthCells - 1);
    maxCol = minmax(maxCol, 0, mazeWidthCells - 1);
    minRow = minmax(minRow, 0, mazeHeightCells - 1);
    maxRow = minmax(maxRow, 0, mazeHeightCells - 1);

    for (int r = minRow; r <= maxRow; ++r) {
        for (int c = minCol; c <= maxCol; ++c) {
            if (r < 0 || r >= mazeHeightCells || c < 0 || c >= mazeWidthCells) continue;
            if (mazeGrid[r][c]) { // check If it's a wall
                Rectangle wallCellRect = { (float)c * cellSizePixels, (float)r * cellSizePixels, (float)cellSizePixels, (float)cellSizePixels };
                if (CheckCollisionRecs(playerRect, wallCellRect)) { 

                    return true; // Collision detected
                }
            }
        }
    }
    return false; // No collision
}

void MazeLevel::Update(float deltaTime) {
    if (levelWon) return; // Don't update if level is already won

    float dx = 0, dy = 0;
    // Handle player movement based on arrow keys
    if (IsKeyDown(KEY_RIGHT)) dx += playerSpeed;
    if (IsKeyDown(KEY_LEFT)) dx -= playerSpeed;
    if (IsKeyDown(KEY_UP)) dy -= playerSpeed;
    if (IsKeyDown(KEY_DOWN)) dy += playerSpeed;

    // Move player if no wall collision
    if (!CheckWallCollision(playerX, playerY, playerSize, dx, 0)) { playerX += dx; }
    if (!CheckWallCollision(playerX, playerY, playerSize, 0, dy)) { playerY += dy; }

    // Keep player within screen bounds
    playerX = minmax(playerX, 0.0f, (float)screenWidth - playerSize);
    playerY = minmax(playerY, 0.0f, (float)screenHeight - playerSize);

    Rectangle playerRect = {playerX, playerY, playerSize, playerSize};
    // Check for coin collection
    for (int i = 0; i < coins.size(); ++i) {
        Rectangle coinRect = { coins[i].x - coinSize / 2, coins[i].y - coinSize / 2, coinSize, coinSize };
        if (CheckCollisionRecs(playerRect, coinRect)) {
            collectedCoins++;
            coins.erase(coins.begin() + i); // Remove collected coin
            i--; // Adjust index after erase
        }
    }

    // Check for level completion (reached exit and collected all coins)
    Rectangle exitRect = { (float)endCol * cellSizePixels, (float)endRow * cellSizePixels, (float)cellSizePixels, (float)cellSizePixels };
    if (CheckCollisionRecs(playerRect, exitRect) && collectedCoins == totalInitialCoins) {
        levelWon = true;
    }
}

void MazeLevel::Draw() {
    // Draw maze grid
    for (int r = 0; r < mazeHeightCells; r++) {
        for (int c = 0; c < mazeWidthCells; c++) {
            // Only draw if it's visible on screen
            if (c * cellSizePixels < screenWidth && r * cellSizePixels < screenHeight) {
                if (mazeGrid[r][c]) {
                    DrawRectangle(c * cellSizePixels, r * cellSizePixels, cellSizePixels, cellSizePixels, MAZE_WALL_COLOR);
                } else {
                    DrawRectangle(c * cellSizePixels, r * cellSizePixels, cellSizePixels, cellSizePixels, MAZE_PATH_COLOR);
                }
            }
        }
    }

    // Draw start and end points
    DrawRectangle(startCol * cellSizePixels, startRow * cellSizePixels, cellSizePixels, cellSizePixels, MAZE_START_COLOR);
    DrawRectangle(endCol * cellSizePixels, endRow * cellSizePixels, cellSizePixels, cellSizePixels, MAZE_END_COLOR);

    // Draw all active coins
    for (const auto& coin : coins) {
        DrawCircle(coin.x, coin.y, coinSize / 2, MAZE_COIN_COLOR);
    }

    // Draw the player (a simple circle with eyes)
    DrawCircle(playerX + playerSize / 2, playerY + playerSize / 2, playerSize / 2, MAZE_PLAYER_COLOR);
    DrawCircle(playerX + playerSize / 2 - playerSize * 0.18f, playerY + playerSize / 2 - playerSize * 0.15f, playerSize * 0.09f, MAZE_PLAYER_EYE_COLOR);
    DrawCircle(playerX + playerSize / 2 + playerSize * 0.18f, playerY + playerSize / 2 - playerSize * 0.15f, playerSize * 0.09f, MAZE_PLAYER_EYE_COLOR);

    // Display coin count
    std::string coinText = "Coins: " + std::to_string(collectedCoins) + "/" + std::to_string(totalInitialCoins);
    DrawText(coinText.c_str(), 10, 10, 20, MAZE_TEXT_COLOR);
}

bool MazeLevel::IsComplete() {
    return levelWon;
}


// Constants for the Space Invaders Level
const int SI_PLAYER_SPEED = 5;            // How fast the player's spaceship moves horizontally.
const int SI_BULLET_SPEED = 3;            // How fast both player and invader bullets travel.
const int SI_INVADER_SPEED = 1;           // The base speed for how much invaders shift horizontally in one step.
const int SI_INVADER_ROWS = 2;            // Number of rows of invaders to spawn.
const int SI_INVADER_COLS = 8;            // Number of columns of invaders to spawn in each row.
const int SI_INVADER_SPACING_X = 50;      // Horizontal distance between the center of invaders.
const int SI_INVADER_SPACING_Y = 40;      // Vertical distance between the center of invader rows.
const int SI_INVADER_START_X = 50;        // X-coordinate where the first invader (top-left of formation) starts.
const int SI_INVADER_START_Y = 100;       // Y-coordinate where the first invader (top-left of formation) starts.
const float SI_INVADER_FIRE_RATE = 0.15f; // The chance (per second) an invader might fire a bullet.
const float SI_INVADER_MOVE_INTERVAL = 0.8f; // How long (in seconds) between each horizontal movement step for the invaders.
const float SI_INVADER_DESCENT_AMOUNT = 20.0f; // How much the invaders drop down when they hit a screen edge and reverse direction.

// Random number generators for invaders
static std::mt19937 s_si_rng(std::chrono::steady_clock::now().time_since_epoch().count());
static std::uniform_real_distribution<float> s_si_dist(0.0f, 1.0f);

// second level: Space Invaders
class SpaceInvadersLevel : public Levels {
public:
    // Represents a bullet in the game
    class Bullet {
    public:
        Rectangle rect;
        bool active;
        bool isPlayerBullet; // Is this a player's bullet or an invader's

        Bullet(Vector2 pos, bool playerBullet) : active(true), isPlayerBullet(playerBullet) {
    // Initialize the bullet's bounding rectangle (position, width, height)
    // 'pos.x' and 'pos.y' come from the Vector2 argument, defining the bullet's starting coordinates.
    // The bullet itself is 5 pixels wide and 10 pixels tall.
    rect = { pos.x, pos.y, 5, 10 };
}
        void Update(float screenHeight) {
            if (active) {
                if (isPlayerBullet) {
                    rect.y -= SI_BULLET_SPEED; // Player bullets go up
                } else {
                    rect.y += SI_BULLET_SPEED; // Invader bullets go down
                }

                // Deactivate bullet if it goes off screen
                if (rect.y < 0 || rect.y > screenHeight) {
                    active = false;
                }
            }
        }

        void Draw() const {
            if (active) {
                DrawRectangleRec(rect, isPlayerBullet ? YELLOW : RED);
            }
        }
    };

    // Represents the player's spaceship
    class Player {
    public:
        Rectangle rect;
        int lives;
        float lastShotTime; // To control firing rate

        Player(int screenW, int screenH) : lives(5), lastShotTime(0.0f) {
            rect = { (float)screenW / 2 - 25, (float)screenH - 70, 50, 50 };
        }

        void Update(std::vector<std::unique_ptr<Bullet>>& playerBullets, float screenW, float currentTime) {
            // Move left/right
            if (IsKeyDown(KEY_LEFT) && rect.x > 0) {
                rect.x -= SI_PLAYER_SPEED;
            }
            if (IsKeyDown(KEY_RIGHT) && rect.x < screenW - rect.width) {
                rect.x += SI_PLAYER_SPEED;
            }
            // Fire bullet if space is pressed and enough time has passed
            if (IsKeyDown(KEY_SPACE) && (currentTime - lastShotTime >= 0.5f)) {
                playerBullets.push_back(std::make_unique<Bullet>(Vector2{ rect.x + rect.width / 2 - 2.5f, rect.y }, true));
                lastShotTime = currentTime;
            }
        }

        void Draw() const {
            // Simple triangle shape for the player
            Vector2 p1 = { rect.x + rect.width / 2, rect.y };
            Vector2 p2 = { rect.x, rect.y + rect.height };
            Vector3 p3_temp = { rect.x + rect.width, rect.y + rect.height, 0.0f };
            DrawTriangle(p1, p2, { p3_temp.x, p3_temp.y }, DARKBLUE);

            // Some details on the player ship
            DrawRectangle(rect.x, rect.y + rect.height * 0.2f, rect.width, rect.height * 0.1f, WHITE);
            DrawRectangle(rect.x, rect.y + rect.height * 0.4f, rect.width, rect.height * 0.1f, BLACK);
            DrawRectangle(rect.x, rect.y + rect.height * 0.6f, rect.width, rect.height * 0.1f, WHITE);
            DrawRectangle(rect.x, rect.y + rect.height * 0.8f, rect.width, rect.height * 0.1f, BLACK);
            DrawRectangle(rect.x + rect.width / 4, rect.y + rect.height / 2, rect.width / 2, rect.height / 2, BLUE);
        }

        void TakeDamage() { lives--; }
        bool IsAlive() const { return lives > 0; }
    };

    // Represents an invader amongus
    class Invader {
    public:
        Rectangle rect;
        bool active;
        int type; // Could be used for different invader behaviors/looks altho we have used a very simpler approach

        Invader(Vector2 pos, int invaderType) : active(true), type(invaderType) {
            rect = { pos.x, pos.y, 30, 30 };
        }

        void Draw() const {
            if (active) {
                // Drawing a simple invader shape
                DrawRectangle(rect.x, rect.y + rect.height * 0.1f, rect.width, rect.height * 0.9f, RED);
                DrawCircle(rect.x + rect.width / 2, rect.y + rect.height * 0.1f, rect.width / 2, RED);
                DrawCircle(rect.x + rect.width / 2, rect.y + rect.height, rect.width / 2, RED);

                DrawRectangle(rect.x + rect.width * 0.2f, rect.y + rect.height * 0.2f, rect.width * 0.6f, rect.height * 0.4f, SKYBLUE);
                DrawRectangleLines(rect.x + rect.width * 0.2f, rect.y + rect.height * 0.2f, rect.width * 0.6f, rect.height * 0.4f, DARKBLUE);

                DrawRectangle(rect.x + rect.width * 0.15f, rect.y - 10, rect.width * 0.7f, 15, BLUE);
                DrawRectangle(rect.x + rect.width * 0.05f, rect.y - 5, rect.width * 0.9f, 5, DARKBLUE);
                DrawRectangle(rect.x + rect.width / 2 - 3, rect.y - 7, 6, 6, YELLOW);
            }
        }

        void FireBullet(std::vector<std::unique_ptr<Bullet>>& invaderBullets) {
            if (active) {
                invaderBullets.push_back(std::make_unique<Bullet>(Vector2{ rect.x + rect.width / 2 - 2.5f, rect.y + rect.height }, false));
            }
        }
    };

    SpaceInvadersLevel(int screenW, int screenH);
    ~SpaceInvadersLevel() override;

    void Load() override;
    void Unload() override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsComplete() override;
    std::string GetName() const override { return "Space Invaders Level"; }
    std::string GetInstructions() const override { return "Use LEFT/RIGHT arrows to move. \n \n Press SPACE to shoot. Destroy all invaders before\n \n  they reach the bottom or \n \n you run out of lives!"; }

    bool DidPlayerWinThisLevel() const { return gameWon; }

private:
    Player player;
    std::vector<std::unique_ptr<Invader>> invaders;
    std::vector<std::unique_ptr<Bullet>> playerBullets;
    std::vector<std::unique_ptr<Bullet>> invaderBullets;
    int score;
    bool gameOver;
    bool gameWon;
    float invaderMoveDirection; // 1.0f for right, -1.0f for left
    float invaderMoveTimer;
    float currentScreenW, currentScreenH;
};

SpaceInvadersLevel::SpaceInvadersLevel(int screenW, int screenH)
    : Levels(screenW, screenH),
      player(screenW, screenH),
      score(0), gameOver(false), gameWon(false),
      invaderMoveDirection(1.0f), invaderMoveTimer(0.0f),
      currentScreenW((float)screenW), currentScreenH((float)screenH)
{
}

SpaceInvadersLevel::~SpaceInvadersLevel() {
    Unload();
}

void SpaceInvadersLevel::Load() {
    player = Player(screenWidth, screenHeight); // Reset player state
    score = 0;
    gameOver = false;
    gameWon = false;
    invaderMoveDirection = 1.0f;
    invaderMoveTimer = 0.0f;

    playerBullets.clear();
    invaderBullets.clear();
    invaders.clear();

    // Spawn invaders in a grid
    for (int row = 0; row < SI_INVADER_ROWS; ++row) {
        for (int col = 0; col < SI_INVADER_COLS; ++col) {
            Vector2 invaderPos = {
                (float)SI_INVADER_START_X + col * SI_INVADER_SPACING_X,
                (float)SI_INVADER_START_Y + row * SI_INVADER_SPACING_Y
            };
            invaders.push_back(std::make_unique<Invader>(invaderPos, row));
        }
    }
}

void SpaceInvadersLevel::Unload() {
    playerBullets.clear();
    invaderBullets.clear();
    invaders.clear();
}

void SpaceInvadersLevel::Update(float deltaTime) {
    if (gameOver || gameWon) {
        return; // Stop updating if game is over or won
    }

    player.Update(playerBullets, currentScreenW, GetTime());

    // Update and clean up player bullets if not active then remove
    for (auto& bullet : playerBullets) { bullet->Update(currentScreenH); }
    playerBullets.erase(std::remove_if(playerBullets.begin(), playerBullets.end(), [](const std::unique_ptr<Bullet>& b) { return !b->active; }), playerBullets.end());

    // Update and clean up invader bullets
    for (auto& bullet : invaderBullets) { bullet->Update(currentScreenH); }
    invaderBullets.erase(std::remove_if(invaderBullets.begin(), invaderBullets.end(), [](const std::unique_ptr<Bullet>& b) { return !b->active; }), invaderBullets.end());

    invaderMoveTimer += GetFrameTime();
    bool shouldDescend = false;
    // Check if it's time for invaders to move horizontally
    if (invaderMoveTimer >= SI_INVADER_MOVE_INTERVAL) {
        invaderMoveTimer = 0.0f;
        float minX = currentScreenW;
        float maxX = 0;
        bool anyInvaderActive = false;
        // Find the leftmost and rightmost active invaders
        for (const auto& invader : invaders) {
            if (invader->active) {
                minX = std::min(minX, invader->rect.x);
                maxX = std::max(maxX, invader->rect.x + invader->rect.width);
                anyInvaderActive = true;
            }
        }

        if (anyInvaderActive) {
            // Reverse direction and descend if invaders hit screen edges
            if (invaderMoveDirection == 1.0f) { // Moving right
                if (maxX >= currentScreenW - 20) {
                    invaderMoveDirection = -1.0f; // Switch to left
                    shouldDescend = true;
                }
            } else { // Moving left
                if (minX <= 20) {
                    invaderMoveDirection = 1.0f; // Switch to right
                    shouldDescend = true;
                }
            }

            // Move invaders
            for (auto& invader : invaders) {
                if (invader->active) {
                    invader->rect.x += invaderMoveDirection * SI_INVADER_SPEED * 10; // Move horizontally
                    if (shouldDescend) {
                        invader->rect.y += SI_INVADER_DESCENT_AMOUNT; // Move down
                        if (invader->rect.y + invader->rect.height >= player.rect.y) {
                            gameOver = true; // Invaders reached player line
                        }
                    }
                }
            }
        }
    }

    // Invaders randomly fire bullets
    for (auto& invader : invaders) {
        if (invader->active && s_si_dist(s_si_rng) < SI_INVADER_FIRE_RATE * GetFrameTime()) {
            //generates a completely random number between 0 and 1 for each active invader every frame and then calculates the probability of firing for the current frame.
            invader->FireBullet(invaderBullets);
        }
    }

    // Player bullet collisions with invaders
    for (auto& pBullet : playerBullets) {
        if (pBullet->active) {
            for (auto& invader : invaders) {
                if (invader->active && CheckCollisionRecs(pBullet->rect, invader->rect)) {
                    pBullet->active = false; // Bullet hits invader
                    invader->active = false; // Invader destroyed
                    score += 100;
                    break;
                }
            }
        }
    }

    // Invader bullet collisions with player
    for (auto& iBullet : invaderBullets) {
        if (iBullet->active) {
            if (CheckCollisionRecs(iBullet->rect, player.rect)) {
                iBullet->active = false; // Bullet hits player
                player.TakeDamage(); // Player loses a life
                if (!player.IsAlive()) {
                    gameOver = true; // No more lives, game over
                }
                break;
            }
        }
    }

    // Check if all invaders are destroyed (win condition)
    bool allInvadersDestroyed = true;
    for (const auto& invader : invaders) {
        if (invader->active) {
            allInvadersDestroyed = false;
            break;
        }
    }
    if (allInvadersDestroyed) {
        gameWon = true;
    }
}

void SpaceInvadersLevel::Draw() {
    player.Draw(); // Draw the player

    // Draw all active invaders and bullets
    for (const auto& invader : invaders) { invader->Draw(); }
    for (const auto& bullet : playerBullets) { bullet->Draw(); }
    for (const auto& bullet : invaderBullets) { bullet->Draw(); }

    // Display score and lives
    DrawText(TextFormat("SCORE: %04i", score), 10, 10, 20, WHITE);
    DrawText(TextFormat("LIVES: %i", player.lives), screenWidth - 100, 10, 20, WHITE);

    // Display game over or level complete messages
    if (gameOver) {
        DrawText("GAME OVER!", screenWidth / 2 - MeasureText("GAME OVER!", 40) / 2, screenHeight / 2 - 20, 40, RED);
    } else if (gameWon) {
        DrawText("LEVEL COMPLETE!", screenWidth / 2 - MeasureText("LEVEL COMPLETE!", 40) / 2, screenHeight / 2 - 20, 40, GOLD);
    }
}

bool SpaceInvadersLevel::IsComplete() {
    return gameOver || gameWon; // Level is complete if won or lost
}

// Constants specific to the Flappy Level
const int FLAPPY_PIPE_WIDTH = 80;                     // The fixed width of each pipe segment in pixels.
const int FLAPPY_PIPE_GAP = 150;                      // The vertical size of the opening/gap between the top and bottom pipes.
const float FLAPPY_PIPE_SPEED = 100.0f;               // How fast pipes move from right to left across the screen (pixels per second).
const float FLAPPY_BIRD_RADIUS = 20.0f;               // The radius of the bird's circular collision and visual model.
const float FLAPPY_BIRD_JUMP_STRENGTH = -250.0f;      // The initial upward vertical velocity applied when the bird 'jumps' (negative because Y increases downwards).
const float FLAPPY_GRAVITY = 700.0f;                  // The constant downward acceleration applied to the bird (pixels per second squared).
const int FLAPPY_WIN_SCORE = 10;                      // The score the player needs to achieve to complete the Flappy Level.
const float FLAPPY_INITIAL_HEALTH = 100.0f;           // The bird's starting health points for the level.
const float FLAPPY_DAMAGE_PER_HIT = 25.0f;            // The amount of health the bird loses upon colliding with a pipe or screen edge.
const float FLAPPY_MIN_HORIZONTAL_PIPE_SPACING = 250.0f; // The minimum horizontal distance maintained between the right edge of one pipe and the left edge of the next.
const int FLAPPY_FONT_SIZE = 40;                      // The size of the font used for displaying score, health, and messages in this level.

// third level: Flappy
class FlappyLevel : public Levels {
public:
    // Internal states for the Flappy level
    enum FlappyGameScreen {
        FLAPPY_MENU = 0,
        FLAPPY_PLAYING,
        FLAPPY_GAME_OVER,
        FLAPPY_WIN
    };

    // Base class for game objects in Flappy (Bird, Pipe)
    class GameObject {
    public:
        virtual void Update(float deltaTime) = 0;
        virtual void Draw() = 0;
        virtual ~GameObject() = default;
    };

    // The bird character
    class Bird : public GameObject {
    private:
        Vector2 m_position;
        float m_velocityY;
        Color m_color;
        float m_radius;
        float m_health;
        int m_screenW, m_screenH;

    public:
        Bird(int screenW, int screenH)
            : m_position({(float)screenW / 4, (float)screenH / 2}), // Start in the middle-left
              m_velocityY(0.0f),
              m_color(PURPLE),
              m_radius(FLAPPY_BIRD_RADIUS),
              m_health(FLAPPY_INITIAL_HEALTH),
              m_screenW(screenW), m_screenH(screenH)
        {
        }

        Vector2 getPosition() const { return m_position; }
        float getRadius() const { return m_radius; }
        float getHealth() const { return m_health; }

        void setPosition(Vector2 pos) { m_position = pos; }
        void setVelocityY(float velocity) { m_velocityY = velocity; }
        void setHealth(float health) { m_health = health; }

        void Jump() {
            m_velocityY = FLAPPY_BIRD_JUMP_STRENGTH; // Apply upward velocity
        }

        void takeDamage(float amount) {
            m_health -= amount;
            if (m_health < 0) {
                m_health = 0;
            }
        }

        void Update(float deltaTime) override {
            m_velocityY += FLAPPY_GRAVITY * deltaTime; // Apply gravity
            m_position.y += m_velocityY * deltaTime;    // Update vertical position

            // Keep bird within vertical bounds of the screen
            if (m_position.y < m_radius * 1.5f) {
                m_position.y = m_radius * 1.5f;
                m_velocityY = 0;
            }
            if (m_position.y > m_screenH - m_radius * 1.5f) {
                m_position.y = m_screenH - m_radius * 1.5f;
                m_velocityY = 0;
            }
        }

        void Draw() override {
            // Draw a slightly-squashed rounded rectangle for the body
            float bodyWidth = m_radius * 2.0f;
            float bodyHeight = m_radius * 2.5f;
            float legHeight = m_radius * 0.8f;
            float legWidth = m_radius * 0.7f;
            float visorWidth = m_radius * 1.2f;
            float visorHeight = m_radius * 0.8f;

            Rectangle bodyRect = {
                m_position.x - bodyWidth / 2,
                m_position.y - bodyHeight / 2,
                bodyWidth,
                bodyHeight
            };
            DrawRectangleRounded(bodyRect, 0.5f, 8, m_color);

            // Draw legs
            Rectangle leftLegRect = {
                m_position.x - bodyWidth / 2 + m_radius * 0.2f,
                m_position.y + bodyHeight / 2 - legHeight,
                legWidth,
                legHeight
            };
            DrawRectangleRounded(leftLegRect, 0.5f, 8, m_color);

            Rectangle rightLegRect = {
                m_position.x + bodyWidth / 2 - legWidth - m_radius * 0.2f,
                m_position.y + bodyHeight / 2 - legHeight,
                legWidth,
                legHeight
            };
            DrawRectangleRounded(rightLegRect, 0.5f, 8, m_color);

            // Draw a visor/eye
            DrawEllipse(
                (int)m_position.x,
                (int)(m_position.y - bodyHeight / 2 + visorHeight / 2 + m_radius * 0.3f),
                (int)(visorWidth / 2),
                (int)(visorHeight / 2),
                SKYBLUE
            );
            DrawEllipseLines(
                (int)m_position.x,
                (int)(m_position.y - bodyHeight / 2 + visorHeight / 2 + m_radius * 0.3f),
                (int)(visorWidth / 2),
                (int)(visorHeight / 2),
                DARKBLUE
            );
        }
    };

    // The pipes that the bird needs to avoid
    class Pipe : public GameObject {
    private:
        Rectangle m_topRect;
        Rectangle m_bottomRect;
        bool m_scored; // Has the player scored by passing this pipe?
        int m_screenH;

    public:
        Pipe(float startX, float gapY, int screenH) : m_scored(false), m_screenH(screenH) {
            // Calculate dimensions for top and bottom pipes based on gapY
            m_topRect = {startX, 0, (float)FLAPPY_PIPE_WIDTH, gapY - FLAPPY_PIPE_GAP / 2};
            m_bottomRect = {startX, gapY + FLAPPY_PIPE_GAP / 2, (float)FLAPPY_PIPE_WIDTH, (float)m_screenH - (gapY + FLAPPY_PIPE_GAP / 2)};
        }

        Rectangle getTopRect() const { return m_topRect; }
        Rectangle getBottomRect() const { return m_bottomRect; }
        bool isScored() const { return m_scored; }
        void markScored() { m_scored = true; }

        void Update(float deltaTime) override {
            // Pipes move from right to left
            m_topRect.x -= FLAPPY_PIPE_SPEED * deltaTime;
            m_bottomRect.x -= FLAPPY_PIPE_SPEED * deltaTime;
        }

        void Draw() override {
            DrawRectangleRec(m_topRect, GREEN);
            DrawRectangleRec(m_bottomRect, GREEN);
            DrawRectangleLinesEx(m_topRect, 2, DARKGREEN);
            DrawRectangleLinesEx(m_bottomRect, 2, DARKBROWN);
        }
    };

    FlappyLevel(int screenW, int screenH);
    ~FlappyLevel() override;

    void Load() override;
    void Unload() override;
    void Update(float deltaTime) override;
    void Draw() override;
    bool IsComplete() override;
    std::string GetName() const override { return "Flappy Level"; }
    std::string GetInstructions() const override { return "Press SPACE to make your character flap.\n \n Avoid hitting the pipes and the ground. \n \nGet a score of 10 to win."; }

    bool DidPlayerWinThisLevel() const { return m_playerWonLevel; }

private:
    std::unique_ptr<Bird> m_bird;
    std::vector<std::unique_ptr<Pipe>> m_pipes;
    int m_score;
    FlappyGameScreen m_currentScreen; // Current state of this level

    std::random_device m_rd;
    std::mt19937 m_gen;
    std::uniform_int_distribution<> m_distrib;

    bool m_levelFinished; // True when this specific level is done
    bool m_playerWonLevel; // True if player won this level

    void GenerateNewPipe();
    void InitFlappyGame(); // Sets up a new Flappy game instance
};

// Static random engine for FlappyLevel (for consistent random numbers)
static std::random_device s_flappy_rd;
static std::mt19937 s_flappy_gen(s_flappy_rd());
static std::uniform_int_distribution<> s_flappy_distrib(FLAPPY_PIPE_GAP, GLOBAL_SCREEN_HEIGHT - FLAPPY_PIPE_GAP);

FlappyLevel::FlappyLevel(int screenW, int screenH)
    : Levels(screenW, screenH),
      m_score(0),
      m_currentScreen(FLAPPY_MENU),
      m_gen(s_flappy_gen),
      m_distrib(s_flappy_distrib),
      m_levelFinished(false),
      m_playerWonLevel(false)
{
}

FlappyLevel::~FlappyLevel() {
    Unload();
}

void FlappyLevel::InitFlappyGame() {
    m_bird = std::make_unique<Bird>(screenWidth, screenHeight);
    m_pipes.clear();
    m_score = 0;
    m_levelFinished = false;
    m_playerWonLevel = false;
    // Add initial pipes, spaced out from the start
    m_pipes.push_back(std::make_unique<Pipe>((float)screenWidth, (float)m_distrib(m_gen), screenHeight));
    m_pipes.push_back(std::make_unique<Pipe>((float)screenWidth + FLAPPY_MIN_HORIZONTAL_PIPE_SPACING, (float)m_distrib(m_gen), screenHeight));
    m_currentScreen = FLAPPY_MENU; // Start at the menu for this level
}

void FlappyLevel::Load() {
    InitFlappyGame(); // Reset and set up the game
}

void FlappyLevel::Unload() {
    m_pipes.clear(); // Clears all unique pointers, deallocating pipes
}

void FlappyLevel::GenerateNewPipe() {
    float gapY = (float)m_distrib(m_gen); // Random Y position for the gap
    // Determine X position for the new pipe
    float newPipeX = m_pipes.empty() ? (float)screenWidth : m_pipes.back()->getTopRect().x + FLAPPY_MIN_HORIZONTAL_PIPE_SPACING;
    m_pipes.push_back(std::make_unique<Pipe>(newPipeX, gapY, screenHeight));
}

void FlappyLevel::Update(float deltaTime) {
    switch (m_currentScreen) {
        case FLAPPY_MENU: {
            if (IsKeyPressed(KEY_SPACE)) { // Start game on spacebar press
                m_currentScreen = FLAPPY_PLAYING;
            }
        } break;
        case FLAPPY_PLAYING: {
            m_bird->Update(deltaTime);

            bool collisionOccurred = false;
            for (size_t i = 0; i < m_pipes.size(); ++i) {
                m_pipes[i]->Update(deltaTime);

                // Check for collision with top or bottom pipe
                if (CheckCollisionCircleRec(m_bird->getPosition(), m_bird->getRadius(), m_pipes[i]->getTopRect()) ||
                    CheckCollisionCircleRec(m_bird->getPosition(), m_bird->getRadius(), m_pipes[i]->getBottomRect())) {
                    collisionOccurred = true;
                }

                // Score if bird passed the pipe and hasn't scored yet for this pipe
                if (!m_pipes[i]->isScored() && m_pipes[i]->getTopRect().x + FLAPPY_PIPE_WIDTH < m_bird->getPosition().x - m_bird->getRadius()) {
                    m_score++;
                    m_pipes[i]->markScored();
                }
            }

            // Check for collision with top/bottom screen edges
            if (m_bird->getPosition().y + m_bird->getRadius() >= screenHeight || m_bird->getPosition().y - m_bird->getRadius() <= 0) {
                 collisionOccurred = true;
            }

            // Handle collisions
            if (collisionOccurred) {
                m_bird->takeDamage(FLAPPY_DAMAGE_PER_HIT); // Take damage
                if (m_bird->getHealth() <= 0) {
                    m_currentScreen = FLAPPY_GAME_OVER; // Game over if no health left
                    m_levelFinished = true;
                    m_playerWonLevel = false;
                } else {
                    // Reset bird position and clear pipes for a "retry"
                    m_bird->setPosition({(float)screenWidth / 4, (float)screenHeight / 2});
                    m_bird->setVelocityY(0.0f);
                    m_pipes.clear();
                    m_pipes.push_back(std::make_unique<Pipe>((float)screenWidth, (float)m_distrib(m_gen), screenHeight));
                    m_pipes.push_back(std::make_unique<Pipe>((float)screenWidth + FLAPPY_MIN_HORIZONTAL_PIPE_SPACING, (float)m_distrib(m_gen), screenHeight));
                }
            }

            // Remove off-screen pipes
            if (!m_pipes.empty() && m_pipes[0]->getTopRect().x + FLAPPY_PIPE_WIDTH < 0) {
                m_pipes.erase(m_pipes.begin());
            }

            // Generate new pipes if needed
            if (m_pipes.back()->getTopRect().x < screenWidth - FLAPPY_MIN_HORIZONTAL_PIPE_SPACING) {
                GenerateNewPipe();
            }

            // Check for win condition
            if (m_score >= FLAPPY_WIN_SCORE) {
                m_currentScreen = FLAPPY_WIN;
                m_levelFinished = true;
                m_playerWonLevel = true;
            }

            if (IsKeyPressed(KEY_SPACE)) { // Player jumps on spacebar press
                m_bird->Jump();
            }
        } break;
        default: break;
    }
}

void FlappyLevel::Draw() {
    // Draw all active pipes
    for (const auto& pipe : m_pipes) {
        pipe->Draw();
    }

    m_bird->Draw(); // Draw the bird

    // Display score
    DrawText(TextFormat("Score: %02i", m_score), 10, 10, FLAPPY_FONT_SIZE, WHITE);

    // Draw health bar
    int healthBarX = screenWidth - 10 - 100;
    int healthBarY = 10;
    int healthBarWidth = 100;
    int healthBarHeight = 20;

    DrawRectangle(healthBarX, healthBarY, healthBarWidth, healthBarHeight, DARKGRAY);
    DrawRectangle(healthBarX, healthBarY, (int)(m_bird->getHealth() / FLAPPY_INITIAL_HEALTH * healthBarWidth), healthBarHeight, RED);
    DrawRectangleLines(healthBarX, healthBarY, healthBarWidth, healthBarHeight, WHITE);
    DrawText(TextFormat("Health: %.0f", m_bird->getHealth()), healthBarX, healthBarY + healthBarHeight + 5, 20, WHITE);

    // Draw ground
    DrawRectangle(0, screenHeight - 20, screenWidth, 20, BROWN);
    DrawRectangleLines(0, screenHeight - 20, screenWidth, 20, DARKBROWN);

    // Draw different screens based on current level state
    switch (m_currentScreen) {
        case FLAPPY_MENU: {
            DrawText("FLAPPY", screenWidth / 2 - MeasureText("FLAPPY", FLAPPY_FONT_SIZE * 1.5f) / 2, screenHeight / 4, FLAPPY_FONT_SIZE * 1.5f, WHITE);
            DrawText("Press SPACE to Start", screenWidth / 2 - MeasureText("Press SPACE to Start", FLAPPY_FONT_SIZE) / 2, screenHeight / 2, FLAPPY_FONT_SIZE, GRAY);
        } break;
        case FLAPPY_GAME_OVER: {
            DrawText("GAME OVER!", screenWidth / 2 - MeasureText("GAME OVER!", FLAPPY_FONT_SIZE * 1.5f) / 2, screenHeight / 4, FLAPPY_FONT_SIZE * 1.5f, RED);
            DrawText(TextFormat("Final Score: %02i", m_score), screenWidth / 2 - MeasureText(TextFormat("Final Score: %02i", m_score), FLAPPY_FONT_SIZE) / 2, screenHeight / 2 - FLAPPY_FONT_SIZE / 2, FLAPPY_FONT_SIZE, WHITE);
        } break;
        case FLAPPY_WIN: {
            DrawText("LEVEL COMPLETE!", screenWidth / 2 - MeasureText("LEVEL COMPLETE!", FLAPPY_FONT_SIZE * 1.5f) / 2, screenHeight / 4, FLAPPY_FONT_SIZE * 1.5f, GOLD);
            DrawText(TextFormat("Final Score: %02i", m_score), screenWidth / 2 - MeasureText(TextFormat("Final Score: %02i", m_score), FLAPPY_FONT_SIZE) / 2, screenHeight / 2 - FLAPPY_FONT_SIZE / 2, FLAPPY_FONT_SIZE, WHITE);
        } break;
        default: break;
    }
}

bool FlappyLevel::IsComplete() {
    return m_levelFinished; // Level is complete when the internal state says so
}


// Constants specific to the Obstacle Level
const float OBSTACLE_PLAYER_SIZE = 40.0f;
const float OBSTACLE_PLAYER_SPEED = 200.0f; 
const float OBSTACLE_JUMP_FORCE = 400.0f;
const float OBSTACLE_GRAVITY = 800.0f;


// Our fourth level: the Obstacle Course!
class ObstacleLevel : public Levels {
public:
    // Internal screens for the Obstacle level
    enum ObstacleGameScreen {
        OBSTACLE_TITLE = 0, // Title screen for this specific level (not used much now)
        OBSTACLE_GAMEPLAY,  // Where the actual running and jumping happens
        OBSTACLE_ENDING     // Win/loss screen for this level
    };

    // Base class for game objects in Obstacle Level
    class GameObject {
    protected:
        Vector2 position;
        Rectangle bounds;
        Color color;

    public:
        GameObject() : position({0, 0}), bounds({0, 0, 0, 0}), color(WHITE) {}
        GameObject(Vector2 pos, float width, float height, Color col)
            : position(pos), bounds({pos.x, pos.y, width, height}), color(col) {}
        GameObject(const GameObject& other)
            : position(other.position), bounds(other.bounds), color(other.color) {}

        virtual ~GameObject() = default;

        virtual void Draw() const = 0;
        virtual void Update(float dt) {} // Default: static objects don't update

        Rectangle GetBounds() const { return bounds; }
        Vector2 GetPosition() const { return position; }
        void SetPosition(Vector2 newPos) {
            position = newPos;
            bounds.x = newPos.x;
            bounds.y = newPos.y;
        }
    };

    // The player character for the Obstacle level
    class Player : public GameObject {
    private:
        Vector2 velocity;
        bool onGround;
        bool jumped;
        int m_screenW, m_screenH;

    public:
        Player(int screenW, int screenH)
            : GameObject({100.0f, (float)screenH - OBSTACLE_PLAYER_SIZE - 50.0f}, OBSTACLE_PLAYER_SIZE, OBSTACLE_PLAYER_SIZE, PURPLE),
              velocity({0, 0}), onGround(false), jumped(false), m_screenW(screenW), m_screenH(screenH) {}

        Player(Vector2 pos, float size, Color col, int screenW, int screenH)
            : GameObject(pos, size, size, col), velocity({0, 0}), onGround(false), jumped(false), m_screenW(screenW), m_screenH(screenH) {}

        Player(const Player& other)
            : GameObject(other), velocity(other.velocity), onGround(other.onGround), jumped(other.jumped), m_screenW(other.m_screenW), m_screenH(other.m_screenH) {}

        ~Player() override = default;

        void Draw() const override {
            DrawRectangleRounded(bounds, 0.5f, 8, color); // Main body
            // Little decorative bits for the player
            DrawRectangle(bounds.x - bounds.width * 0.2f, bounds.y + bounds.height * 0.1f, bounds.width * 0.2f, bounds.height * 0.6f, ColorAlpha(color, 0.8f));
            Rectangle visor = {bounds.x + bounds.width * 0.2f, bounds.y + bounds.height * 0.2f, bounds.width * 0.6f, bounds.height * 0.3f};
            DrawRectangleRounded(visor, 0.5f, 8, SKYBLUE);
            DrawRectangleRoundedLines(visor, 0.5f, 8, 2, DARKBLUE);
        }

        void Update(float dt) override {
            velocity.y += OBSTACLE_GRAVITY * dt; // Apply gravity

            // Handle horizontal movement
            if (IsKeyDown(KEY_LEFT)) {
                velocity.x = -OBSTACLE_PLAYER_SPEED;
            } else if (IsKeyDown(KEY_RIGHT)) {
                velocity.x = OBSTACLE_PLAYER_SPEED;
            } else {
                velocity.x = 0;
            }

            // Handle jumping
            if (IsKeyPressed(KEY_SPACE) && onGround) {
                velocity.y = -OBSTACLE_JUMP_FORCE; // Instant upward force
                onGround = false;
                jumped = true;
            }

            position.x += velocity.x * dt; // Update horizontal position
            position.y += velocity.y * dt; // Update vertical position

            bounds.x = position.x;
            bounds.y = position.y;

            // Keep player within horizontal screen bounds
            if (position.x < 0) {
                position.x = 0;
                velocity.x = 0;
            } else if (position.x + bounds.width > m_screenW) {
                position.x = m_screenW - bounds.width;
                velocity.x = 0;
            }
            onGround = false; // Assume off ground until collision detects otherwise
        }

        Vector2 GetVelocity() const { return velocity; }
        bool IsOnGround() const { return onGround; }
        bool HasJumped() const { return jumped; }

        void SetVelocity(Vector2 newVel) { velocity = newVel; }
        void SetOnGround(bool status) { onGround = status; }
        void SetJumped(bool status) { jumped = status; }

        // Comparison for player state (useful for debugging/testing)
        bool operator==(const Player& other) const {
            return (position.x == other.position.x && position.y == other.position.y);
        }
    };

    // Rectangular obstacles that block the player
    class Obstacle : public GameObject {
    public:
        Obstacle() : GameObject() {}
        Obstacle(Rectangle rect, Color col = BLUE) : GameObject({rect.x, rect.y}, rect.width, rect.height, col) {}
        Obstacle(const Obstacle& other) : GameObject(other) {}
        ~Obstacle() override = default;

        void Draw() const override {
            DrawRectangleRec(bounds, color);
        }
        void Update(float dt) override {}
    };

    // Collectible coins!
    class Coin : public GameObject {
    public:
        Coin() : GameObject() {}
        Coin(Vector2 pos, float size, Color col = YELLOW) : GameObject(pos, size, size, col) {}
        Coin(const Coin& other) : GameObject(other) {}
        ~Coin() override = default;

        void Draw() const override {
            // Draw a circle with a dollar sign on it
            DrawCircle((int)(bounds.x + bounds.width / 2), (int)(bounds.y + bounds.height / 2), bounds.width / 2, color);
            DrawCircleLines((int)(bounds.x + bounds.width / 2), (int)(bounds.y + bounds.height / 2), bounds.width / 2, DARKGRAY);
            const char* dollarSign = "$";
            int fontSize = (int)(bounds.width * 0.6f);
            int textWidth = MeasureText(dollarSign, fontSize);
            DrawText(dollarSign, (int)(bounds.x + bounds.width / 2 - textWidth / 2), (int)(bounds.y + bounds.height / 2 - fontSize / 2), fontSize, BROWN);
        }
        void Update(float dt) override {}
    };

    // The exit door to complete the level
    class ExitDoor : public GameObject {
    public:
        ExitDoor() : GameObject() {}
        ExitDoor(Rectangle rect, Color col) : GameObject({rect.x, rect.y}, rect.width, rect.height, col) {}
        ExitDoor(const ExitDoor& other) : GameObject(other) {}
        ~ExitDoor() override = default;

        void Draw() const override {
            // Draw a rectangular door with panels
            DrawRectangleRec(bounds, color);
            DrawRectangleLinesEx(bounds, 3, BLACK);
            Rectangle panel1 = {bounds.x + bounds.width * 0.1f, bounds.y + bounds.height * 0.1f, bounds.width * 0.8f, bounds.height * 0.4f};
            Rectangle panel2 = {bounds.x + bounds.width * 0.1f, bounds.y + bounds.height * 0.55f, bounds.width * 0.8f, bounds.height * 0.35f};
            DrawRectangleRec(panel1, DARKBROWN);
            DrawRectangleRec(panel2, DARKBROWN);
            DrawRectangleLinesEx(panel1, 2, BLACK);
            DrawRectangleLinesEx(panel2, 2, BLACK);
            DrawText("EXIT", (int)bounds.x + 5, (int)bounds.y - 20, 15, WHITE);
        }
        void Update(float dt) override {}
    };

    ObstacleLevel(int screenW, int screenH);
    ~ObstacleLevel() override;

    void Load() override;
    void Unload() override;
    void Update(float dt) override;
    void Draw() override;
    bool IsComplete() override;
    std::string GetName() const override { return "Obstacle Course Level"; }
    std::string GetInstructions() const override { return "Use LEFT/RIGHT arrows to move. \n \n Press SPACE to jump. \n \n Collect all coins and reach the EXIT door to win!"; }

    bool DidPlayerWinThisLevel() const { return m_playerWonLevel; }

private:
    Player m_player;
    ExitDoor m_exitDoor;
    std::vector<std::unique_ptr<GameObject>> m_obstacles; // Store various obstacles
    std::vector<std::unique_ptr<Coin>> m_coins;
    ObstacleGameScreen m_currentScreen;
    bool m_levelFinished;
    bool m_playerWonLevel;
    Vector2 m_startPoint; // Player's starting position
    int m_collectedCoins;
    int m_totalCoins;

    void InitObstacleGame(); // Setup for a new game in this level
};

ObstacleLevel::ObstacleLevel(int screenW, int screenH)
    : Levels(screenW, screenH),
      m_player(screenW, screenH),
      m_exitDoor(),
      m_currentScreen(OBSTACLE_TITLE),
      m_levelFinished(false),
      m_playerWonLevel(false),
      m_startPoint({0,0}),
      m_collectedCoins(0),
      m_totalCoins(0)
{
}

ObstacleLevel::~ObstacleLevel() {
    Unload();
}

void ObstacleLevel::InitObstacleGame() {
    // Reset player and set starting point
    m_player = Player({100.0f, (float)screenHeight - OBSTACLE_PLAYER_SIZE - 50.0f}, OBSTACLE_PLAYER_SIZE, PURPLE, screenWidth, screenHeight);
    m_startPoint = m_player.GetPosition();

    // Set the exit door's position
    m_exitDoor = ExitDoor({ (float)screenWidth - 100.0f, 50.0f, 50.0f, 80.0f }, BROWN);

    m_obstacles.clear();
    m_coins.clear();

    // Add ground obstacle (bottom of the screen)
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 0, (float)screenHeight - 50, (float)screenWidth, 50 }));

    // Add various platforms as obstacles
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 150, (float)screenHeight - 150, 100, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 300, (float)screenHeight - 250, 90, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 450, (float)screenHeight - 350, 80, 20 }));

    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 550, (float)screenHeight - 300, 60, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 700, (float)screenHeight - 300, 60, 20 }));

    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 800, (float)screenHeight - 400, 50, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 900, (float)screenHeight - 500, 50, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 800, (float)screenHeight - 600, 50, 20 }));

    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 950, (float)screenHeight - 550, 60, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 1100, (float)screenHeight - 450, 70, 20 }));

    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 1150, (float)screenHeight - 300, 50, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ (float)screenWidth - 150, (float)screenHeight - 100, 100, 20 }));
    m_obstacles.push_back(std::make_unique<Obstacle>(Rectangle{ 1000, (float)screenHeight - 200, 80, 20 }));

    // Spawn coins above platforms
    float coinSize = 20.0f;
    for (const auto& obs_ptr : m_obstacles) {
        if (obs_ptr->GetBounds().height < 50 && obs_ptr->GetBounds().y < screenHeight - 50) { // Only add coins to platforms
            Vector2 coinPos = {
                obs_ptr->GetBounds().x + obs_ptr->GetBounds().width / 2 - coinSize / 2,
                obs_ptr->GetBounds().y - coinSize - 10
            };
            m_coins.push_back(std::make_unique<Coin>(coinPos, coinSize));
        }
    }
    m_totalCoins = m_coins.size(); // Keep track of total coins
    m_collectedCoins = 0;

    m_currentScreen = OBSTACLE_GAMEPLAY; // Start directly in gameplay for this level
    m_levelFinished = false;
    m_playerWonLevel = false;
}

void ObstacleLevel::Load() {
    InitObstacleGame(); // Prepare the level for play
}

void ObstacleLevel::Unload() {
    m_obstacles.clear(); // Clear all unique pointers
    m_coins.clear();
}

void ObstacleLevel::Update(float dt) {
    switch (m_currentScreen) {
        case OBSTACLE_GAMEPLAY: {
            m_player.Update(dt); // Update player physics and input

            m_player.SetOnGround(false); // Assume airborne until collision with ground/platform

            // Handle player-obstacle collisions
            for (const auto& obs_ptr : m_obstacles) {
                if (CheckCollisionRecs(m_player.GetBounds(), obs_ptr->GetBounds())) {
                    // Check for collision from above (landing on platform)
                    if (m_player.GetVelocity().y > 0 && m_player.GetBounds().y + m_player.GetBounds().height - m_player.GetVelocity().y * dt <= obs_ptr->GetBounds().y) {
                        m_player.SetPosition({m_player.GetPosition().x, obs_ptr->GetBounds().y - m_player.GetBounds().height});
                        m_player.SetVelocity({m_player.GetVelocity().x, 0}); // Stop vertical movement
                        m_player.SetOnGround(true);
                        m_player.SetJumped(false);
                    }
                    // Check for collision from below (hitting head on platform)
                    else if (m_player.GetVelocity().y < 0 && m_player.GetBounds().y - m_player.GetBounds().y * dt >= obs_ptr->GetBounds().y + obs_ptr->GetBounds().height) {
                        m_player.SetPosition({m_player.GetPosition().x, obs_ptr->GetBounds().y + obs_ptr->GetBounds().height});
                        m_player.SetVelocity({m_player.GetVelocity().x, 0}); // Stop upward movement
                    }
                    // Check for collision from left (hitting side of platform)
                    else if (m_player.GetVelocity().x > 0 && m_player.GetBounds().x + m_player.GetBounds().width - m_player.GetVelocity().x * dt <= obs_ptr->GetBounds().x) {
                        m_player.SetPosition({obs_ptr->GetBounds().x - m_player.GetBounds().width, m_player.GetPosition().y});
                        m_player.SetVelocity({0, m_player.GetVelocity().y}); // Stop horizontal movement
                    }
                    // Check for collision from right (hitting side of platform)
                    else if (m_player.GetVelocity().x < 0 && m_player.GetBounds().x - m_player.GetVelocity().x * dt >= obs_ptr->GetBounds().x + obs_ptr->GetBounds().width) {
                        m_player.SetPosition({obs_ptr->GetBounds().x + obs_ptr->GetBounds().width, m_player.GetPosition().y});
                        m_player.SetVelocity({0, m_player.GetVelocity().y}); // Stop horizontal movement
                    }
                }
            }

            // Check for coin collection
            for (int i = 0; i < m_coins.size(); ) {
                if (CheckCollisionRecs(m_player.GetBounds(), m_coins[i]->GetBounds())) {
                    m_collectedCoins++;
                    m_coins.erase(m_coins.begin() + i); // Remove collected coin
                } else {
                    i++; // Move to next coin
                }
            }

            // Check if player fell off the screen (game over)
            if (m_player.GetPosition().y > screenHeight) {
                m_levelFinished = true;
                m_playerWonLevel = false;
                m_currentScreen = OBSTACLE_ENDING;
            }

            // Check for level completion (reached exit door and collected all coins)
            if (CheckCollisionRecs(m_player.GetBounds(), m_exitDoor.GetBounds())) {
                if (m_collectedCoins == m_totalCoins) {
                    m_playerWonLevel = true;
                    m_levelFinished = true;
                    m_currentScreen = OBSTACLE_ENDING;
                }
            }
        } break;
        case OBSTACLE_ENDING: {
            // Nothing to update, waiting for main game state to change
        } break;
        default: break;
    }
}

void ObstacleLevel::Draw() {
    switch (m_currentScreen) {
        case OBSTACLE_GAMEPLAY: {
            // Draw all obstacles and coins
            for (const auto& obs_ptr : m_obstacles) {
                obs_ptr->Draw();
            }
            for (const auto& coin_ptr : m_coins) {
                coin_ptr->Draw();
            }
            m_player.Draw(); // Draw the player
            m_exitDoor.Draw(); // Draw the exit door

            // Draw start point indicator
            DrawCircle((int)m_startPoint.x + (int)OBSTACLE_PLAYER_SIZE / 2, (int)m_startPoint.y + (int)OBSTACLE_PLAYER_SIZE / 2, 10, GREEN);
            DrawText("START", (int)m_startPoint.x, (int)m_startPoint.y - 20, 15, GREEN);
            // Display coin count
            DrawText(TextFormat("Coins: %d/%d", m_collectedCoins, m_totalCoins), 10, 10, 20, WHITE);
        } break;
        case OBSTACLE_ENDING: {
            // Display win or lose message
            if (m_playerWonLevel) {
                DrawText("LEVEL COMPLETE!", screenWidth / 2 - MeasureText("LEVEL COMPLETE!", 50) / 2, screenHeight / 3, 50, GOLD);
                DrawText(TextFormat("Collected: %d/%d Coins", m_collectedCoins, m_totalCoins), screenWidth / 2 - MeasureText(TextFormat("Collected: %d/%d Coins", m_collectedCoins, m_totalCoins), 30) / 2, screenHeight / 3 + 60, 30, WHITE);
            } else {
                DrawText("GAME OVER!", screenWidth / 2 - MeasureText("GAME OVER!", 60) / 2, screenHeight / 3, 60, RED);
            }
        } break;
        default: break;
    }
}

bool ObstacleLevel::IsComplete() {
    return m_levelFinished; // Level is complete when the internal state says so
}


// Enum for the overall game screens/states
enum GameScreen {
    TITLE_SCREEN_GLOBAL,         // The very first screen of the game
    PLAYING_LEVEL,               // A level from our queue is active
    LEVEL_TRANSITION,            // Screen between levels
    GAME_OVER_GLOBAL,            // Player lost the whole game
    GAME_WON_GLOBAL              // Player completed all levels
};

GameScreen currentGlobalScreen = TITLE_SCREEN_GLOBAL; // Start here!
std::queue<std::unique_ptr<Levels>> gameLevels; // The order of levels to play
std::unique_ptr<Levels> currentActiveLevel = nullptr; // The level we are currently playing

std::string nextLevelName = "";
std::string nextLevelInstructions = "";
Rectangle confirmButton = { (float)GLOBAL_SCREEN_WIDTH / 2 - 100, (float)GLOBAL_SCREEN_HEIGHT * 0.75f, 200, 50 };

// Forward declarations for our global UI drawing functions
void DrawGlobalTitleScreen();
void DrawLevelTransitionScreen();
void DrawGlobalGameOverScreen();
void DrawGlobalGameWonScreen();
void SetupGameLevels(); // Prepares the sequence of levels
void LoadNextLevel(); // Loads the next level from the queue

// Function to draw the new starting screen
void DrawStartingScreen() {
    ClearBackground(BLACK); // Black background for the start screen

    // Text inviting the player to "escape" or "suffer"
    const char* descriptionText = "You are in prison for kidnapping a qurbani ka bakra,\n \n \n \n \n \n \n \n       you should";
    int fontSize = 30;
    int textWidth = MeasureText(descriptionText, fontSize);
    DrawText(descriptionText, GLOBAL_SCREEN_WIDTH / 2 - textWidth / 2, GLOBAL_SCREEN_HEIGHT / 2 - 150, fontSize, WHITE);

    // "Escape" Button
    Rectangle escapeButtonRec = { (float)GLOBAL_SCREEN_WIDTH / 2 - 150, (float)GLOBAL_SCREEN_HEIGHT / 2 + 50, 300, 70 };
    DrawRectangleRec(escapeButtonRec, GREEN);
    DrawText("ESCAPE", (int)(escapeButtonRec.x + escapeButtonRec.width / 2 - MeasureText("ESCAPE", 40) / 2), (int)(escapeButtonRec.y + escapeButtonRec.height / 2 - 20), 40, BLACK);

    // "Suffer" Button
    Rectangle sufferButtonRec = { (float)GLOBAL_SCREEN_WIDTH / 2 - 150, (float)GLOBAL_SCREEN_HEIGHT / 2 + 150, 300, 70 };
    DrawRectangleRec(sufferButtonRec, RED);
    DrawText("SUFFER", (int)(sufferButtonRec.x + sufferButtonRec.width / 2 - MeasureText("SUFFER", 40) / 2), (int)(sufferButtonRec.y + sufferButtonRec.height / 2 - 20), 40, BLACK);

    // Check for button clicks
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePoint = GetMousePosition();

        if (CheckCollisionPointRec(mousePoint, escapeButtonRec)) {
            showSufferMessage = false; // Hide message if it was showing
            SetupGameLevels(); // Populate the queue with levels
            LoadNextLevel();   // Load the first level into memory
            currentGlobalScreen = PLAYING_LEVEL; // Change state to main game
            std::cout << "Escape button pressed! Changing to PLAYING_LEVEL." << std::endl; // Debug output
        } else if (CheckCollisionPointRec(mousePoint, sufferButtonRec)) {
            showSufferMessage = true;
            sufferMessageTimer = 0.0f; // Reset timer for the message
            std::cout << "Suffer button pressed! Displaying message." << std::endl; // Debug output
        }
    }

    // Update and draw "Suffer" message if active
    if (showSufferMessage) {
        sufferMessageTimer += GetFrameTime();
        if (sufferMessageTimer < SUFFER_MESSAGE_DISPLAY_TIME) {
            DrawText("NO LOSER YOU NEED TO ESCAPE", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("NO LOSER YOU NEED TO ESCAPE", 30) / 2, GLOBAL_SCREEN_HEIGHT / 2 + 280, 30, YELLOW);
        } else {
            showSufferMessage = false; // Hide message after its time
        }
    }
}


// Main game loop and state management
int main() {
    InitWindow(GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT, ""); // Initialize the game window
    SetTargetFPS(60); // Aim for 60 frames per second

    while (!WindowShouldClose()) { // Loop while the window is open
        float deltaTime = GetFrameTime(); // Time since last frame

        // Update logic based on the current overall game screen
        switch (currentGlobalScreen) {
            case TITLE_SCREEN_GLOBAL:
                // All title screen updates are handled within DrawStartingScreen() now.
                break;
            case PLAYING_LEVEL:
                if (currentActiveLevel) {
                    currentActiveLevel->Update(deltaTime); // Update the current level
                    if (currentActiveLevel->IsComplete()) { // Check if the level is finished
                        bool levelSucceeded = true;
                        // Special checks for specific level types to see if player won or lost
                        if (currentActiveLevel->GetName() == "Space Invaders Level") {
                            SpaceInvadersLevel* siLevel = static_cast<SpaceInvadersLevel*>(currentActiveLevel.get());
                            if (!siLevel->DidPlayerWinThisLevel()) {
                                levelSucceeded = false;
                            }
                        } else if (currentActiveLevel->GetName() == "Flappy Level") {
                            FlappyLevel* flappyLevel = static_cast<FlappyLevel*>(currentActiveLevel.get());
                            if (!flappyLevel->DidPlayerWinThisLevel()) {
                                levelSucceeded = false;
                            }
                        } else if (currentActiveLevel->GetName() == "Obstacle Course Level") {
                            ObstacleLevel* obstacleLevel = static_cast<ObstacleLevel*>(currentActiveLevel.get());
                            if (!obstacleLevel->DidPlayerWinThisLevel()) {
                                levelSucceeded = false;
                            }
                        }
                        // MazeLevel's IsComplete() means a win for that level

                        currentActiveLevel->Unload(); // Clean up current level's resources

                        if (levelSucceeded) {
                            if (!gameLevels.empty()) {
                                // Prepare data for the transition screen to the next level
                                nextLevelName = gameLevels.front()->GetName();
                                nextLevelInstructions = gameLevels.front()->GetInstructions();
                                currentGlobalScreen = LEVEL_TRANSITION; // Go to the transition screen
                            } else {
                                currentActiveLevel = nullptr; // No more levels left
                                currentGlobalScreen = GAME_WON_GLOBAL; // Player completed all levels!
                            }
                        } else {
                            currentActiveLevel = nullptr; // Level failed
                            currentGlobalScreen = GAME_OVER_GLOBAL; // Game Over for the whole game
                        }
                    }
                } else {
                    currentGlobalScreen = GAME_OVER_GLOBAL; // Fallback to game over if somehow no active level
                }
                break;

            case LEVEL_TRANSITION: {
                // Wait for the player to click "Ready!"
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (CheckCollisionPointRec(GetMousePosition(), confirmButton)) {
                        LoadNextLevel(); // Load the next level
                        if (currentActiveLevel) {
                            currentGlobalScreen = PLAYING_LEVEL; // Start playing the new level
                        } else {
                            currentGlobalScreen = GAME_WON_GLOBAL; // Should mean all levels are done
                        }
                    }
                }
            } break;

            case GAME_OVER_GLOBAL:
                if (IsKeyPressed(KEY_ENTER)) { // Press Enter to go back to title
                    currentGlobalScreen = TITLE_SCREEN_GLOBAL;
                    showSufferMessage = false; // Reset title screen messages
                    sufferMessageTimer = 0.0f;
                }
                break;

            case GAME_WON_GLOBAL:
                if (IsKeyPressed(KEY_ENTER)) { // Press Enter to go back to title
                    currentGlobalScreen = TITLE_SCREEN_GLOBAL;
                    showSufferMessage = false; // Reset title screen messages
                    sufferMessageTimer = 0.0f;
                }
                break;
        }

        BeginDrawing(); // Start drawing for this frame
        ClearBackground(BLACK); // Clear screen to black

        // Draw based on the current overall game screen
        if (currentGlobalScreen == PLAYING_LEVEL && currentActiveLevel) {
            currentActiveLevel->Draw(); // Draw the current active game level
        } else if (currentGlobalScreen == TITLE_SCREEN_GLOBAL) {
            DrawStartingScreen(); // Draw the initial game start screen
        } else if (currentGlobalScreen == LEVEL_TRANSITION) {
            DrawLevelTransitionScreen(); // Draw the screen between levels
        } else if (currentGlobalScreen == GAME_OVER_GLOBAL) {
            DrawGlobalGameOverScreen(); // Draw the game over screen
        } else if (currentGlobalScreen == GAME_WON_GLOBAL) {
            DrawGlobalGameWonScreen(); // Draw the game won screen
        }

        EndDrawing(); // End drawing for this frame
    }

    // Clean up resources before closing the window
    if (currentActiveLevel) {
        currentActiveLevel->Unload();
    }

    CloseWindow(); // Close the Raylib window
    return 0;
}

// This function is kept for forward declaration consistency but its content is now in DrawStartingScreen().
void DrawGlobalTitleScreen() {
}

// Draws the screen shown between levels
void DrawLevelTransitionScreen() {
    DrawRectangle(0, 0, GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT, Fade(BLACK, 0.8f)); // Dark overlay

    // Display next level's name and instructions
    const char* titleText = "Next Level: ";
    int titleFontSize = 40;
    int titleTextWidth = MeasureText(titleText, titleFontSize);
    int nameTextWidth = MeasureText(nextLevelName.c_str(), titleFontSize);
    DrawText(titleText, GLOBAL_SCREEN_WIDTH / 2 - (titleTextWidth + nameTextWidth) / 2, GLOBAL_SCREEN_HEIGHT / 4, titleFontSize, RAYWHITE);
    DrawText(nextLevelName.c_str(), GLOBAL_SCREEN_WIDTH / 2 - (titleTextWidth + nameTextWidth) / 2 + titleTextWidth, GLOBAL_SCREEN_HEIGHT / 4, titleFontSize, GOLD);

    DrawText("How to Play:", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("How to Play:", 30) / 2, GLOBAL_SCREEN_HEIGHT / 2 - 50, 30, RAYWHITE);
    DrawText(nextLevelInstructions.c_str(), GLOBAL_SCREEN_WIDTH / 2 - MeasureText(nextLevelInstructions.c_str(), 25) / 2, GLOBAL_SCREEN_HEIGHT / 2, 25, LIGHTGRAY);

    // "Ready!" button to proceed
    DrawRectangleRec(confirmButton, DARKGREEN);
    DrawRectangleLinesEx(confirmButton, 3, GREEN);
    const char* buttonText = "Ready!";
    int buttonTextWidth = MeasureText(buttonText, 30);
    DrawText(buttonText, (int)(confirmButton.x + confirmButton.width / 2 - buttonTextWidth / 2), (int)(confirmButton.y + confirmButton.height / 2 - 15), 30, RAYWHITE);
}

// Draws the screen when the player loses the entire game
void DrawGlobalGameOverScreen() {
    DrawText("loser you got caught.", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("GAME OVER!", 60) / 2, GLOBAL_SCREEN_HEIGHT / 2 - 50, 60, RED);
    DrawText("Press ENTER to bribe & Try Again", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Try Again", 30) / 2, GLOBAL_SCREEN_HEIGHT / 2 + 20, 30, WHITE);
}

// Draws the screen when the player wins the entire game
void DrawGlobalGameWonScreen() {
    DrawText("CONGRATULATIONS!", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("CONGRATULATIONS!", 50) / 2, GLOBAL_SCREEN_HEIGHT / 2 - 80, 50, GOLD);
    DrawText("You Escaped prison!", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("You Escaped ALL Levels!", 40) / 2, GLOBAL_SCREEN_HEIGHT / 2 - 20, 40, LIME);
    DrawText("Press ENTER to kidnap a bakra again!", GLOBAL_SCREEN_WIDTH / 2 - MeasureText("Press ENTER to Play Again", 30) / 2, GLOBAL_SCREEN_HEIGHT / 2 + 50, 30, WHITE);
}

// Sets up the predefined order of levels for the game
void SetupGameLevels() {
    // Clear any previous levels
    while (!gameLevels.empty()) {
        gameLevels.pop();
    }

    // Add levels in sequence
    gameLevels.push(std::make_unique<MazeLevel>(GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT));
    gameLevels.push(std::make_unique<SpaceInvadersLevel>(GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT));
    gameLevels.push(std::make_unique<FlappyLevel>(GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT));
    gameLevels.push(std::make_unique<ObstacleLevel>(GLOBAL_SCREEN_WIDTH, GLOBAL_SCREEN_HEIGHT));
}

// Helper function to load the next level from our queue
void LoadNextLevel() {
    // Unload the current level if one is active
    if (currentActiveLevel) {
        currentActiveLevel->Unload();
        currentActiveLevel = nullptr; // Ensure proper deallocation
    }
    
    // If there are more levels in the queue, load the next one
    if (!gameLevels.empty()) {
        currentActiveLevel = std::move(gameLevels.front()); // Take ownership of the next level
        gameLevels.pop(); // Remove it from the queue
        currentActiveLevel->Load(); // Initialize the new level
        std::cout << "Loading Level: " << currentActiveLevel->GetName() << std::endl; // Debug output
        std::cout << "Instructions: " << currentActiveLevel->GetInstructions() << std::endl; // Debug output
    } else {
        currentActiveLevel = nullptr; // No more levels left
    }
}
