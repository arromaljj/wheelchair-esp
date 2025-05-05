/*=====================================================================
 * motor_control.c — Differential‑drive motor controller for ESP32
 *
 * Soft‑stop watchdog version — May 5 2025
 *  • Each incoming command (MQTT, UART, etc.) sets a TARGET speed.
 *  • If no command is received for MOTOR_DECAY_MS, TARGET is forced to 0.
 *  • A 10 ms control task slews the ACTUAL output toward TARGET by a
 *    fixed percent per tick, producing a linear 300 ms decay to zero.
 *  • Uses the same PWM + DIR interface as before (MDD20A or similar).
 *====================================================================*/

#include <math.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "motor_control.h"     // public API / pin definitions

static const char *TAG = "MOTOR_CTRL";

/*---------------------------------------------------------------------
 * Timing and slew‑rate configuration
 *-------------------------------------------------------------------*/
#define MOTOR_DECAY_MS          300                 // stop after this time
#define MOTOR_TASK_PERIOD_MS    10                  // control loop period
#define MOTOR_SLEW_DELTA        (100.0f * (float)MOTOR_TASK_PERIOD_MS / \
                                 (float)MOTOR_DECAY_MS)   // ≈ 3.33 % per tick

/*---------------------------------------------------------------------
 * Internal state
 *-------------------------------------------------------------------*/
static volatile int16_t g_target_left  = 0;          // last commanded value
static volatile int16_t g_target_right = 0;
static int16_t g_actual_left  = 0;                  // what we output now
static int16_t g_actual_right = 0;
static int64_t g_last_cmd_us  = 0;                  // for watchdog

/* Forward declarations */
static void motor_apply_speeds(int left, int right);
static int16_t slew(int16_t cur, int16_t tgt, float delta);
static void motor_timer_cb(void *arg);

/*=====================================================================
 * Public API implementation
 *====================================================================*/

void motor_control_init(void)
{
    /* -------- GPIO direction pins ------------------------------- */
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode      = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << MOTOR1_DIR_PIN) | (1ULL << MOTOR2_DIR_PIN);
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "Motor DIR pins configured (M1 DIR=%d, M2 DIR=%d)",
             MOTOR1_DIR_PIN, MOTOR2_DIR_PIN);

    /* -------- LEDC timer (shared) --------------------------------- */
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = MOTOR_LEDC_SPEED_MODE,
        .timer_num       = MOTOR_PWM_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz         = MOTOR_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_LOGI(TAG, "LEDC timer %d set to %d Hz, %d‑bit",
             MOTOR_PWM_TIMER, MOTOR_PWM_FREQ_HZ,
             (int)log2f((float)(1 << MOTOR_PWM_RESOLUTION)));

    /* -------- LEDC channel — Motor 1 ------------------------------ */
    ledc_channel_config_t ch1 = {
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .channel    = MOTOR_PWM_CHANNEL_M1,
        .timer_sel  = MOTOR_PWM_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = MOTOR1_PWM_PIN,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch1));

    /* -------- LEDC channel — Motor 2 ------------------------------ */
    ledc_channel_config_t ch2 = {
        .speed_mode = MOTOR_LEDC_SPEED_MODE,
        .channel    = MOTOR_PWM_CHANNEL_M2,
        .timer_sel  = MOTOR_PWM_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = MOTOR2_PWM_PIN,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch2));

    /* -------- Make sure we start stopped -------------------------- */
    motor_emergency_stop();

    /* -------- Start the 10 ms control timer ----------------------- */
    esp_timer_handle_t h;
    const esp_timer_create_args_t targs = {
        .callback = motor_timer_cb,
        .name     = "motor_ctrl"
    };
    ESP_ERROR_CHECK(esp_timer_create(&targs, &h));
    ESP_ERROR_CHECK(esp_timer_start_periodic(h, MOTOR_TASK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Motor control initialised; watchdog active");
}

void motor_set_speeds(int left_speed, int right_speed)
{
    /* clip */
    if (left_speed  > 100) left_speed  = 100;
    if (left_speed  < -100) left_speed = -100;
    if (right_speed > 100) right_speed = 100;
    if (right_speed < -100) right_speed = -100;

    g_target_left  = left_speed;
    g_target_right = right_speed;
    g_last_cmd_us  = esp_timer_get_time();

    ESP_LOGD(TAG, "Cmd rx: L=%d R=%d (%%)", left_speed, right_speed);
}

void motor_get_speeds(int *left_speed, int *right_speed)
{
    if (left_speed)  *left_speed  = g_actual_left;
    if (right_speed) *right_speed = g_actual_right;
}

void motor_emergency_stop(void)
{
    ESP_LOGW(TAG, "EMERGENCY STOP");
    g_target_left   = 0;
    g_target_right  = 0;
    g_actual_left   = 0;
    g_actual_right  = 0;
    g_last_cmd_us   = esp_timer_get_time();
    motor_apply_speeds(0, 0);
}

/*=====================================================================
 * Internal helpers
 *====================================================================*/

/* convert ±100 % → PWM + DIR */
static void motor_apply_speeds(int left, int right)
{
    const uint32_t max_duty = (1 << MOTOR_PWM_RESOLUTION) - 1;

    /* ---------------- Motor 1 ---------------- */
    uint32_t duty1 = (uint32_t)(fabsf((float)left) * max_duty / 100.0f);
    int dir1 = (left < 0) ? 1 : 0;   // 0 = forward, 1 = reverse
    gpio_set_level(MOTOR1_DIR_PIN, dir1);
    ESP_ERROR_CHECK(ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_PWM_CHANNEL_M1, duty1));
    ESP_ERROR_CHECK(ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_PWM_CHANNEL_M1));

    /* ---------------- Motor 2 ---------------- */
    uint32_t duty2 = (uint32_t)(fabsf((float)right) * max_duty / 100.0f);
    int dir2 = (right < 0) ? 1 : 0;
    gpio_set_level(MOTOR2_DIR_PIN, dir2);
    ESP_ERROR_CHECK(ledc_set_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_PWM_CHANNEL_M2, duty2));
    ESP_ERROR_CHECK(ledc_update_duty(MOTOR_LEDC_SPEED_MODE, MOTOR_PWM_CHANNEL_M2));
}

/* first‑order slew filter — limit to ±delta per tick */
static int16_t slew(int16_t cur, int16_t tgt, float delta)
{
    float diff = (float)tgt - (float)cur;
    if (fabsf(diff) <= delta) return tgt;
    if (diff > 0) return (int16_t)roundf((float)cur + delta);
    return (int16_t)roundf((float)cur - delta);
}

/* 10 ms periodic callback */
static void motor_timer_cb(void *arg)
{
    int64_t age_us = esp_timer_get_time() - g_last_cmd_us;
    if (age_us >= MOTOR_DECAY_MS * 1000) {
        g_target_left  = 0;
        g_target_right = 0;
    }

    g_actual_left  = slew(g_actual_left,  g_target_left,  MOTOR_SLEW_DELTA);
    g_actual_right = slew(g_actual_right, g_target_right, MOTOR_SLEW_DELTA);

    motor_apply_speeds(g_actual_left, g_actual_right);
}
