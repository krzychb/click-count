/* Click Count Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "click-count";

#define LED_STRIP_GPIO (GPIO_NUM_33)
#define LED_STRIP_RMT_CHANNEL 0
#define LED_STRIP_UPDATE_PERIOD_MS 100

#define BLUE_PB_GPIO (GPIO_NUM_18)
#define RED_PB_GPIO  (GPIO_NUM_22)
#define PB_DEBOUNCE_DELAY_MS 10

#define ADD_DEBOUNCE_GPIO (GPIO_NUM_38)

#define PBS_WAIT_FOR_LOW  0x10
#define PBS_WAIT_FOR_HIGH 0x20
#define PBS_DEBOUCE_WAIT  0x30
#define PBS_LOOP_IDDLE_MS 1

static led_strip_t *pStrip_a;

/* Data structure used by procedures to read push button clicks */
typedef struct pb_data_t
{
    uint32_t gpio;    // GPIO where push button is connected to
    uint32_t counter; // Number of counted clicks
} generic_pb_data_t;


/* Simple procedure to read clicks from push buttons */
static void read_pb_simple(void* x_struct)
{
    generic_pb_data_t * pb_data = (generic_pb_data_t *) x_struct;
    u_int8_t seq_step = PBS_WAIT_FOR_LOW;
    u_int8_t loop_count = 0;
    gpio_reset_pin(pb_data->gpio);
    gpio_set_direction(pb_data->gpio, GPIO_MODE_INPUT);
    while (1) {
        if (seq_step == PBS_WAIT_FOR_LOW) {
            if (gpio_get_level(pb_data->gpio) == 0) {
                seq_step = PBS_WAIT_FOR_HIGH;
            }
        }
        if (seq_step == PBS_WAIT_FOR_HIGH) {
            if (gpio_get_level(pb_data->gpio) == 1) {
                seq_step = PBS_WAIT_FOR_LOW;
                pb_data->counter++;
            }
        }
        /* Run the loop as fast as possible
        but delay the task from time to time
        to give a change to the other tasks to run */
        if (loop_count++ == 255) {
            vTaskDelay(PBS_LOOP_IDDLE_MS / portTICK_PERIOD_MS);
        }
    }
}

/* Procedure to read clicks from push buttons that includes debouncing */
static void read_pb(void* x_struct)
{
    generic_pb_data_t * pb_data = (generic_pb_data_t *) x_struct;
    unsigned long previous_time_stamp;
    u_int8_t seq_step = PBS_WAIT_FOR_LOW;
    gpio_reset_pin(pb_data->gpio);
    gpio_set_direction(pb_data->gpio, GPIO_MODE_INPUT);
    while (1) {
        if (seq_step == PBS_WAIT_FOR_LOW) {
            if (gpio_get_level(pb_data->gpio) == 0) {
                previous_time_stamp = xTaskGetTickCount();
                seq_step = PBS_DEBOUCE_WAIT;
            }
        }
        if (seq_step == PBS_DEBOUCE_WAIT) {
            if (xTaskGetTickCount() - previous_time_stamp >= PB_DEBOUNCE_DELAY_MS) {
                seq_step = PBS_WAIT_FOR_HIGH;
            }
        }
        if (seq_step == PBS_WAIT_FOR_HIGH) {
            if (gpio_get_level(pb_data->gpio) == 1) {
                seq_step = PBS_WAIT_FOR_LOW;
                pb_data->counter++;
            }
        }
        /* Delay the task to give a change to the other tasks to run */
        vTaskDelay(PBS_LOOP_IDDLE_MS / portTICK_PERIOD_MS);
    }
}


void app_main(void)
{
    ESP_LOGI(TAG, "Push Button Click Count application is starting!");

    /* LED strip initialization with the GPIO and pixels number */
    pStrip_a = led_strip_init(LED_STRIP_RMT_CHANNEL, LED_STRIP_GPIO, 8);
    pStrip_a->clear(pStrip_a, 50);  // Set all LED off to clear all pixels

    /* Configure pin to define if to debounce push buttons */
    gpio_reset_pin(ADD_DEBOUNCE_GPIO);
    gpio_set_direction(ADD_DEBOUNCE_GPIO, GPIO_MODE_INPUT);

    /* Spin off push button click counting tasks */
    generic_pb_data_t blue_pb_data = {BLUE_PB_GPIO, 0};
    generic_pb_data_t red_pb_data  = {RED_PB_GPIO,  0};
    if (gpio_get_level(ADD_DEBOUNCE_GPIO) == 1) {
        xTaskCreate(read_pb, "read_blue_pb", 2048, (void *) &blue_pb_data, 10, NULL);
        xTaskCreate(read_pb, "read_red_pb",  2048, (void *) &red_pb_data,  10, NULL);
        ESP_LOGI(TAG, "Counting clicks with debounce delay of %d ms", PB_DEBOUNCE_DELAY_MS);
    } else {
        xTaskCreate(read_pb_simple, "read_blue_pb", 2048, (void *) &blue_pb_data, 10, NULL);
        xTaskCreate(read_pb_simple, "read_red_pb",  2048, (void *) &red_pb_data,  10, NULL);
        ESP_LOGI(TAG, "Counting clicks without any debounce delay");
    }

    /* Show on LCD strip count of clicks by blue and red buttons */
    while (1) {
        int32_t diff = blue_pb_data.counter - red_pb_data.counter;
        if (diff > 4) {
            diff = 4;
        } else if (diff < -4) {
            diff = -4;
        }
        for (int i = 0; i < 4 + diff; i++) {
            pStrip_a->set_pixel(pStrip_a, i, 0, 0, 16);                
        }
        for (int i = 4 + diff; i < 8; i++) {
            pStrip_a->set_pixel(pStrip_a, i, 16, 0, 0);                
        }
        pStrip_a->refresh(pStrip_a, 100);
        /* Delay the task to give a change to the other tasks to run */
        vTaskDelay(LED_STRIP_UPDATE_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
