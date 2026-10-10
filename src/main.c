#include <stdio.h>
#include <stdlib.h>
#include <esp_timer.h>
#include "fonts.h"
#include "graphics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "leds.h"
#include "esp_err.h"
#include "event_log.h"
#include "mpu6050.h"
#include <stdbool.h>
#include "networking.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#ifdef TTGO_S3
#define RIGHT_BUTTON 14
#else
#define RIGHT_BUTTON 35
#endif

#define LEFT_BUTTON 0

#define ENCODER_CLK 43
#define ENCODER_DT 18
#define ENCODER_SW 17

#define SCREEN_COUNT 4

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
  for (int i = 0; i < MATRIX_RAIN_COLS; i++) {  // 40 column loop

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

typedef enum {
  MISSION_SAFE,  // 0
  MISSION_ARMED,  // 1
  MISSION_COUNTDOWN,  // 2
  MISSION_FLIGHT,  // 3
  MISSION_LANDED,   // 4
  MISSION_ABORTED  // 5
} mission_state_t;



// static void draw_mission_screen(mission_state_t state, const char *timer_text) {
//   set_orientation(LANDSCAPE);
//   setFont(FONT_UBUNTU16);
//   setFontColour(255, 255, 255);
//
//   cls(rgbToColour(5, 10, 20));
//
//   print_xy("MISSION CONTROL", 10, 10);
//   print_xy("----------------", 10, 35);
//
//   switch (state) {
//     case MISSION_SAFE:
//       setFontColour(180, 180, 180);
//       print_xy("SAFE", 10, 65);
//       break;
//
//     case MISSION_ARMED:
//       setFontColour(0, 255, 100);
//       print_xy("ARMED", 10, 65);
//       break;
//
//     case MISSION_COUNTDOWN:
//       setFontColour(255, 180, 0);
//       print_xy("COUNTDOWN", 10, 65);
//       break;
//
//     case MISSION_FLIGHT:
//       setFontColour(0, 200, 255);
//       print_xy("FLIGHT", 10, 65);
//       break;
//
//     case MISSION_LANDED:
//       setFontColour(0, 255, 100);
//       print_xy("LANDED", 10, 65);
//       break;
//
//     case MISSION_ABORTED:
//       setFontColour(255, 40, 40);
//       print_xy("ABORTED", 10, 65);
//       break;
//   }
//
//   if (timer_text[0] != '\0') {
//     setFontColour(255, 255, 255);
//     print_xy(timer_text, 10, 100);
//   }
//
//   flip_frame();
// }


static void draw_mission_screen(mission_state_t state, const char *timer_text)
{
  const uint16_t bg       = rgbToColour(5, 10, 20);
  const uint16_t panel    = rgbToColour(14, 28, 45);
  const uint16_t cyan     = rgbToColour(50, 210, 255);
  const uint16_t white    = rgbToColour(235, 245, 255);
  const uint16_t muted    = rgbToColour(115, 145, 170);
  const uint16_t green    = rgbToColour(50, 255, 130);
  const uint16_t amber    = rgbToColour(255, 180, 30);
  const uint16_t red      = rgbToColour(255, 55, 65);

  const char *state_text = "UNKNOWN";
  uint16_t state_colour = white;

  switch (state) {
    case MISSION_SAFE:
      state_text = "SAFE";
      state_colour = green;
      break;

    case MISSION_ARMED:
      state_text = "ARMED";
      state_colour = green;
      break;

    case MISSION_COUNTDOWN:
      state_text = "COUNTDOWN";
      state_colour = amber;
      break;

    case MISSION_FLIGHT:
      state_text = "IN FLIGHT";
      state_colour = cyan;
      break;

    case MISSION_LANDED:
      state_text = "LANDED";
      state_colour = green;
      break;

    case MISSION_ABORTED:
      state_text = "ABORTED";
      state_colour = red;
      break;
  }

  set_orientation(LANDSCAPE);
  cls(bg);

  /* Header */
  draw_rectangle(0, 0, 320, 28, panel);
  draw_rectangle(0, 27, 320, 2, cyan);

  setFont(FONT_UBUNTU16);
  setFontColour(50, 210, 255);
  print_xy("MISSION CONTROL", 8, 5);

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("SYSTEM 01", 238, 9);

  /* Main status panel */
  draw_rectangle(8, 37, 304, 62, panel);
  draw_rectangle(8, 37, 4, 62, state_colour);

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("MISSION STATUS", 20, 42);

  setFont(FONT_UBUNTU16);
  setFontColour(
      (state_colour >> 11) * 255 / 31,
      ((state_colour >> 5) & 0x3F) * 255 / 63,
      (state_colour & 0x1F) * 255 / 31
      );
  print_xy((char *)state_text, 20, 60);

  /* Timer panel */
  draw_rectangle(8, 105, 304, 32, rgbToColour(9, 19, 32));

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("MISSION TIMER", 16, 113);

  setFont(FONT_UBUNTU16);
  setFontColour(235, 245, 255);

  if (timer_text != NULL && timer_text[0] != '\0') {
    print_xy((char *)timer_text, 180, 111);
  } else {
    print_xy("--:--", 220, 111);
  }

  /* Controls footer */
  draw_line(8, 143, 312, 143, rgbToColour(35, 60, 80));

  setFont(FONT_SMALL);

  setFontColour(50, 255, 130);
  print_xy("ARM", 16, 149);

  setFontColour(255, 180, 30);
  print_xy("LAUNCH", 112, 149);

  setFontColour(255, 55, 65);
  print_xy("ABORT", 240, 149);

  flip_frame();
}


static void draw_page_header(const char *title, int page)
{
  set_orientation(LANDSCAPE);
  cls(rgbToColour(5, 10, 20));

  draw_rectangle(0, 0, 320, 28, rgbToColour(14, 28, 45));
  draw_rectangle(0, 27, 320, 2, rgbToColour(50, 210, 255));

  setFont(FONT_UBUNTU16);
  setFontColour(50, 210, 255);
  print_xy((char *)title, 8, 5);

  char page_text[16];
  snprintf(page_text, sizeof(page_text), "%d / 4", page + 1);

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy(page_text, 275, 9);
}


static void draw_telemetry_screen(
    const mpu6050_reading_t *reading, bool sensor_ok)
{
  draw_page_header("SENSOR TELEMETRY", 1);
  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("INERTIAL MEASUREMENT UNIT", 12, 40);

  setFont(FONT_UBUNTU16);
  setFontColour(50, 210, 255);
  print_xy("MPU6050", 12, 55);

  if (!sensor_ok) {
    setFont(FONT_SMALL);
    setFontColour(255, 80, 80);
    print_xy("SENSOR INITIALISATION FAILED", 12, 85);
    setFontColour(170, 190, 210);
    print_xy("Check wiring and serial log", 12, 108);
    flip_frame();
    return;
  }

  char line[40];

  setFont(FONT_SMALL);
  setFontColour(50, 255, 130);
  print_xy("ACCELERATION (m/s2)", 12, 79);

  setFontColour(220, 230, 240);
  snprintf(line, sizeof(line), "X: %6.2f   Y: %6.2f",
      reading->accel_x, reading->accel_y);
  print_xy(line, 12, 94);

  snprintf(line, sizeof(line), "Z: %6.2f",
      reading->accel_z);
  print_xy(line, 12, 107);

  setFontColour(50, 210, 255);
  print_xy("GYROSCOPE (deg/s)", 12, 121);

  setFontColour(220, 230, 240);
  snprintf(line, sizeof(line), "X: %6.1f Y: %6.1f Z: %6.1f",
      reading->gyro_x, reading->gyro_y, reading->gyro_z);
  print_xy(line, 12, 134);

  setFontColour(255, 180, 30);
  snprintf(line, sizeof(line), "IMU TEMP: %.1f C",
      reading->temperature_c);
  print_xy(line, 190, 151);

  flip_frame();
}

static void draw_comms_screen(void)
{
  draw_page_header("COMMUNICATIONS", 2);

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("WIRELESS LINK STATUS", 12, 43);

  setFont(FONT_UBUNTU16);
  setFontColour(wifi_connected() ? 50 : 255,
      wifi_connected() ? 255 : 80,
      wifi_connected() ? 130 : 80);
  print_xy(wifi_connected() ? "WI-FI   CONNECTED" : "WI-FI   OFFLINE",
      12, 64);

  setFont(FONT_UBUNTU16);
  setFontColour(mqtt_connected() ? 50 : 255,
      mqtt_connected() ? 255 : 80,
      mqtt_connected() ? 130 : 80);
  print_xy(mqtt_connected() ? "MQTT    CONNECTED" : "MQTT    OFFLINE",
      12, 91);

  setFont(FONT_SMALL);
  setFontColour(170, 190, 210);
  print_xy(network_event, 12, 122);

  flip_frame();
}

static void draw_event_log_screen(void)
{
  draw_page_header("MISSION EVENT LOG", 3);

  setFont(FONT_SMALL);
  setFontColour(115, 145, 170);
  print_xy("RECENT EVENTS", 12, 43);

  int count = event_log_count();

  if (count == 0) {
    setFontColour(170, 190, 210);
    print_xy("NO EVENTS RECORDED", 12, 70);
  }

  for (int i = 0; i < count && i < 5; i++) {
    char message[EVENT_LOG_MESSAGE_SIZE];
    char line[48];
    int64_t timestamp_us;

    if (event_log_get_recent(i, message, sizeof(message),
          &timestamp_us)) {

      int elapsed_s = (int)(timestamp_us / 1000000);
      snprintf(line, sizeof(line), "+%04ds  %.28s",
          elapsed_s,
          message);

      if (i == 0) {
        setFontColour(50, 255, 130);
      } else {
        setFontColour(210, 225, 240);
      }

      print_xy(line, 12, 63 + i * 20);
    }
  }

  flip_frame();
}


static uint32_t pulse_brightness(int64_t now_us, int period_ms)
{
  int64_t period_us = (int64_t)period_ms * 1000;
  int64_t phase = now_us % period_us;

  uint32_t triangle;

  if (phase < period_us / 2) {
    triangle = (uint32_t)(phase * 2046 / period_us);
  } else {
    triangle = (uint32_t)((period_us - phase) * 2046 / period_us);
  }

  return 80 + (uint32_t)((uint64_t)triangle * 943 / 1023);
}

static void update_mission_leds(
    mission_state_t state,
    int64_t now_us,
    int64_t countdown_start_us)
{
  uint32_t green = 0;
  uint32_t blue = 0;
  uint32_t red = 0;

  switch (state) {
    case MISSION_SAFE:
      green = pulse_brightness(now_us, 1700);
      break;

    case MISSION_ARMED:
      green = 1023;
      blue = pulse_brightness(now_us, 800);
      break;

    case MISSION_COUNTDOWN: {
                              blue = 1023;

                              int64_t elapsed = now_us - countdown_start_us;
                              int seconds_left = 10 - (int)(elapsed / 1000000);

                              int pulse_period;

                              if (seconds_left > 7) {
                                pulse_period = 1000;
                              } else if (seconds_left > 4) {
                                pulse_period = 650;
                              } else if (seconds_left > 2) {
                                pulse_period = 400;
                              } else {
                                pulse_period = 200;
                              }

                              red = pulse_brightness(now_us, pulse_period);
                              break;
                            }

    case MISSION_FLIGHT:
                            blue = 1023;
                            red = pulse_brightness(now_us, 1400);
                            break;

    case MISSION_LANDED:
                            green = 1023;
                            break;

    case MISSION_ABORTED:
                            red = 1023;
                            break;
  }

  ESP_ERROR_CHECK(led_set_brightness(10, green));
  ESP_ERROR_CHECK(led_set_brightness(11, blue));
  ESP_ERROR_CHECK(led_set_brightness(12, red));
}

static int encoder_previous_clk = 1;
static int64_t encoder_last_step_us = 0;

static void encoder_init(void)
{
  gpio_config_t config = {
    .pin_bit_mask = (1ULL << ENCODER_CLK) |
      (1ULL << ENCODER_DT)  |
      (1ULL << ENCODER_SW),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  ESP_ERROR_CHECK(gpio_config(&config));

  encoder_previous_clk = gpio_get_level(ENCODER_CLK);
}


static int encoder_read_delta(void)
{
  static int encoder_previous_state = -1;
  static int encoder_step_accumulator = 0;

  // Valid quadrature transitions: previous state -> current state.
  static const int8_t transition_table[16] = {
    0, -1,  1,  0,
    1,  0,  0, -1,
    -1,  0,  0,  1,
    0,  1, -1,  0
  };

  int clk = gpio_get_level(ENCODER_CLK);
  int dt  = gpio_get_level(ENCODER_DT);

  int current_state = (clk << 1) | dt;

  // Initialise the previous state on the first read.
  if (encoder_previous_state < 0) {
    encoder_previous_state = current_state;
    return 0;
  }

  int table_index = (encoder_previous_state << 2) | current_state;
  encoder_step_accumulator += transition_table[table_index];
  encoder_previous_state = current_state;

  // Two valid transitions make one navigation step.
  if (encoder_step_accumulator >= 2) {
    encoder_step_accumulator = 0;
    return 1;
  }

  if (encoder_step_accumulator <= -2) {
    encoder_step_accumulator = 0;
    return -1;
  }

  return 0;
}


static void networking_task(void *arg)
{
  event_log_add("WIFI CONNECTING");
  mqtt_connect(NULL);

  if (wifi_connected()) {
    event_log_add("WIFI CONNECTED");
  } else {
    event_log_add("WIFI CONNECTION FAILED");
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  input_output_init();
  encoder_init();

  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  ESP_ERROR_CHECK(ret);

  graphics_init();
  set_orientation(LANDSCAPE);
  cls(0);

  ESP_ERROR_CHECK(leds_init());

  event_log_init();
  event_log_add("SYSTEM INITIALISED");

  xTaskCreate(
      networking_task,
      "networking_task",
      4096,
      NULL,
      5,
      NULL
      );

  mpu6050_reading_t imu_reading = {0};
  bool imu_ok = (mpu6050_init() == ESP_OK);

  if (imu_ok) {
    event_log_add("MPU6050 INITIALISED");
    printf("MPU6050 initialised successfully\n");
  } else {
    event_log_add("MPU6050 INIT FAILED");
    printf("MPU6050 initialisation failed\n");
  }

  printf("Mission Control: ready\n");

  mission_state_t current_state = MISSION_SAFE;
  int selected_screen = 0;

  int prev_arm = 1;
  int prev_launch = 1;
  int prev_abort = 1;
  int64_t countdown_start_us = 0;
  int64_t flight_start_us = 0;
  const int64_t countdown_duration_us = 10LL*1000000;


  while (1) {
    int arm_now = gpio_get_level(1);
    int launch_now = gpio_get_level(2);
    int abort_now = gpio_get_level(3);

    int encoder_delta = encoder_read_delta();

    static int64_t last_imu_read_us = 0;
    static int64_t last_mqtt_publish_us = 0;
    int64_t sensor_now_us = esp_timer_get_time();

    if (imu_ok && sensor_now_us - last_imu_read_us >= 100000) {
      last_imu_read_us = sensor_now_us;

      if (mpu6050_read(&imu_reading) == ESP_OK) {
        printf(
            "ACCEL %.2f %.2f %.2f | "
            "GYRO %.1f %.1f %.1f | TEMP %.1f C\n",
            imu_reading.accel_x,
            imu_reading.accel_y,
            imu_reading.accel_z,
            imu_reading.gyro_x,
            imu_reading.gyro_y,
            imu_reading.gyro_z,
            imu_reading.temperature_c
            );

        if (mqtt_connected() &&
            sensor_now_us - last_mqtt_publish_us >= 500000) {

          int msg_id = mqtt_publish_telemetry(&imu_reading);

          if (msg_id >= 0) {
            last_mqtt_publish_us = sensor_now_us;
          }
        }
      } else {
        printf("MPU6050 read failed\n");
      }
    }

    if (encoder_delta != 0) {
      selected_screen += encoder_delta;

      if (selected_screen < 0) {
        selected_screen = SCREEN_COUNT - 1;
      } else if (selected_screen >= SCREEN_COUNT) {
        selected_screen = 0;
      }

      printf("Selected screen: %d\n", selected_screen);
    }

    if (prev_arm == 1 && arm_now == 0) {
      if (current_state == MISSION_SAFE) {
        current_state = MISSION_ARMED;
        event_log_add("MISSION ARMED");
      } else if (current_state == MISSION_ARMED) {
        current_state = MISSION_SAFE;
        event_log_add("MISSION SAFE");
      }
    }

    if (prev_launch == 1 && launch_now == 0) {
      if (current_state == MISSION_ARMED) {
        countdown_start_us = esp_timer_get_time();
        current_state = MISSION_COUNTDOWN;
        event_log_add("COUNTDOWN STARTED");
      }
    }

    if (prev_abort == 1 && abort_now == 0) {
      if (current_state == MISSION_COUNTDOWN ||
          current_state == MISSION_FLIGHT) {
        current_state = MISSION_ABORTED;
        event_log_add("MISSION ABORTED");

      }
    }

    int64_t now_us = esp_timer_get_time();
    char timer_text[32] = "";

    if (current_state == MISSION_COUNTDOWN) {
      int64_t elapsed_us = now_us - countdown_start_us;

      if (elapsed_us >= countdown_duration_us) {
        flight_start_us = countdown_start_us + countdown_duration_us;
        current_state = MISSION_FLIGHT;
        event_log_add("FLIGHT STARTED");

      } else {
        int seconds_left = 10 - (int)(elapsed_us / 1000000);

        snprintf(timer_text, sizeof(timer_text),
            "T-00:%02d", seconds_left);
      }
    }

    if (current_state == MISSION_FLIGHT) {
      int64_t elapsed_seconds = (now_us - flight_start_us) / 1000000;
      int minutes = (int)(elapsed_seconds / 60);
      int seconds = (int)(elapsed_seconds % 60);

      snprintf(timer_text, sizeof(timer_text),
          "T+%02d:%02d", minutes, seconds);
    }

    update_mission_leds(current_state, now_us, countdown_start_us);

    prev_arm = arm_now;
    prev_launch = launch_now;
    prev_abort = abort_now;

    switch (selected_screen) {
      case 0:
        draw_mission_screen(current_state, timer_text);
        break;

      case 1:
        draw_telemetry_screen(&imu_reading, imu_ok);
        break;

      case 2:
        draw_comms_screen();
        break;

      case 3:
        draw_event_log_screen();
        break;

      default:
        selected_screen = 0;
        draw_mission_screen(current_state, timer_text);
        break;
    }

    // printf("Current mission state: %d\n", current_state);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
