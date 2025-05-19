#include "unPhone.h"
#include <Adafruit_HX8357.h>

unPhone u = unPhone("maze_ball");

enum GameLevel
{
  EASY,
  INTERMEDIATE
};
GameLevel currentLevel = EASY;

float x_pos = 20;
float y_pos = 20;
float velocity_x = 0;
float velocity_y = 0;
const float accel_factor = 0.2;
const float friction = 0.99;
int prev_x = -1, prev_y = -1;
bool mazeDrawn = false;
bool hasVibrated = false;

const int ball_radius = 6;
bool gameWon = false;

struct Wall
{
  int x, y, w, h;
};

// // --- EASY LEVEL ---
Wall walls_easy[] = {
    {0, 0, 320, 10}, {0, 0, 10, 480}, {0, 470, 320, 10}, {310, 0, 10, 480}, {10, 80, 230, 10}, {80, 140, 280, 10}, {10, 200, 230, 10}, {80, 260, 280, 10}, {10, 320, 230, 10}, {80, 380, 280, 10}};
const int numWalls_easy = sizeof(walls_easy) / sizeof(walls_easy[0]);
int goal_x_easy = 250, goal_y_easy = 410, goal_w_easy = 40, goal_h_easy = 40;

// INTERMEDIATE LEVEL (matches user sketch)
Wall walls_intermediate[] = {
    // Borders
    {0, 0, 320, 10},
    {0, 0, 10, 480},
    {0, 470, 320, 10},
    {310, 0, 10, 480},

    // Horizontal walls
    {10, 60, 230, 10},
    {130, 120, 250, 10},
    {10, 180, 220, 10},
    {1300, 240, 250, 10},
    {10, 300, 190, 10},
    {60, 420, 300, 10},

    // Vertical walls (key segments from drawing)
    {60, 60, 10, 60},

    {60, 180, 10, 60},
    {200, 300, 10, 60},
    {60, 360, 10, 60}};
const int numWalls_intermediate = sizeof(walls_intermediate) / sizeof(walls_intermediate[0]);
int goal_x_intermediate = 20, goal_y_intermediate = 420, goal_w_intermediate = 30, goal_h_intermediate = 30;

// === MAZE DRAW FUNCTION ===
void drawMaze(Adafruit_HX8357 *tft)
{
  Wall *currentWalls;
  int wallCount;
  int gx, gy, gw, gh;

  if (currentLevel == EASY)
  {
    currentWalls = walls_easy;
    wallCount = numWalls_easy;
    gx = goal_x_easy;
    gy = goal_y_easy;
    gw = goal_w_easy;
    gh = goal_h_easy;
  }
  else
  {
    currentWalls = walls_intermediate;
    wallCount = numWalls_intermediate;
    gx = goal_x_intermediate;
    gy = goal_y_intermediate;
    gw = goal_w_intermediate;
    gh = goal_h_intermediate;
  }

  for (int i = 0; i < wallCount; i++)
  {
    tft->fillRect(currentWalls[i].x, currentWalls[i].y, currentWalls[i].w, currentWalls[i].h, 0x07E0);
  }
  tft->fillRect(gx, gy, gw, gh, 0xF800);
}

bool collides(float x, float y)
{
  Wall *currentWalls;
  int wallCount;

  if (currentLevel == EASY)
  {
    currentWalls = walls_easy;
    wallCount = numWalls_easy;
  }
  else
  {
    currentWalls = walls_intermediate;
    wallCount = numWalls_intermediate;
  }

  for (int i = 0; i < wallCount; i++)
  {
    if (x + ball_radius > currentWalls[i].x && x - ball_radius < currentWalls[i].x + currentWalls[i].w &&
        y + ball_radius > currentWalls[i].y && y - ball_radius < currentWalls[i].y + currentWalls[i].h)
    {
      return true;
    }
  }
  return false;
}

bool checkGoal(float x, float y)
{
  int gx, gy, gw, gh;
  if (currentLevel == EASY)
  {
    gx = goal_x_easy;
    gy = goal_y_easy;
    gw = goal_w_easy;
    gh = goal_h_easy;
  }
  else
  {
    gx = goal_x_intermediate;
    gy = goal_y_intermediate;
    gw = goal_w_intermediate;
    gh = goal_h_intermediate;
  }
  return (x > gx && x < gx + gw && y > gy && y < gy + gh);
}

void setup()
{
  Serial.begin(115200);
  u.begin();
  u.backlight(true);
  u.vibe(false);

  if (u.accelp == nullptr || u.tftp == nullptr)
  {
    Serial.println("Missing components.");
    return;
  }

  u.tftp->fillScreen(0x0000);
  u.tftp->setTextColor(0xFFFF);
  u.tftp->setTextSize(2);
  u.tftp->setCursor(60, 100);
  u.tftp->print("Maze Ball Start!");
  delay(2000);
}

void loop()
{
  if (gameWon)
    return;

  sensors_event_t event;
  if (u.accelp != nullptr)
  {
    u.getAccelEvent(&event);
    if (!isnan(event.acceleration.x) && !isnan(event.acceleration.y))
    {
      velocity_x -= event.acceleration.x * accel_factor;
      velocity_y -= -event.acceleration.y * accel_factor;
    }
  }

  velocity_x *= friction;
  velocity_y *= friction;

  float maxSpeed = 3.5;
  if (velocity_x > maxSpeed)
    velocity_x = maxSpeed;
  if (velocity_x < -maxSpeed)
    velocity_x = -maxSpeed;
  if (velocity_y > maxSpeed)
    velocity_y = maxSpeed;
  if (velocity_y < -maxSpeed)
    velocity_y = -maxSpeed;

  float new_x = x_pos + velocity_x;
  float new_y = y_pos + velocity_y;

  bool hitWall = false;
  int steps = max(abs(velocity_x), abs(velocity_y));
  if (steps < 1)
    steps = 1;

  float dx = velocity_x / steps;
  float dy = velocity_y / steps;

  for (int i = 0; i < steps; i++)
  {
    float trial_x = x_pos + dx;
    float trial_y = y_pos + dy;

    if (!collides(trial_x, y_pos))
    {
      x_pos = trial_x;
    }
    else
    {
      velocity_x = 0;
      hitWall = true;
    }

    if (!collides(x_pos, trial_y))
    {
      y_pos = trial_y;
    }
    else
    {
      velocity_y = 0;
      hitWall = true;
    }

    if (hitWall && !hasVibrated)
    {
      u.vibe(true);
      delay(30);
      u.vibe(false);
      hasVibrated = true;
      break;
    }
  }

  if (!hitWall)
  {
    hasVibrated = false;
  }

  if (checkGoal(x_pos, y_pos))
  {
    u.tftp->fillScreen(0x0000);
    u.tftp->setTextColor(0x07E0);
    u.tftp->setTextSize(3);

    if (currentLevel == EASY)
    {
      u.tftp->setCursor(90, 100);
      u.tftp->print("YOU WIN!!!");
      delay(1500);
      u.tftp->fillScreen(0x0000);
      u.tftp->setCursor(40, 120);
      u.tftp->setTextColor(0xFFFF);
      u.tftp->print("Intermediate");
      u.tftp->setCursor(90, 160);
      u.tftp->setTextColor(0xFFFF);
      u.tftp->print("Level");
      delay(2000);

      currentLevel = INTERMEDIATE;
      x_pos = 20;
      y_pos = 20;
      velocity_x = 0;
      velocity_y = 0;
      u.tftp->fillScreen(0x0000);          // clear screen
      drawMaze((Adafruit_HX8357 *)u.tftp); // draw new maze
      return;
    }

    // Final level win
    u.tftp->setCursor(90, 100);
    u.tftp->print("YOU WIN!!!");
    u.vibe(true);
    delay(1000);
    u.vibe(false);
    gameWon = true;
    return;
  }

  Adafruit_HX8357 *tft = (Adafruit_HX8357 *)u.tftp;

  if (!mazeDrawn)
  {
    tft->fillScreen(0x0000);
    drawMaze(tft);
    mazeDrawn = true;
  }

  // Erase previous ball
  static int prev_x = -1, prev_y = -1;
  if (prev_x >= 0 && prev_y >= 0)
  {
    tft->fillCircle(prev_x, prev_y, ball_radius, 0x0000); // black background
  }

  // Draw ball
  tft->fillCircle((int)x_pos, (int)y_pos, ball_radius, 0xFFFF);
  prev_x = (int)x_pos;
  prev_y = (int)y_pos;

  delay(10);
}
