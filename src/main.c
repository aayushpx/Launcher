#include <stdio.h>
#include <stdlib.h>
#include <esp_timer.h>
#include "fonts.h"
#include "graphics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#ifdef TTGO_S3
#define RIGHT_BUTTON 14
#else
#define RIGHT_BUTTON 35
#endif

#define LEFT_BUTTON 0

void input_output_init();  // initialises gpio 0, 14, 35 as inputs 
int storage_read_int(char *name, int def);  // reads highscore from NVS
void storage_write_int(char *name, int val);  // writes highscore to NVS

// 56x36 px player
#define PLAYER_W 56
#define PLAYER_H 36
#define PLAYER_SPEED 300

// 10x31 px bullet
#define BLOCK_W 10
#define BLOCK_H 31
#define MAX_BLOCKS 20

// Spawn 1.5 -> 0.3 seconds
#define INITIAL_SPAWN_MS 1500
#define MIN_SPAWN_MS 300

// Fall speed 120 -> 600 px/seconds
#define INITIAL_FALL_SPEED 120
#define MAX_FALL_SPEED 600

// Score
#define SCORE_PER_MISS 100

// Matrix rain constants
#define MATRIX_RAIN_COLS 40
#define MATRIX_RAIN_SPEED_MIN 30
#define MATRIX_RAIN_SPEED_MAX 80
#define MATRIX_RAIN_LENGTH_MIN 8
#define MATRIX_RAIN_LENGTH_MAX 20

extern image_header neo_main_image;
extern image_header bullet_main_image;

typedef struct {
  float x, y;  // position
  int w, h;  // dimensions
  float xvel, yvel;  // velocity
  uint16_t colour;  // RGB565 colour
} obj;

static float last_rain_time = 0;  // time between rain frames

static float getdt() {
  /*
   * Returns seconds since last call
   */
  static uint64_t last_time = 0;
  float dt;
  uint64_t current_time = esp_timer_get_time();
  if (last_time == 0) dt = 0;
  else dt = (current_time - last_time) / 1000000.0f;
  if (dt > 0.05f) dt = 0.05f;
  last_time = current_time;
  return dt;
}

static int overlap(obj *r1, obj *r2) {
  /*
   * Collision detection. Returns 1 if overlapping
   */
  if (r1->x < r2->x + r2->w && r1->x + r1->w > r2->x
    && r1->y < r2->y + r2->h && r1->y + r1->h > r2->y)
    return 1;
  return 0;
}

typedef struct {
  int col;  // column
  int y;  // offset
  int speed;  // fall speed
  int length;  // trail length
  int chars[MATRIX_RAIN_LENGTH_MAX];  // char codes
} matrix_rain_t;

static float draw_matrix_rain(matrix_rain_t *rain, int bright) {
  /*
   * Renders matrix rain
   */
  float dt;
  uint64_t time = esp_timer_get_time();
  dt = (time - last_rain_time) / 1000000.0f;  // dt from last rain time
  last_rain_time = time;

  for (int i = 0; i < MATRIX_RAIN_COLS; i++) {  // 40 column loop
    matrix_rain_t *r = &rain[i];  // current column

    for (int j = 0; j < r->length; j++) {  // loop through chars
      int draw_y = r->y + j * 12;  // screen position (y)
      if (draw_y >= 0 && draw_y < display_height) {  // only draw if on screen

        // Colour gradient 
        if (j == r->length - 1) {
          setFontColour(0, bright, bright / 3);  // Brightest green
        } else if (j >= r->length - 3) {
          setFontColour(0, bright * 2 / 3, bright / 5);
        } else if (j >= r->length - 6) {
          setFontColour(0, bright / 2, bright / 8);
        } else {
          setFontColour(0, bright / 4, bright / 16);  // Dim
        }

        char c[2] = {r->chars[j], '\0'};
        print_xy(c, r->col, draw_y);  // print single char
      }
    }

    r->y += (int)(r->speed * dt * 10);  // advance column downward

    // Reset column when off screen
    if (r->y > display_height + r->length * 12) {
      r->y = -(rand() % display_height);
      r->speed = rand() % (MATRIX_RAIN_SPEED_MAX - MATRIX_RAIN_SPEED_MIN) + MATRIX_RAIN_SPEED_MIN;
      r->length = rand() % (MATRIX_RAIN_LENGTH_MAX - MATRIX_RAIN_LENGTH_MIN) + MATRIX_RAIN_LENGTH_MIN;
      for (int j = 0; j < r->length; j++) {
        r->chars[j] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()"[rand() % 46];
      }
    }
  }
  return dt;
}

static void init_matrix_rain(matrix_rain_t *rain) {
  last_rain_time = esp_timer_get_time();
  const char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()";  // char set
  for (int i = 0; i < MATRIX_RAIN_COLS; i++) {  // loop 40 columns
    
    // Position
    rain[i].col = (display_width / MATRIX_RAIN_COLS) * i + (display_width / MATRIX_RAIN_COLS / 2);
    rain[i].y = -(rand() % display_height);

    // Random fall speed
    rain[i].speed = rand() % (MATRIX_RAIN_SPEED_MAX - MATRIX_RAIN_SPEED_MIN) + MATRIX_RAIN_SPEED_MIN;
    rain[i].length = rand() % (MATRIX_RAIN_LENGTH_MAX - MATRIX_RAIN_LENGTH_MIN) + MATRIX_RAIN_LENGTH_MIN;

    // Fill char arr with random symbols
    for (int j = 0; j < rain[i].length; j++) {
      rain[i].chars[j] = symbols[rand() % (sizeof(symbols) - 1)];
    }
  }
}

static void draw_title_screen(matrix_rain_t *rain) {
  cls(rgbToColour(0, 10, 0));
  draw_matrix_rain(rain, 255);

  setFont(FONT_DEJAVU18);
  setFontColour(0, 255, 70);
  print_xy("THE MATRIX", CENTER, 30);

  setFont(FONT_DEJAVU18);
  setFontColour(0, 200, 50);
  print_xy("SYSTEM FAILURE", CENTER, 65);

  setFont(FONT_UBUNTU16);
  setFontColour(0, 150, 40);
  print_xy("Dodge the bullets.", CENTER, 110);
  print_xy("Survive.", CENTER, 130);

  setFontColour(0, 100, 30);
  print_xy("LEFT:  Move Left", CENTER, 170);
  print_xy("RIGHT: Move Right", CENTER, 190);

  setFont(FONT_SMALL);
  setFontColour(0, 255, 70);
  print_xy("PRESS LEFT TO ENTER", CENTER, 260);

  flip_frame();
}

static void draw_game_over(int score, int high_score) {
  cls(rgbToColour(50, 10, 10));

  setFont(FONT_DEJAVU18);
  setFontColour(255, 100, 100);
  print_xy("SYSTEM CRASH", CENTER, 60);

  setFont(FONT_DEJAVU18);
  setFontColour(255, 255, 255);
  char buf[32];
  sprintf(buf, "Score: %d", score);
  print_xy(buf, CENTER, 110);

  sprintf(buf, "High Score: %d", high_score);
  setFontColour(200, 200, 200);
  print_xy(buf, CENTER, 140);

  setFont(FONT_UBUNTU16);
  setFontColour(100, 200, 100);
  print_xy("GET READY", CENTER, 200); 
  print_xy("FOR", CENTER, 220);
  print_xy("THE NEXT BATTLE", CENTER, 240);

  flip_frame();
}

static void spawn_block(obj *block, int fall_speed) {
  /*
   * Takes pointer to a single object-block and current fall speed.
   */

  // Block dimensions
  block->w = BLOCK_W; 
  block->h = BLOCK_H;
  block->x = rand() % (display_width - BLOCK_W); // random x-position, display_width - 10
  block->y = -BLOCK_H;  // start y-position, -31
  block->yvel = fall_speed; // 120 -> 600
  block->xvel = 0;  // unused, blocks fall straight down
  block->colour = rgbToColour(220, 50, 50); //  red
}

static void falling_blocks_game() {
  // Allocate 20 blocks + 40 rain columns
  obj *blocks = malloc(sizeof(obj) * MAX_BLOCKS);
  if (!blocks) return;

  matrix_rain_t *matrix_rain = malloc(sizeof(matrix_rain_t) * MATRIX_RAIN_COLS);
  if (!matrix_rain) {
    free(blocks);  // free what was allocated
    return;
  }

  set_orientation(PORTRAIT);
  init_matrix_rain(matrix_rain); // rain columns
  int high_score = storage_read_int("highscore", 0); // read NVS

  obj player;
  player.x = (display_width - PLAYER_W) / 2;  // center x
  player.y = (display_height - 73) - 2;  // 73: player height
  player.w = PLAYER_W;
  player.h = PLAYER_H;
  player.xvel = 0;
  player.yvel = 0;
  player.colour = rgbToColour(0, 255, 100);  // green player

  // Block arr initialisation
  for (int i = 0; i < MAX_BLOCKS; i++) {
    blocks[i].w = 0;
    blocks[i].h = 0;
    blocks[i].yvel = 0;
  }

  // Game state variables
  int score = 0;
  int spawn_interval_ms = INITIAL_SPAWN_MS;
  int fall_speed = INITIAL_FALL_SPEED;
  uint64_t last_spawn_time = esp_timer_get_time() / 1000;
  uint64_t difficulty_timer = esp_timer_get_time();

  setFont(FONT_UBUNTU16);
  setFontColour(255, 255, 255); // white

  // Title screen
  do {
    draw_title_screen(matrix_rain);
  } while (gpio_get_level(LEFT_BUTTON));

  while (!gpio_get_level(LEFT_BUTTON)) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  while (1) {
    float dt = getdt(); // delta time

    cls(rgbToColour(5, 20, 5));

    if (!gpio_get_level(LEFT_BUTTON)) {  // 0 = pressed
      player.x -= PLAYER_SPEED * dt;  // 300 px/second
      if (player.x < 0) player.x = 0;  // left boundary
    }
    if (!gpio_get_level(RIGHT_BUTTON)) {
      player.x += PLAYER_SPEED * dt;
      if (player.x > display_width - player.w) player.x = display_width - player.w; // right boundary
    }

    // Spawn new block
    uint64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - last_spawn_time >= (uint64_t)spawn_interval_ms) {
      for (int i = 0; i < MAX_BLOCKS; i++) {
        if (blocks[i].w == 0) {  // final empty index
          spawn_block(&blocks[i], fall_speed);
          break;
        }
      }
      last_spawn_time = now_ms;
    }

    // Difficulty increases every 10 seconds
    uint64_t now = esp_timer_get_time();
    if (now - difficulty_timer >= 10000000) {
      difficulty_timer = now;
      if (spawn_interval_ms > MIN_SPAWN_MS) {
        spawn_interval_ms -= 100;
      }
      if (fall_speed < MAX_FALL_SPEED) {
        fall_speed += 50;
        for (int i = 0; i < MAX_BLOCKS; i++) {
          if (blocks[i].w > 0) {
            blocks[i].yvel = fall_speed;  // update active blocks
          }
        }
      }
    }

    // Update blocks off-screen
    for (int i = 0; i < MAX_BLOCKS; i++) {
      if (blocks[i].w > 0) {
        blocks[i].y += blocks[i].yvel * dt;  // move down
        if (blocks[i].y > display_height) {  // past bottom
          blocks[i].w = 0;  // deactivate
          blocks[i].h = 0;
          score += SCORE_PER_MISS;  // +100 score
          if (score > high_score) {
            high_score = score;
            storage_write_int("highscore", high_score);
          }
        }
      }
    }

    // Collision Detection
    for (int i = 0; i < MAX_BLOCKS; i++) {
      if (blocks[i].w > 0 && overlap(&player, &blocks[i])) {
        draw_game_over(score, high_score);
        vTaskDelay(pdMS_TO_TICKS(3000)); // 3 sec delay
        free(matrix_rain);
        free(blocks);
        return;  // exit to app_main
      }
    }
    draw_image(&neo_main_image, player.x + PLAYER_W / 2, player.y + PLAYER_H / 2 + 7);

    for (int i = 0; i < MAX_BLOCKS; i++) {
      if (blocks[i].w > 0) {
        draw_image(&bullet_main_image, blocks[i].x + BLOCK_W / 2, blocks[i].y + BLOCK_H / 2);
      }
    }

    // Current score
    char buf[32];
    sprintf(buf, "Score: %d", score);
    print_xy(buf, 5, 5);
    sprintf(buf, "Hi: %d", high_score);
    print_xy(buf, display_width - 70, 5);

    flip_frame();
  }
}

void app_main() {
  input_output_init();
  graphics_init();
  cls(0);

  while (1) {
    falling_blocks_game();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
