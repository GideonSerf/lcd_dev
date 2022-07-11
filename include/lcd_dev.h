#include <stdlib.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"

#define RGB(r,g,b) (((r&0b11111)<<11)|((g&0b111111)<<5)|((b&0b11111)))

typedef struct
		{
			gpio_num_t din,clk,cs,ds,rst,bl;
			uint32_t parallel_lines;
			bool bl_state;
			spi_device_handle_t spi;
		} lcd_t;

typedef struct {
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

void lcd_init(lcd_t* lcds);

void lcd_cmd(lcd_t* lcds,const uint8_t cmd, bool keep_cs_active);

void lcd_data(lcd_t* lcds, const uint8_t *data, int len);

void lcd_set_cursor(lcd_t* lcds, uint8_t Xstart,uint8_t Ystart, uint8_t Xend, uint8_t Yend);

void lcd_clear(lcd_t* lcds,uint16_t color);

void lcd_draw_rect(lcd_t* lcds, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);
