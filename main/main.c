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


#define MENU_SCREEN 1
#define MENU_FREQUENCY_SET_SCREEN 10
#define MENU_AMPLITUDE_SET_SCREEN 11
#define MENU_WAVEFORM_SET_SCREEN  12
#define MENU_LOGIC_SET_SCREEN     13
#define MENU_STATS_SET_SCREEN     20
/**********************
 *  STATIC PROTOTYPES
 **********************/
static void IRAM_ATTR lv_tick_task(void *arg);
// static void event_handler(lv_obj_t * obj, lv_event_t event);

static uint8_t Current_Screen = 1;
static uint8_t Change_Screen  = 1;
static uint8_t Next_Screen    = 1;
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
	static lv_obj_t * te = NULL;

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

	static lv_indev_drv_t keypad_list1;
	lv_indev_drv_init(&keypad_list1);             /*Basic initialization*/
	keypad_list1.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_list1.read_cb = button_read;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * list_input_keypad = lv_indev_drv_register(&keypad_list1);

	static lv_indev_drv_t keypad_back;
	lv_indev_drv_init(&keypad_back);             /*Basic initialization*/
	keypad_back.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_back.read_cb = back_button;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * back_button_keypad = lv_indev_drv_register(&keypad_back);

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 1;
	io_conf.pull_up_en = 0;

	io_conf.pin_bit_mask = GPIO_SEL_21; // Select
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_22; // Select UP
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_23; // Select Down
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_19; // Back
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


    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff

        if (xSemaphoreTake(xGuiSemaphore, (TickType_t) 10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
			// vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

		if (Change_Screen){
			switch (Current_Screen){
			    case MENU_SCREEN:
					if(te != NULL){
						lv_obj_del(te);
						lv_obj_set_hidden(te, true);
						lv_obj_set_hidden(list1, false);
					}
					lv_tabview_clean(tab1);
					printf("Delete Menu objects\n");
			        break;
			    case MENU_FREQUENCY_SET_SCREEN:
					lv_obj_del(te);
					lv_obj_set_hidden(te, true);
					lv_tabview_clean(tab1);
			        break;
				case MENU_AMPLITUDE_SET_SCREEN:
					lv_obj_del(te);
					lv_obj_set_hidden(te, true);
					lv_tabview_clean(tab1);
			        break;
				case MENU_WAVEFORM_SET_SCREEN:
					lv_obj_del(te);
					lv_obj_set_hidden(te, true);
					lv_tabview_clean(tab1);
					break;
				case MENU_LOGIC_SET_SCREEN:
					lv_obj_del(te);
					lv_obj_set_hidden(te, true);
					lv_tabview_clean(tab1);
					break;
			    case MENU_STATS_SET_SCREEN:
					lv_obj_del(te);
					lv_obj_set_hidden(te, true);
					lv_tabview_clean(tab1);
					break;
			}
			switch (Next_Screen){

				case MENU_SCREEN:
					printf("MENU_SCREEN\n");
					list1 = lv_list_create(tab1, NULL);
					lv_obj_set_size(list1, 128, 50);
					lv_obj_align(list1, NULL,  LV_ALIGN_IN_TOP_LEFT, 0, 0);
					lv_list_set_anim_time(list1, 60);
					lv_list_set_sb_mode(list1, LV_SB_MODE_OFF); 	// LIST Disable Scroll Bar
					lv_list_set_single_mode(list1, true);

					style4.body.border.width =0;
					style4.body.padding.left = -1;
					style4.body.padding.right = -1;
					// style4.body.padding.top = 0;
					lv_list_set_style(list1, LV_LIST_STYLE_BG, &style4);

					style5.body.padding.inner = -3;
					style5.body.border.width = 0;
					// style5.body.padding.top = -5;
					lv_list_set_style(list1, LV_LIST_STYLE_SCRL, &style5);

					// To adjust the height of an individual list button,
					// dimensions of the container need to be changed.
					style6.body.padding.top = 6;
					style6.body.padding.bottom = 5;
					style6.body.radius = 0;

					// Long Strings cause glitchy scroll, in the button label.
					list_btn = lv_list_add_btn(list1, NULL, "1.Frequency");
					lv_obj_set_event_cb(list_btn, select_frequency);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);
					// lv_group_add_obj(g, list_btn);

					list_btn = lv_list_add_btn(list1, NULL, "2.Amplitude");
					lv_obj_set_event_cb(list_btn, select_amplitude);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);
					// lv_group_add_obj(g, list_btn);

					list_btn = lv_list_add_btn(list1, NULL, "3.Waveform");
					lv_obj_set_event_cb(list_btn, select_waveform);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);
					// lv_list_set_btn_selected(list1, list_btn);
					// lv_group_add_obj(g, list_btn);

					list_btn = lv_list_add_btn(list1, NULL, "4.Logic In");
					lv_obj_set_event_cb(list_btn, select_logic);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);
					// lv_group_add_obj(g, list_btn);

					list_btn = lv_list_add_btn(list1, NULL, "5.Stats");
					lv_obj_set_event_cb(list_btn, select_stats);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);

					g = lv_group_create();
					lv_group_add_obj(g, list1);
					lv_indev_set_group(list_input_keypad, g);
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_FREQUENCY_SET_SCREEN:
					printf("SET_FREQUENCY\n");
					te = lv_label_create(tab1, NULL);
					lv_label_set_text(te, "SET_FREQUENCY");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_AMPLITUDE_SET_SCREEN:
					printf("SET_AMPLITUDE\n");
					te = lv_label_create(tab1, NULL);
					lv_label_set_text(te, "SET_AMPLITUDE");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_WAVEFORM_SET_SCREEN:
					printf("SET_WAVEFORM\n");
					te = lv_label_create(tab1, NULL);
					lv_label_set_text(te, "SET_WAVEFORM");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_LOGIC_SET_SCREEN:
					printf("SET_LOGIC_IN\n");
					te = lv_label_create(tab1, NULL);
					lv_label_set_text(te, "SET_LOGIC_IN");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_STATS_SET_SCREEN:
					printf("OPEN_STATS\n");
					te = lv_label_create(tab1, NULL);
					lv_label_set_text(te, "OPEN_STATS");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
			}
		}
    }

    //A task should NEVER return
    vTaskDelete(NULL);
}

static void select_frequency(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_PRESSED) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		Next_Screen = MENU_FREQUENCY_SET_SCREEN;
		lv_list_focus(obj, LV_ANIM_ON);
		Change_Screen = 1;
		// lv_list_set_btn_selected(list1, obj);
    }
	// else printf("NO select_frequency %d\n", event);
}
static void select_amplitude(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_PRESSED) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		Next_Screen = MENU_AMPLITUDE_SET_SCREEN;
		lv_list_focus(obj, LV_ANIM_ON);
		Change_Screen = 1;
    }
	// else printf("NO select_amplitude %d\n", event);
}
static void select_waveform(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_PRESSED) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		Next_Screen = MENU_WAVEFORM_SET_SCREEN;
		lv_list_focus(obj, LV_ANIM_ON);
		Change_Screen = 1;
    }
	// else printf("NO select_waveform %d\n", event);
}
static void select_logic(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_PRESSED) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		Next_Screen = MENU_LOGIC_SET_SCREEN;
		lv_list_focus(obj, LV_ANIM_ON);
		Change_Screen = 1;
    }
	// else printf("NO select_logic %d\n", event);
}
static void select_stats(lv_obj_t * obj, lv_event_t event){
    if(event == LV_EVENT_PRESSED) {

        printf("Clicked: %s\n", lv_list_get_btn_text(obj));
		Next_Screen = MENU_STATS_SET_SCREEN;
		lv_list_focus(obj, LV_ANIM_ON);
		Change_Screen = 1;
    }
	// else printf("NO select_logic %d\n", event);
}

static bool button_read(lv_indev_drv_t * drv, lv_indev_data_t*data){

	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_21)) {
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_ENTER;
	}else if(gpio_get_level(GPIO_NUM_22)) { // Select UP
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_UP;
	}else if(gpio_get_level(GPIO_NUM_23)) { // Select DOWN
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_DOWN;
	}else data->state = LV_BTN_STATE_REL;

	data->key = last_key;            /*Get the last pressed or released key*/

	return false; /*No buffering now so no more data read*/
}
static bool back_button(lv_indev_drv_t * drv, lv_indev_data_t*data){
	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_19) && Current_Screen != MENU_SCREEN) { // Select DOWN
		data->state = LV_BTN_STATE_PR;
		printf("Change to MENU\n");
		last_key = LV_KEY_HOME;
		Next_Screen = MENU_SCREEN;
		Change_Screen = 1;
	}else data->state = LV_BTN_STATE_REL;

	data->key = last_key;            /*Get the last pressed or released key*/

	return false; /*No buffering now so no more data read*/
}
