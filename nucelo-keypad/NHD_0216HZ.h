/*----------------------------------------------------------------------------
 Definitions header file for Newhaven NHD0216HZ LCD
 *----------------------------------------------------------------------------*/
#include "mbed.h"

#ifndef NHD_0216HZ_H
#define NHD_0216HZ_H

//Define constants
#define ENABLE 0x08
#define DATA_MODE 0x04
#define COMMAND_MODE 0x00

#define LINE_LENGTH 0x40
#define TOT_LENGTH 0x80
		
	//Function prototypes
	void write_cmd(SPI *spi, int data);
	void write_data(SPI *spi, char c);
	void write_4bit(SPI *spi, int nibble, int mode);
	void init_spi(SPI *spi);
	void init_lcd(SPI *spi);
    void clear_lcd(SPI *spi);
	void set_cursor(SPI *spi, int column, int row);
	void print_lcd(SPI *spi, const char *string);

#endif

// *******************************ARM University Program Copyright (c) ARM Ltd 2019*************************************
