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
void render_list(lv_obj_t *tab1, lv_obj_t * list1, lv_obj_t *list_btn, lv_style_t *style4, lv_style_t *style5, lv_style_t *style6, lv_group_t * g);
static bool button_read(lv_indev_drv_t * drv, lv_indev_data_t*data);
