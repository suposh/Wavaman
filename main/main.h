#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"

/* Littlevgl specific */
#include "lvgl/lvgl.h"
#include "lvgl_driver.h"

//STATIC PROTOTYPES
static void IRAM_ATTR lv_tick_task(void *arg);

//Function prototypes
void guiTask();
static bool keypad_UP_DOWN_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);
static bool keypad_Back_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);
static bool keypad_ENTER_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);
static bool keypad_LEFT_RIGHT_cb(lv_indev_drv_t * drv, lv_indev_data_t*data);

static void select_frequency(lv_obj_t * obj, lv_event_t event);
static void select_amplitude(lv_obj_t * obj, lv_event_t event);
static void select_waveform(lv_obj_t * obj, lv_event_t event);
static void select_logic(lv_obj_t * obj, lv_event_t event);
static void select_stats(lv_obj_t * obj, lv_event_t event);

static void spinbox_frequency_cb(lv_obj_t * obj, lv_event_t event);
static void roller_waveform_cb(lv_obj_t * obj, lv_event_t event);

struct MENU_DATA {
	uint32_t frequency;
	uint8_t  amplitude;
	uint8_t  waveform;
	uint8_t  gain;
};
