/**
 * @file ssd1306.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "ssd1306.h"
#include "driver/i2c.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "SSD1306"

// Code from https://github.com/yanbe/ssd1306-esp-idf-i2c.git is used as a starting point,
// in addition to code from https://github.com/espressif/esp-iot-solution.
// Following definitions are borrowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html
// For LittlevGL the forum has been used, in particular: https://blog.littlevgl.com/2019-05-06/oled

// SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define OLED_I2C_ADDRESS                    0x3C
#define OLED_WIDTH                          128
#define OLED_HEIGHT                         64
#define OLED_COLUMNS                        128
#define OLED_PAGES                          8
#define OLED_PIXEL_PER_PAGE                 8

// Control byte
#define OLED_CONTROL_BYTE_CMD_SINGLE        0x80
#define OLED_CONTROL_BYTE_CMD_STREAM        0x00
#define OLED_CONTROL_BYTE_DATA_STREAM       0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST               0x81    // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM                0xA4
#define OLED_CMD_DISPLAY_ALLON              0xA5
#define OLED_CMD_DISPLAY_NORMAL             0xA6
#define OLED_CMD_DISPLAY_INVERTED           0xA7
#define OLED_CMD_DISPLAY_OFF                0xAE
#define OLED_CMD_DISPLAY_ON                 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE       0x20    // follow with 0x00 = HORZ mode
#define OLED_CMD_SET_COLUMN_RANGE           0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define OLED_CMD_SET_PAGE_RANGE             0x22    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Something something maps ram to display differently - in
// practice, flips the bitmapping vertically - inverting this
// will write right-to-left instead of left-to-right.
#define OLED_CMD_SET_SEGMENT_REMAP_INVERSE  0xA1
#define OLED_CMD_SET_SEGMENT_REMAP_NORMAL   0xA0

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE     0x40
#define OLED_CMD_SET_MUX_RATIO              0xA8    // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE_NORMAL   0xC0
#define OLED_CMD_SET_COM_SCAN_MODE_REVERSE  0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET         0xD3    // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP            0xDA    // follow with 0x12
#define OLED_CMD_NOP                        0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV        0xD5    // follow with 0x80
#define OLED_CMD_SET_PRECHARGE              0xD9    // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT          0xDB    // follow with 0x30

// DC-DC Control. Follow this with 0x8A for off and 0x8B for on.
#define OLED_CMD_SET_CHARGE_PUMP_CTRL       0xAD
#define OLED_CMD_SET_CHARGE_PUMP_ON         0x0B
#define OLED_CMD_SET_CHARGE_PUMP_OFF        0x0A

#define OLED_IIC_FREQ_HZ                   400000  // I2C colock frequency

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

#define BIT_SET(a,b) ((a) |= (1U<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1U<<(b)))

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode               = I2C_MODE_MASTER,
		.sda_io_num         = SSD1306_SDA,
		.scl_io_num         = SSD1306_SCL,
		.sda_pullup_en      = GPIO_PULLUP_ENABLE,
		.scl_pullup_en      = GPIO_PULLUP_ENABLE,
		.master.clk_speed   = OLED_IIC_FREQ_HZ
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void ssd1306_init()
{
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP_CTRL, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP_ON, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP_INVERSE, true); // reverse left-right mapping
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE_REVERSE, true); // reverse up-bottom mapping

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);

	i2c_master_write_byte(cmd, 0x00, true); // reset column low bits
	i2c_master_write_byte(cmd, 0x10, true); // reset column high bits
	i2c_master_write_byte(cmd, 0xB0, true); // reset page
    i2c_master_write_byte(cmd, 0x40, true); // set start line
    i2c_master_write_byte(cmd, OLED_CMD_SET_DISPLAY_OFFSET, true);
    i2c_master_write_byte(cmd, 0x00, true);

// #if defined CONFIG_LVGL_INVERT_DISPLAY
// 	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_INVERTED, true); // Inverted display
// #else
// 	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_NORMAL, true); // Non-inverted display
// 	#endif

	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "OLED configured successfully");
	} else {
		ESP_LOGE(TAG, "OLED configuration failed. code: 0x%.2X", ret);
	}
	i2c_cmd_link_delete(cmd);
}

void ssd1306_set_px_cb(struct _disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
        lv_color_t color, lv_opa_t opa) {
    uint16_t byte_index = x + (( y>>3 ) * buf_w);
    uint8_t  bit_index  = y & 0x7;

    if ( color.full == 0 ) {
        BIT_SET(buf[byte_index], bit_index);
    } else {
        BIT_CLEAR(buf[byte_index], bit_index);
    }
}

void ssd1306_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
	i2c_cmd_handle_t cmd;

	for (uint8_t cur_page = 0; cur_page < 8; cur_page++){
		// vTaskDelay(20/portTICK_PERIOD_MS);

		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
		i2c_master_write_byte(cmd, 0xB0 | cur_page, true); // reset page

		// Set lower column Address between 0x00  - 0x0F
		// An offset of 2 column exists on both sides of screen (left and right)
		// 8 byte column Address is divided in two halves lower and higher half bytes
		// 0x02 lower half and 0x10 upperis first column
		i2c_master_write_byte(cmd, 0x02, true); // Lower half byte
		i2c_master_write_byte(cmd, 0x10, true); // Higher half byte
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);

		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
		i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);

		for (uint8_t column = 2; column <= 0x81;column++){

			i2c_master_write_byte(cmd, (int) color_p->full, true);
			color_p++;

		}
		i2c_master_stop(cmd);
		i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
		i2c_cmd_link_delete(cmd);


	}
    // uint8_t row1 = 0, row2 = 0;
   	// i2c_cmd_handle_t cmd;
	//
	// cmd = i2c_cmd_link_create();
	// i2c_master_start(cmd);
	// i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	//
	// i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	//
	// i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, true);
	// i2c_master_write_byte(cmd, 0x00, true);
	// i2c_master_write_byte(cmd, OLED_CMD_SET_COLUMN_RANGE, true);
	// i2c_master_write_byte(cmd, area->x1, true);
	// i2c_master_write_byte(cmd, area->x2, true);
	// i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_RANGE, true);
	// i2c_master_write_byte(cmd, row1, true);
	// i2c_master_write_byte(cmd, row2, true);
	// i2c_master_stop(cmd);
	// i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	// i2c_cmd_link_delete(cmd);
	//
	// cmd = i2c_cmd_link_create();
	// i2c_master_start(cmd);
	// i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	//
	// i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
	// i2c_master_write(cmd, (uint8_t *)color_p, OLED_COLUMNS * (1+row2-row1), true);
	// i2c_master_stop(cmd);
	// i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	// i2c_cmd_link_delete(cmd);

    lv_disp_flush_ready(disp_drv);
}

void ssd1306_rounder(struct _disp_drv_t * disp_drv, lv_area_t *area)
{
  area->y1 = (area->y1 & (~0x7));
  area->y2 = (area->y2 & (~0x7)) + 7;
}

void ssd1306_sleep_in()
{
    esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_OFF, true);
	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "ssd1306_display_off configuration failed. code: 0x%.2X", ret);
	}
	i2c_cmd_link_delete(cmd);
}

void ssd1306_sleep_out()
{
    esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "ssd1306_display_on configuration failed. code: 0x%.2X", ret);
	}
	i2c_cmd_link_delete(cmd);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
