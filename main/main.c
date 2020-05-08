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
uint8_t id = 3;
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
	/*Create a Tab view object*/
    lv_obj_t *tabview;

    tabview = lv_tabview_create(lv_scr_act(), NULL);
	lv_style_t style1 = lv_style_transp, style2 = lv_style_plain_color, style3 = lv_style_transp;
	lv_tabview_set_anim_time(tabview, 800);
	// lv_style_plain_color  lv_style_transp
    /*Add 2 tabs (the tabs are page (lv_page) and can be scrolled*/
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "SET");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "STATS");


	style1.body.padding.top = 1;
	style1.body.padding.bottom = 2;
	// style1.body.shadow.color = LV_COLOR_WHITE;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_BG,  &style1);

	// style2.body.radius = 10;
	style2.body.padding.inner = 0;
	// style2.body.grad_color = LV_COLOR_WHITE;
	//
	// style2.body.padding.bottom = 00;
	// style2.body.padding.top = 00;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_INDIC,  &style2);

	style3.body.padding.top = 3;
	style3.body.padding.bottom = 2;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_REL,  &style3);
	lv_obj_refresh_style(tabview);


    /*Add content to the tabs*/
    lv_obj_t * label = lv_label_create(tab1, NULL);
    lv_label_set_text(label, "First tab");

    label = lv_label_create(tab2, NULL);
    lv_label_set_text(label, "Second tab");


	void pseudo_button(lv_task_t *task){
		lv_obj_t *list1 = task->user_data;
		lv_tabview_set_tab_act(list1, id, LV_ANIM_ON);
		// printf("now %d\n",id);
		if(id >= 3) id = 0;
		else id++;
		// else printf("change\n");

	}
	lv_task_t *pseudo_button_task = lv_task_create(pseudo_button, 2000, LV_TASK_PRIO_MID, tabview);



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
