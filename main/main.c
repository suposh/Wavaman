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

	static lv_obj_t *tabview, *tab0, *tab1;		//Create tabs

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

	static lv_indev_drv_t keypad_UP_DOWN;
	lv_indev_drv_init(&keypad_UP_DOWN);             /*Basic initialization*/
	keypad_UP_DOWN.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_UP_DOWN.read_cb = keypad_UP_DOWN_cb;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * keypad_UD_Button = lv_indev_drv_register(&keypad_UP_DOWN);

	static lv_indev_drv_t keypad_Back;
	lv_indev_drv_init(&keypad_Back);             /*Basic initialization*/
	keypad_Back.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_Back.read_cb = keypad_Back_cb;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * keypad_Back_Button = lv_indev_drv_register(&keypad_Back);

	static lv_indev_drv_t keypad_LR_DOWN;
	lv_indev_drv_init(&keypad_LR_DOWN);             /*Basic initialization*/
	keypad_LR_DOWN.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_LR_DOWN.read_cb = keypad_LEFT_RIGHT_cb;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * keypad_LR_Button = lv_indev_drv_register(&keypad_LR_DOWN);

	static lv_indev_drv_t keypad_ENTER;
	lv_indev_drv_init(&keypad_ENTER);             /*Basic initialization*/
	keypad_ENTER.type = LV_INDEV_TYPE_KEYPAD;     /*See below.*/
	keypad_ENTER.read_cb = keypad_ENTER_cb;           /*See below.*/
	/*Register the driver in LittlevGL and save the created input device object*/
	lv_indev_t * keypad_ENTER_Button = lv_indev_drv_register(&keypad_ENTER);

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_down_en = 1;
	io_conf.pull_up_en = 0;

	io_conf.pin_bit_mask = GPIO_SEL_21; // Select
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_22; // UP Button
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_23; // Down Button
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_19; // Back
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_16; // Right Button
	gpio_config(&io_conf);

	io_conf.pin_bit_mask = GPIO_SEL_17; // Left Button
	gpio_config(&io_conf);

	/*Create a Tab view object*/
	tabview = lv_tabview_create(lv_scr_act(), NULL);
	/*Add 2 tabs (the tabs are page (lv_page) and can be scrolled*/
	tab0 = lv_tabview_add_tab(tabview, "SET");
	tab1 = lv_tabview_add_tab(tabview, "ACTIVE");
	static lv_group_t *tabview_input_group = NULL;

	lv_style_t style2 = lv_style_plain_color;
	style2.body.padding.inner = 0;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_INDIC,  &style2);

	lv_style_t style3 = lv_style_transp;
	style3.body.padding.top = 3;
	style3.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_REL,  &style3);

	lv_tabview_set_anim_time(tabview, 200);
	lv_style_t style7 = lv_style_pretty_color; //lv_style_pretty
	style7.body.opa = LV_OPA_TRANSP;
	style7.body.border.width = 0;
	lv_page_set_style(tab0, LV_PAGE_STYLE_SB, &style7);		// PAGE Scroll bar hidden

	lv_style_t style1 = lv_style_transp;
	style1.body.padding.top = 1;
	style1.body.padding.bottom = 1;
	lv_tabview_set_style(tabview, LV_TABVIEW_STYLE_BTN_BG, &style1);

	// set one button to twice the rest
	lv_tabview_ext_t * ext = (lv_tabview_ext_t *)lv_obj_get_ext_attr(tabview);
	lv_btnm_set_btn_width(ext->btns, 1, 2);
	lv_obj_refresh_style(tabview);

	lv_style_t style4 = lv_style_transp_fit;
	lv_style_t style5 = lv_style_transp_fit; //lv_style_pretty
	lv_style_t style6 = lv_style_pretty;

	static lv_obj_t *menulist = NULL, *list_btn ;			/*Create a list*/
	static lv_group_t * menulist_input_group = NULL;

	static lv_obj_t * trial_text_label = NULL;

	static lv_obj_t * spinbox_frequency = NULL;
	static lv_group_t * spinbox_input_group = NULL;
	lv_style_t *spinbox_cursor_style, *spinbox_text_style;

	static lv_obj_t * roller_waveform = NULL;
	lv_style_t *roller_text_style = NULL;
	static lv_group_t *roller_input_group = NULL;

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
			    case MENU_SCREEN:			//DELETE items on menu screen
					if(menulist != NULL){
						lv_group_del(menulist_input_group);
						menulist_input_group = NULL;
						lv_obj_set_hidden(menulist, false);
						lv_obj_del(menulist);
					}
					lv_tabview_clean(tab0);
					lv_tabview_clean(tab1);
					printf("Deleted Menu objects\n");
			        break;
			    case MENU_FREQUENCY_SET_SCREEN:
					printf("Delete old FREQUENCY stuff\n");
					lv_group_del(spinbox_input_group);
					lv_indev_enable(keypad_LR_Button, false);
					lv_obj_del(spinbox_frequency);
					lv_tabview_clean(tab0);
			        break;
				case MENU_AMPLITUDE_SET_SCREEN:
					lv_obj_del(trial_text_label);
					lv_obj_set_hidden(trial_text_label, true);
					lv_tabview_clean(tab0);
			        break;
				case MENU_WAVEFORM_SET_SCREEN:
					printf("Delete old WAVEFORM stuff\n");
					lv_group_del(roller_input_group);
					lv_obj_set_hidden(roller_waveform, true);
					// lv_obj_del(roller_waveform);
					// roller_waveform=NULL;
					lv_tabview_clean(tab0);
					break;
				case MENU_LOGIC_SET_SCREEN:
					lv_obj_del(trial_text_label);
					lv_obj_set_hidden(trial_text_label, true);
					lv_tabview_clean(tab0);
					break;
			    case MENU_STATS_SET_SCREEN:
					printf("Delete old STATS stuff\n");
					lv_obj_del(trial_text_label);
					// lv_group_del(spinbox_input_group);
					// tabview_input_group = NULL;
					lv_obj_set_hidden(trial_text_label, true);
					lv_tabview_clean(tab1);
					break;
			}
			switch (Next_Screen){

				case MENU_SCREEN:
					printf("MENU_SCREEN\n");
					lv_tabview_set_tab_act(tabview, 0, LV_ANIM_ON);
					menulist = lv_list_create(tab0, NULL);
					lv_obj_set_size(menulist, 128, 50);
					lv_obj_align(menulist, NULL,  LV_ALIGN_IN_TOP_LEFT, 0, 0);
					lv_list_set_anim_time(menulist, 60);
					lv_list_set_sb_mode(menulist, LV_SB_MODE_OFF); 	// LIST Disable Scroll Bar
					lv_list_set_single_mode(menulist, true);

					style4.body.border.width =0;
					style4.body.padding.left = -1;
					style4.body.padding.right = -1;
					// style4.body.padding.top = 0;
					lv_list_set_style(menulist, LV_LIST_STYLE_BG, &style4);

					style5.body.padding.inner = -3;
					style5.body.border.width = 0;
					// style5.body.padding.top = -5;
					lv_list_set_style(menulist, LV_LIST_STYLE_SCRL, &style5);

					// To adjust the height of an individual list button,
					// dimensions of the container need to be changed.
					style6.body.padding.top = 6;
					style6.body.padding.bottom = 5;
					style6.body.radius = 0;

					// Long Strings cause glitchy scroll, in the button label.
					list_btn = lv_list_add_btn(menulist, NULL, "1.Frequency");
					lv_obj_set_event_cb(list_btn, select_frequency);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);

					list_btn = lv_list_add_btn(menulist, NULL, "2.Amplitude");
					lv_obj_set_event_cb(list_btn, select_amplitude);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);

					list_btn = lv_list_add_btn(menulist, NULL, "3.Waveform");
					lv_obj_set_event_cb(list_btn, select_waveform);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);
					// lv_list_set_btn_selected(menulist, list_btn);

					list_btn = lv_list_add_btn(menulist, NULL, "4.Logic In");
					lv_obj_set_event_cb(list_btn, select_logic);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);

					list_btn = lv_list_add_btn(menulist, NULL, "5.Stats");
					lv_obj_set_event_cb(list_btn, select_stats);
					lv_btn_set_style(list_btn,LV_CONT_STYLE_MAIN, &style6);

					menulist_input_group = lv_group_create();
					lv_group_add_obj(menulist_input_group, menulist);
					lv_indev_set_group(keypad_UD_Button, menulist_input_group);
					lv_indev_set_group(keypad_ENTER_Button, menulist_input_group);
					// lv_tabview_set_tab_act(tabview, 0, LV_ANIM_ON);

					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_FREQUENCY_SET_SCREEN:
					printf("SET_FREQUENCY\n");
					spinbox_frequency = lv_spinbox_create(lv_scr_act(), NULL);
					lv_spinbox_set_digit_format(spinbox_frequency, 9, 0);
					lv_spinbox_step_prev(spinbox_frequency);
					lv_obj_set_width(spinbox_frequency, 110);
					lv_spinbox_set_range(spinbox_frequency, 1, 268435456);
					lv_obj_set_event_cb(spinbox_frequency, spinbox_frequency_cb);

					spinbox_text_style = lv_spinbox_get_style(spinbox_frequency, LV_LABEL_STYLE_MAIN);
					spinbox_text_style->text.letter_space = 4;
					lv_spinbox_set_style(spinbox_frequency, LV_LABEL_STYLE_MAIN, spinbox_text_style);

					spinbox_text_style = lv_spinbox_get_style(spinbox_frequency, LV_SPINBOX_STYLE_BG);
					spinbox_text_style->body.padding.top = 7;
					spinbox_text_style->body.padding.bottom = 2;
					spinbox_text_style->body.border.width = 1;
					lv_spinbox_set_style(spinbox_frequency, LV_SPINBOX_STYLE_BG, spinbox_text_style);

					spinbox_cursor_style = lv_spinbox_get_style(spinbox_frequency, LV_SPINBOX_STYLE_CURSOR);
					// spinbox_cursor_style->line.width = 1;
					spinbox_cursor_style->body.radius = 2;
					// spinbox_cursor_style.body.padding.inner  = 3;
					spinbox_cursor_style->body.padding.right = 1;
					spinbox_cursor_style->body.padding.left  = 2;
					spinbox_cursor_style->body.padding.top = 5;
					spinbox_cursor_style->body.padding.bottom = 3;
					lv_spinbox_set_style(spinbox_frequency,LV_SPINBOX_STYLE_CURSOR, spinbox_cursor_style);

					lv_ta_set_cursor_type(spinbox_frequency, LV_CURSOR_BLOCK);
					lv_spinbox_set_padding_left(spinbox_frequency, 1);
					lv_ta_set_cursor_blink_time(spinbox_frequency, 0);

					trial_text_label = lv_label_create(tab0, NULL);
					lv_label_set_text(trial_text_label, "Set Frequency:");

					lv_obj_align(spinbox_frequency, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -8);

					spinbox_text_style = lv_label_get_style(trial_text_label, LV_LABEL_STYLE_MAIN);
					spinbox_text_style->body.opa = LV_OPA_TRANSP;
					spinbox_text_style->text.letter_space = -1;
					lv_label_set_style(trial_text_label, LV_LABEL_STYLE_MAIN, spinbox_text_style);

					spinbox_input_group = lv_group_create();
					lv_group_add_obj(spinbox_input_group, spinbox_frequency);
					lv_indev_set_group(keypad_UD_Button, spinbox_input_group);
					lv_indev_set_group(keypad_ENTER_Button, spinbox_input_group);
					lv_indev_set_group(keypad_LR_Button, spinbox_input_group);
					lv_indev_enable(keypad_LR_Button, true);
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_AMPLITUDE_SET_SCREEN:
					printf("SET_AMPLITUDE\n");
					trial_text_label = lv_label_create(tab0, NULL);
					lv_label_set_text(trial_text_label, "SET_AMPLITUDE");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_WAVEFORM_SET_SCREEN:
					printf("SET_WAVEFORM\n");
					roller_waveform = lv_roller_create(lv_scr_act(), NULL);
				    lv_roller_set_options(roller_waveform,
				                        "Sinosoid\n"
				                        "Triangular\n"
				                        "Square",
				                        LV_ROLLER_MODE_NORMAL);

				    lv_roller_set_visible_row_count(roller_waveform, 2);
				    lv_obj_align(roller_waveform, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -4);
				    lv_obj_set_event_cb(roller_waveform, roller_waveform_cb);

					trial_text_label = lv_label_create(tab0, NULL);
					lv_label_set_text(trial_text_label, "SET WAVEFORM");

					roller_text_style = lv_label_get_style(trial_text_label, LV_LABEL_STYLE_MAIN);
					roller_text_style->body.opa = LV_OPA_TRANSP;
					roller_text_style->text.letter_space = -1;
					lv_label_set_style(trial_text_label, LV_LABEL_STYLE_MAIN, roller_text_style);

					roller_input_group = lv_group_create();
					lv_group_add_obj(roller_input_group, roller_waveform);
					lv_indev_set_group(keypad_UD_Button, roller_input_group);
					lv_indev_set_group(keypad_ENTER_Button, roller_input_group);


					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_LOGIC_SET_SCREEN:
					printf("SET_LOGIC_IN\n");
					trial_text_label = lv_label_create(tab0, NULL);
					lv_label_set_text(trial_text_label, "SET_LOGIC_IN");
					Change_Screen = 0;
					Current_Screen = Next_Screen;
					break;
				case MENU_STATS_SET_SCREEN:
					lv_tabview_set_tab_act(tabview, 1, LV_ANIM_ON);
					printf("OPEN_STATS\n");
					trial_text_label = lv_label_create(tab1, NULL);
					lv_label_set_text(trial_text_label, "STATS_HERE");
					lv_obj_align(trial_text_label, NULL,  LV_ALIGN_CENTER, 0, 0);
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
		// lv_list_set_btn_selected(menulist, obj);
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

static bool keypad_UP_DOWN_cb(lv_indev_drv_t * drv, lv_indev_data_t*data){

	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_22)) { // Select UP
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
static bool keypad_Back_cb(lv_indev_drv_t * drv, lv_indev_data_t*data){
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
static bool keypad_ENTER_cb(lv_indev_drv_t * drv, lv_indev_data_t*data){
	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_21)) {
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_ENTER;
	}
	else data->state = LV_BTN_STATE_REL;
	data->key = last_key;            /*Get the last pressed or released key*/
	return false; /*No buffering now so no more data read*/
}
static bool keypad_LEFT_RIGHT_cb(lv_indev_drv_t * drv, lv_indev_data_t*data){
	static uint8_t last_key = 0;
	// printf("%d\n",gpio_get_level(GPIO_NUM_21));
	if(gpio_get_level(GPIO_NUM_17)) { 		// Select Left
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_LEFT;
	}else if(gpio_get_level(GPIO_NUM_16)) { // Select Right
		data->state = LV_BTN_STATE_PR;
		// printf("button_read 1\n");
		last_key = LV_KEY_RIGHT;
	}else data->state = LV_BTN_STATE_REL;

	data->key = last_key;            /*Ge-> the last pressed or released key*/

	return false; /*No buffering now so no more data read*/
}
static void spinbox_frequency_cb(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Value: %d\n", lv_spinbox_get_value(obj));
    }
    else if(event == LV_EVENT_CLICKED) {
        /*For simple test: Click the spinbox to increment its value*/
        lv_spinbox_increment(obj);
    }
}
static void roller_waveform_cb(lv_obj_t * obj, lv_event_t event){
	if(event == LV_EVENT_VALUE_CHANGED) {
        char buf[32];
        lv_roller_get_selected_str(obj, buf, sizeof(buf));
        printf("Selected month: %s\n", buf);
    }
}
