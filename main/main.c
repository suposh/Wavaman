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
#define TAG "Wave"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR lv_tick_task(void *arg);
static void event_handler(lv_obj_t * obj, lv_event_t event);

void guiTask();
void event_handler(lv_obj_t * obj, lv_event_t event);
void lv_ex_list_1(void);


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

	lv_obj_t * list1 = lv_list_create(lv_scr_act(), NULL);
	lv_obj_set_size(list1, 128, 64);
	lv_obj_align(list1, NULL, LV_ALIGN_CENTER, 0, 0);

	/*Add buttons to the list*/

	lv_obj_t * list_btn;

	list_btn = lv_list_add_btn(list1, LV_SYMBOL_FILE, "Waveform");
	lv_obj_set_event_cb(list_btn, event_handler);

	list_btn = lv_list_add_btn(list1, LV_SYMBOL_DIRECTORY, "Amplitude");
	lv_obj_set_event_cb(list_btn, event_handler);

	list_btn = lv_list_add_btn(list1, LV_SYMBOL_CLOSE, "Frequency");
	lv_obj_set_event_cb(list_btn, event_handler);

	list_btn = lv_list_add_btn(list1, LV_SYMBOL_EDIT, "Sleep");
	lv_obj_set_event_cb(list_btn, event_handler);

	list_btn = lv_list_add_btn(list1, LV_SYMBOL_SAVE, "Power Off");
	lv_obj_set_event_cb(list_btn, event_handler);

	lv_list_set_scroll_propagation(list1, true);
	lv_list_set_anim_time(list1, 1500);
	lv_list_focus(list_btn, LV_ANIM_ON);
	lv_list_set_btn_selected(list1, list_btn);
	// lv_list_up(list1);
	void pseudo_button(lv_task_t *task){
		lv_obj_t *list1 = task->user_data;
		lv_list_down(list1);
	}
	lv_task_t *pseudo_button_task = lv_task_create(pseudo_button, 2000, LV_TASK_PRIO_MID, list1);



    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff

        if (xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
			// vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }

    //A task should NEVER return
    vTaskDelete(NULL);
}

static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_CLICKED) {
        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
    }
}

void lv_ex_list_1(void)
{
    /*Create a list*/
    lv_obj_t * list1 = lv_list_create(lv_scr_act(), NULL);
    lv_obj_set_size(list1, 128, 64);
    lv_obj_align(list1, NULL, LV_ALIGN_CENTER, 0, 0);

    /*Add buttons to the list*/

    lv_obj_t * list_btn;

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_FILE, "New");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_DIRECTORY, "Open");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_CLOSE, "Delete");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_EDIT, "Edit");
    lv_obj_set_event_cb(list_btn, event_handler);

    list_btn = lv_list_add_btn(list1, LV_SYMBOL_SAVE, "Save");
    lv_obj_set_event_cb(list_btn, event_handler);
}
