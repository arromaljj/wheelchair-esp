/*=====================================================================
 * motor_control.h — Public API and hardware configuration
 *
 * Works together with motor_control.c (soft‑stop watchdog version)
 *====================================================================*/

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "driver/gpio.h"
#include "driver/ledc.h"

/*---------------------------------------------------------------------
 * Hardware pin mapping — adjust to your wiring
 *-------------------------------------------------------------------*/

/* Motor 1 */
#ifndef MOTOR1_PWM_PIN
#define MOTOR1_PWM_PIN      GPIO_NUM_23   /* PWM input */
#endif
#ifndef MOTOR1_DIR_PIN
#define MOTOR1_DIR_PIN      GPIO_NUM_22   /* DIR input */
#endif

/* Motor 2 */
#ifndef MOTOR2_PWM_PIN
#define MOTOR2_PWM_PIN      GPIO_NUM_18   /* PWM input */
#endif
#ifndef MOTOR2_DIR_PIN
#define MOTOR2_DIR_PIN      GPIO_NUM_19   /* DIR input */
#endif

/*---------------------------------------------------------------------
 * PWM/LEDC configuration (shared timer)
 *-------------------------------------------------------------------*/
#ifndef MOTOR_LEDC_SPEED_MODE
#define MOTOR_LEDC_SPEED_MODE   LEDC_LOW_SPEED_MODE  /* < 80 MHz/2^res */
#endif
#ifndef MOTOR_PWM_TIMER
#define MOTOR_PWM_TIMER         LEDC_TIMER_0
#endif
#ifndef MOTOR_PWM_CHANNEL_M1
#define MOTOR_PWM_CHANNEL_M1    LEDC_CHANNEL_0
#endif
#ifndef MOTOR_PWM_CHANNEL_M2
#define MOTOR_PWM_CHANNEL_M2    LEDC_CHANNEL_1
#endif
#ifndef MOTOR_PWM_RESOLUTION
#define MOTOR_PWM_RESOLUTION    LEDC_TIMER_10_BIT     /* 0‑1023 */
#endif
#ifndef MOTOR_PWM_FREQ_HZ
#define MOTOR_PWM_FREQ_HZ       5000                  /* 5 kHz */
#endif

/*---------------------------------------------------------------------
 * Control‑loop timing (may be overridden before include)
 *-------------------------------------------------------------------*/
#ifndef MOTOR_DECAY_MS
#define MOTOR_DECAY_MS          300   /* watchdog timeout → target 0 */
#endif
#ifndef MOTOR_TASK_PERIOD_MS
#define MOTOR_TASK_PERIOD_MS    10    /* control loop period */
#endif

/*=====================================================================
 * Public API
 *====================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise GPIO, PWM and start the periodic control timer. */
void motor_control_init(void);

/**
 * Set desired wheel speeds.
 * @param left_speed   −100 … +100 (percent) — positive = forward
 * @param right_speed  −100 … +100 (percent) — positive = forward
 */
void motor_set_speeds(int left_speed, int right_speed);

/** Get the *actual* output speeds currently driven (‑100 … +100). */
void motor_get_speeds(int *left_speed, int *right_speed);

/** Immediate brake (sets target & actual to zero). */
void motor_emergency_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_H */