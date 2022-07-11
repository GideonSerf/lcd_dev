#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "lcd_dev.h"
#include "esp_err.h"
#include "driver/spi_master.h"
void lcd_spi_pre_callback(spi_transaction_t *trans);

#define LCD_WIDTH 160
#define LCD_HEIGHT 128

typedef struct
{
	int ds_pin;
	uint8_t level;
} user_vals;

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t lcd_init_cmds[]={
	{0xb1,{0x01,0x2c,0x2d},3},
	{0xB2,{0x01,0x2c,0x2d},3},
	{0xB3,{0x01,0x2c,0x2d,0x01,0x2c,0x2d},6},
	{0xB4,{0x07},1},
	{0xC0, {0xA2,0x02,0x84}, 3},
	{0xC1,{0xC5},1},
	{0xC2, {0x0A, 0x00}, 2},
	{0xC3, {0x8a,0x2a}, 2},
	{0xC4, {0x8a,0xee}, 2},
	{0xC5, {0x0E}, 1},

	{0xE0, {0x0f, 0x1a, 0x0f, 0x18, 0x2f, 0x28, 0x20, 0x22, 0x1f, 0x1b, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10}, 16},
	{0xE1, {0x0f, 0x1b, 0x0f, 0x17, 0x33, 0x2c, 0x29, 0x2e, 0x30, 0x30, 0x39, 0x3f, 0x00, 0x07, 0x03, 0x10}, 16},

	{0xF0, {0x01}, 1},
	{0xF6, {0x00}, 1},
	{0x3A, {0x05}, 1},
    {0x36, {0x60}, 1},
	{0x11, {0}, 0x80},
	{0x29, {0}, 0x80},

    {0, {0}, 0xff}
};

void lcd_init(lcd_t* lcds)
{
	esp_err_t err;

	//Initialise non-SPI pins
	gpio_reset_pin(lcds->bl);
	gpio_reset_pin(lcds->ds);
	gpio_reset_pin(lcds->rst);

	gpio_set_direction(lcds->bl, GPIO_MODE_OUTPUT);
	gpio_set_direction(lcds->rst, GPIO_MODE_OUTPUT);
	gpio_set_direction(lcds->ds, GPIO_MODE_OUTPUT);
	lcds->bl_state=true;
	gpio_set_level(lcds->bl, 1);
	gpio_set_level(lcds->rst, 1);
	spi_bus_config_t buscfg={
	        .miso_io_num=-1,
	        .mosi_io_num=lcds->din,
	        .sclk_io_num=lcds->clk,
	        .quadwp_io_num=-1,
	        .quadhd_io_num=-1,
	        .max_transfer_sz=lcds->parallel_lines*160*2+8
	    };

	spi_device_interface_config_t devcfg={
			.clock_speed_hz=12.5*1000*1000,
			.mode=0,
			.spics_io_num=lcds->cs,
			.queue_size=7,
			.pre_cb=lcd_spi_pre_callback
	};

	err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(err);

	err = spi_bus_add_device(SPI2_HOST,&devcfg,&(lcds->spi));
	ESP_ERROR_CHECK(err);

	int cmd=0;
	while (lcd_init_cmds[cmd].databytes!=0xff) {
	        lcd_cmd(lcds, lcd_init_cmds[cmd].cmd, false);
	        lcd_data(lcds, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
	        if (lcd_init_cmds[cmd].databytes&0x80) {
	            vTaskDelay(100 / portTICK_PERIOD_MS);
	        }
	        cmd++;
	    }
}

void lcd_cmd(lcd_t* lcds,const uint8_t cmd, bool keep_cs_active)
{
	esp_err_t err;
	spi_transaction_t t;
	memset(&t,0,sizeof(t));
	t.length=8;
	t.tx_buffer=&cmd;

	user_vals u={.ds_pin=lcds->ds,.level=0};

	t.user = (void*)(&u);

	if(keep_cs_active)
	{
		t.flags=SPI_TRANS_CS_KEEP_ACTIVE;
	}

	err = spi_device_polling_transmit(lcds->spi,&t);
	assert(err==ESP_OK);
}

void lcd_data(lcd_t* lcds, const uint8_t *data, int len)
{
	esp_err_t err;
	spi_transaction_t t;
	if(len==0) return;
	memset(&t,0,sizeof(t));
	t.length=len*8;
	t.tx_buffer=data;

	user_vals u={.ds_pin=lcds->ds,.level=1};

	t.user = &u;

	err = spi_device_polling_transmit(lcds->spi,&t);
	assert(err==ESP_OK);
}

void lcd_spi_pre_callback(spi_transaction_t *trans)
{
	user_vals* v = (user_vals*)(trans->user);
	gpio_set_level(v->ds_pin, v->level);
}

void lcd_set_cursor(lcd_t* lcds, uint8_t Xstart,uint8_t Ystart, uint8_t Xend, uint8_t Yend)
{
	uint8_t dat1[]={0,(Xstart&0xff)+1,0,((Xend)&0xff)+1};
	uint8_t dat2[]={0,(Ystart&0xff)+2,0,((Yend)&0xff)+2};

	lcd_cmd(lcds,0x2a,false);


	lcd_data(lcds,dat1,4);

	lcd_cmd(lcds,0x2b,false);


		lcd_data(lcds,dat2,4);

	lcd_cmd(lcds,0x2C,false);
}

void lcd_clear(lcd_t* lcds,uint16_t color)
{
	lcd_set_cursor(lcds,0, 0, LCD_WIDTH - 1, LCD_HEIGHT-1);

	int i,j;

	uint8_t linedata[lcds->parallel_lines* 2*LCD_WIDTH];
	for(i=0;i<LCD_WIDTH;i++)
	{

		for(j=0;j<lcds->parallel_lines;j++)
		{
		linedata[j*2*LCD_WIDTH+i*2]=(uint8_t)(color>>8);
		linedata[j*2*LCD_WIDTH+i*2 + 1] = (uint8_t)(color&0xff);
		}
	}


	    for (j = 0; j < LCD_HEIGHT/lcds->parallel_lines; j++) {
	    	lcd_data(lcds,linedata,lcds->parallel_lines*2*LCD_WIDTH);
	    }
}

void lcd_draw_rect(lcd_t* lcds, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color)
{
	int data_len,j;

	if(w*h*2>=lcds->parallel_lines*160*2+8)
	{
		data_len = w*h;
		j=1;
	}
	else if(w*h>=lcds->parallel_lines*160*2+8)
	{
		data_len=w*h/2;
		j=2;
	}
	else
	{
		data_len=w;
		j=h;
	}
	uint8_t linedata[data_len*2];
	for(int i=0;i<data_len;i++)
	{
		linedata[i*2] = (uint8_t)(color>>8);
		linedata[i*2+1] = (uint8_t)(color&0xff);
	}
	lcd_set_cursor(lcds,x, y,x+ w-1,y+ h-1);
	for(int i=0;i<j;i++)
	{
		lcd_data(lcds,linedata,2*data_len);
	}
}
