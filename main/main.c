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
signed int id = 0;
signed int dir = -1;
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
	lv_tabview_set_anim_time(tabview, 200);
    /*Add 2 tabs (the tabs are page (lv_page) and can be scrolled*/
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "SET");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "STATS");


	style1.body.padding.top = 1;
	style1.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_BG,  &style1);

	style2.body.padding.inner = 0;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_INDIC,  &style2);

	style3.body.padding.top = 3;
	style3.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_REL,  &style3);
	lv_obj_refresh_style(tabview);

    /*Create a list*/
    lv_obj_t * list1 = lv_list_create(tab1, NULL);
    lv_obj_set_size(list1, 115, 33);
	lv_page_set_sb_mode(list1, LV_SB_MODE_OFF); 	// Disable Scroll Bar
    lv_obj_align(list1, NULL, LV_ALIGN_CENTER, 0, 0);
	lv_list_set_anim_time(list1, 100);
	lv_list_set_layout(list1, LV_LAYOUT_COL_L);

	lv_style_t style4 = lv_style_transp_fit;
	style4.body.border.width =0;
	// style4.body.padding.top = 0;
	// style4.body.padding.top = 0;
	lv_list_set_style(list1, LV_LIST_STYLE_BG, &style4);

	lv_style_t style5 = lv_style_transp_fit; //lv_style_pretty
	style5.body.padding.inner = 0;
	style5.body.border.width = 0;
	// style5.body.padding.top = -5;
	lv_list_set_style(list1, LV_LIST_STYLE_SCRL, &style5);

    lv_obj_t * list_btn[5];

	// Long Strings cause glitchy scroll, in the button label.
    list_btn[0] = lv_list_add_btn(list1, NULL, "1.Frequency");
	lv_obj_set_event_cb(list_btn[0], event_handler);

	// To adjust the height of an individual list button,
	// dimensions of the container need to be changed.
	lv_style_t style6 = lv_style_pretty;
	style6.body.padding.top = 4;
	style6.body.padding.bottom = 4;
	style6.body.radius = 0;
	lv_btn_set_style(list_btn[0],LV_CONT_STYLE_MAIN, &style6);

    list_btn[1] = lv_list_add_btn(list1, NULL, "2.Amplitude");
    lv_obj_set_event_cb(list_btn[1], event_handler);

    list_btn[2] = lv_list_add_btn(list1, NULL, "3.Waveform");
    lv_obj_set_event_cb(list_btn[2], event_handler);

	list_btn[3] = lv_list_add_btn(list1, NULL, "4.Logic In");
    lv_obj_set_event_cb(list_btn[3], event_handler);

	list_btn[4] = lv_list_add_btn(list1, NULL, "5.Stats");
    lv_obj_set_event_cb(list_btn[4], event_handler);


	struct ls{
		lv_obj_t *btn[5];
		lv_obj_t *list;
	} *obj1, obj;
	// ls obj;
	for(uint8_t i=0; i<5; i++) obj.btn[i] = list_btn[i];
	obj.list = list1;
	obj1 = &obj;

	void pseudo_button(lv_task_t *task){
		struct ls *obj;
		obj = task->user_data;
		lv_obj_t *btn[5];
		for(uint8_t i=0; i<5; i++) btn[i]= obj->btn[i];
		lv_obj_t *list = obj->list;



		// lv_list_down(list1);

		if(dir == 1){		// go down
			// lv_list_down(list);
			printf("id %d\t dir%d\n",id, dir);
			lv_list_focus(btn[id], LV_ANIM_ON);
			// lv_list_set_btn_selected(list, btn[id]);
			if(id == 0) {
				dir = -1;
				id++;
			}
			else id--;
		}
		else{		// go up
			// lv_list_up(list);
			printf("id %d\t dir%d\n",id, dir);
			lv_list_focus(btn[id], LV_ANIM_ON);
			// lv_list_set_btn_selected(list, btn[id]);
			if(id == 4){
				dir = 1;
				id--;
			}
			else id++;
		}

		// else id++;
		// else printf("change\n");
	}
	lv_task_t *pseudo_button_task = lv_task_create(pseudo_button, 2000, LV_TASK_PRIO_MID, obj1);



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
