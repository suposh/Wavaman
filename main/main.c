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
#include "lvgl/src/lv_themes/lv_theme_mono.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR lv_tick_task(void *arg);
static void event_handler(lv_obj_t * obj, lv_event_t event);

void guiTask();
void lv_ex_line_1(void);
void lv_ex_label_1(void);
void lv_ex_btn_1(void);


/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    //If you want to use a task to create the graphic, you NEED to create a Pinned task
    //Otherwise there can be problem such as memory corruption and so on
    xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);
}

static void IRAM_ATTR lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(portTICK_RATE_MS);
}

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
SemaphoreHandle_t xGuiSemaphore;

void guiTask() {
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    lvgl_driver_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_buf_t disp_buf;
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;


    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    lv_theme_mono_init(0, NULL);
    lv_theme_set_current( lv_theme_get_mono() );




    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_task,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10*1000)); //10ms (expressed as microseconds)


    /* use a pretty small demo for 128x64 monochrome displays */
    lv_obj_t * scr = lv_disp_get_scr_act(NULL);     /*Get the current screen*/
	// lv_ex_btn_1();
    /*Create a Label on the currently active screen*/
    // lv_obj_t * label1 =  lv_label_create(scr, NULL);

    /*Modify the Label's text*/
    // lv_label_set_text(label1, "Hello\nworld!");
	// lv_ex_label_1();
    /* Align the Label to the center
     * NULL means align on parent (which is the screen now)
     * 0, 0 at the end means an x, y offset after alignment*/
    // lv_obj_align(label1, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);

	lv_obj_t * label;
	uint8_t boo = 0;
	//
	// lv_obj_t * btn1 = lv_btn_create(lv_scr_act(), NULL);
	// lv_obj_set_event_cb(btn1, event_handler);
	// lv_obj_align(btn1, NULL, LV_ALIGN_CENTER, 0, 0);
	//
	// label = lv_label_create(btn1, NULL);
	// lv_label_set_text(label, "Button");

	lv_obj_t * btn2 = lv_btn_create(lv_scr_act(), NULL);
	lv_obj_set_event_cb(btn2, event_handler);
	lv_obj_align(btn2, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
	lv_btn_set_toggle(btn2, true);
	lv_btn_toggle(btn2);
	lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

	label = lv_label_create(btn2, NULL);
	lv_label_set_text(label, "Toggled");
	lv_btn_set_toggle(btn2, true);


    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		lv_btn_toggle(btn2);
        if (xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }

    //A task should NEVER return
    vTaskDelete(NULL);
}


void lv_ex_line_1(void)
{
    /*Create an array for the points of the line*/
    static lv_point_t line_points[] = { {10, 20}, {10, 50} };

    /*Create new style (thick dark blue)*/
    static lv_style_t style_line;
    lv_style_copy(&style_line, &lv_style_plain);
    style_line.line.color = LV_COLOR_MAKE(0x00, 0x3b, 0x75);
    style_line.line.width = 1;
    style_line.line.rounded = 1;

    /*Copy the previous line and apply the new style*/
    lv_obj_t * line1;
    line1 = lv_line_create(lv_scr_act(), NULL);
    lv_line_set_points(line1, line_points, 2);     /*Set the points*/
    lv_line_set_style(line1, LV_LINE_STYLE_MAIN, &style_line);
    // lv_obj_align(line1, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
}
#include "lvgl/lvgl.h"

void lv_ex_label_1(void)
{
    lv_obj_t * label1 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_BREAK);     /*Break the long lines*/
    lv_label_set_recolor(label1, true);                      /*Enable re-coloring by commands in the text*/
    lv_label_set_align(label1, LV_ALIGN_IN_LEFT_MID);       /*Center aligned lines*/
	// #000080 Re-color# #0000ff words# #6666ff of a#
    lv_label_set_text(label1, "Check this out,wrap long text automatically.");
    lv_obj_set_width(label1, 127);
    lv_obj_align(label1, NULL, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * label2 = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_SROLL_CIRC);     /*Circular scroll*/
    lv_obj_set_width(label2, 127);
    lv_label_set_text(label2, "It is a circularly scrolling text. ");
    lv_obj_align(label2, NULL,  LV_ALIGN_IN_TOP_MID, 0, 0);
}
static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        printf("Clicked\n");
    }
    else if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Toggled\n");
    }
}

void lv_ex_btn_1(void){
    lv_obj_t * label;
	//
    // lv_obj_t * btn1 = lv_btn_create(lv_scr_act(), NULL);
    // lv_obj_set_event_cb(btn1, event_handler);
    // lv_obj_align(btn1, NULL, LV_ALIGN_CENTER, 0, 0);
	//
    // label = lv_label_create(btn1, NULL);
    // lv_label_set_text(label, "Button");

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act(), NULL);
    lv_obj_set_event_cb(btn2, event_handler);
    lv_obj_align(btn2, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_btn_set_toggle(btn2, true);
    lv_btn_toggle(btn2);
    lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

    label = lv_label_create(btn2, NULL);
    lv_label_set_text(label, "Toggled");
	lv_btn_set_toggle(btn2, true);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	lv_btn_toggle(btn2);
}
