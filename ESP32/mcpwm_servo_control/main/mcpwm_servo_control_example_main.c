/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "example";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

/*/ This function returns the angle in degrees converted to a value proportional to the pulse 
    width in microseconds starting from SERVO_MIN_PULSEWIDTH_US */
static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

/*
    (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)
    -------------------------------------------------------------------------------- + SERVO_MIN_PULSEWIDTH_US
                            (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE)
*/

void app_main(void)
{
    ESP_LOGI(TAG, "Create timer and operator");

    // Step 1: creates a null pointer timer handle
    mcpwm_timer_handle_t timer = NULL;

    // Step 2: initializes the timer configuration struct
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };

    /* Step 3: creates a motor control PWM timer and check the error code,
       if not ESP_OK, terminates the program */
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    /* Step 4: create a null pointer to a struct of type mcpwm_oper_t (a motor control PWM
       operator handle) */
    mcpwm_oper_handle_t oper = NULL;

    /* Step 5: create a motor control PWM operator configuration struct and initialize his
       group_id */
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };

    /* Step 6: create a motor control PWM operator and check the error code, if not ESP_OK,
       terminates the program */
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG, "Connect timer and operator");

    /* Step 7: Connect the motor control PWM operator and timer, so that the operator can be
       driven by the timer */
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");

    /* Step 8: Define the motor control PWM comparator handle (a pointer to a struct of type
       mcpwm_cmpr_t) and initialize it as NULL */
    mcpwm_cmpr_handle_t comparator = NULL;

    /* Step 9: Define the motor control PWM comparator configuration structure */
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };

    /* Step 10: Create the comparator for the operator previously created */
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    /* Step 11: Define the motor control PWM generator handle (a pointer to a mcpwm_gen_t type
       struct) */
    mcpwm_gen_handle_t generator = NULL;

    /* Step 12: Define the motor control PWM generator configuration structure */
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PULSE_GPIO,
    };

    /* Step 13: Allocates the generator for the previously created operator and check the error
       code, if not ESP_OK, terminates the program */
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    /*/ Step 14: Set the initial compare value, so that the servo will spin to the center 
        position, the angle is converted to a pulsewidth value in microseconds, also checks
        the error code */
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(0)));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    
    /*/ Step 15: Set generator action on timer event: go high on counter empty */
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    int angle = 0;
    int step = 2;
    while (1) {
        ESP_LOGI(TAG, "Angle of rotation: %d", angle);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
        //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
        vTaskDelay(pdMS_TO_TICKS(500));
        if ((angle + step) > 60 || (angle + step) < -60) {
            step *= -1;
        }
        angle += step;
    }
}
