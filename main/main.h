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

//Function prototypes
void guiTask();
static bool button_read(lv_indev_drv_t * drv, lv_indev_data_t*data);
static bool back_button(lv_indev_drv_t * drv, lv_indev_data_t*data);

static void select_frequency(lv_obj_t * obj, lv_event_t event);
static void select_amplitude(lv_obj_t * obj, lv_event_t event);
static void select_waveform(lv_obj_t * obj, lv_event_t event);
static void select_logic(lv_obj_t * obj, lv_event_t event);
static void select_stats(lv_obj_t * obj, lv_event_t event);
