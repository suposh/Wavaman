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
#include "main.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "Wave"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR lv_tick_task(void *arg);
static void event_handler(lv_obj_t * obj, lv_event_t event);

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

	static lv_obj_t *tabview, *tab1, *tab2;		//Create tabs
    static lv_obj_t *list1, *list_btn ;			/*Create a list*/
	static lv_group_t * g;

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

	static lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);             /*Basic initialization*/
	indev_drv.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	indev_drv.read_cb = button_read;           /*See below.*/

	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = GPIO_SEL_21;         /*Set GPIO PIN*/
	io_conf.pull_down_en = 1;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);




	/*Create a Tab view object*/
	tabview = lv_tabview_create(lv_scr_act(), NULL);
	/*Add 2 tabs (the tabs are page (lv_page) and can be scrolled*/
	tab1 = lv_tabview_add_tab(tabview, "SET");
	tab2 = lv_tabview_add_tab(tabview, "STATS");

	lv_style_t style2 = lv_style_plain_color;
	style2.body.padding.inner = 0;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_INDIC,  &style2);

	lv_style_t style3 = lv_style_transp;
	style3.body.padding.top = 3;
	style3.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_REL,  &style3);
	lv_obj_refresh_style(tabview);

	lv_tabview_set_anim_time(tabview, 200);
	lv_style_t style7 = lv_style_pretty_color; //lv_style_pretty
	style7.body.opa = LV_OPA_TRANSP;
	style7.body.border.width = 0;
	lv_page_set_style(tab1, LV_PAGE_STYLE_SB, &style7);		// PAGE Scroll bar hidden

	lv_style_t style1 = lv_style_transp;
	style1.body.padding.top = 1;
	style1.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_BG, &style1);
	// menu(tabview, &style1);

	lv_style_t style4 = lv_style_transp_fit;
	lv_style_t style5 = lv_style_transp_fit; //lv_style_pretty
	lv_style_t style6 = lv_style_pretty;

	g = lv_group_create();
	render_list(tab1, list1, list_btn, &style4, &style5, &style6, g);
	// lv_group_add_obj(g, list1);
	lv_indev_set_group(my_indev, g);


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

static void event_handler(lv_obj_t * obj, lv_event_t event){
    if(event == LV_KEY_DOWN) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		lv_list_focus(obj, LV_ANIM_ON);
		// lv_list_set_btn_selected(list1, obj);
    }
	else printf("event handler %d\n", event);
}
void render_list(lv_obj_t *tab1, lv_obj_t * list1, lv_obj_t *list_btn, lv_style_t *style4, lv_style_t *style5, lv_style_t *style6, lv_group_t * g) {

	list1 = lv_list_create(tab1, NULL);
	lv_obj_set_size(list1, 128, 50);
	lv_obj_align(list1, NULL,  LV_ALIGN_IN_TOP_LEFT, 0, 0);
	lv_list_set_anim_time(list1, 60);
	lv_list_set_sb_mode(list1, LV_SB_MODE_OFF); 	// LIST Disable Scroll Bar
	lv_list_set_single_mode(list1, true);

	style4->body.border.width =0;
	style4->body.padding.left = -1;
	style4->body.padding.right = -1;
	// style4.body.padding.top = 0;
	lv_list_set_style(list1, LV_LIST_STYLE_BG, style4);

	style5->body.padding.inner = -3;
	style5->body.border.width = 0;
	// style5.body.padding.top = -5;
	lv_list_set_style(list1, LV_LIST_STYLE_SCRL, style5);

	// To adjust the height of an individual list button,
	// dimensions of the container need to be changed.
	style6->body.padding.top = 6;
	style6->body.padding.bottom = 5;
	style6->body.radius = 0;

	// Long Strings cause glitchy scroll, in the button label.
	list_btn = lv_list_add_btn(list1, NULL, "1.Frequency");
	lv_obj_set_event_cb(list_btn, event_handler);
	lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, style6);
	// lv_group_add_obj(g, list_btn);

	list_btn = lv_list_add_btn(list1, NULL, "2.Amplitude");
	lv_obj_set_event_cb(list_btn, event_handler);
	lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, style6);
	// lv_group_add_obj(g, list_btn);

	list_btn = lv_list_add_btn(list1, NULL, "3.Waveform");
	lv_obj_set_event_cb(list_btn, event_handler);
	lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, style6);
	// lv_list_set_btn_selected(list1, list_btn);
	// lv_group_add_obj(g, list_btn);

	list_btn = lv_list_add_btn(list1, NULL, "4.Logic In");
	lv_obj_set_event_cb(list_btn, event_handler);
	lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, style6);
	// lv_group_add_obj(g, list_btn);

	list_btn = lv_list_add_btn(list1, NULL, "5.Stats");
	lv_obj_set_event_cb(list_btn, event_handler);
	lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, style6);

	lv_group_add_obj(g, list1);
}

static bool button_read(lv_indev_drv_t * drv, lv_indev_data_t*data){

	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_21)) {
		data->state = LV_BTN_STATE_PR;
		printf("button_read 1\n");
		last_key = LV_KEY_DOWN;
	}
	else data->state = LV_BTN_STATE_REL;

	data->key = last_key;            /*Get the last pressed or released key*/

	return false; /*No buffering now so no more data read*/
}
