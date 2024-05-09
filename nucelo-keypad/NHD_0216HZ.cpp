/*----------------------------------------------------------------------------
 Newhaven NHD0216HZ LCD C/C++ file
 *----------------------------------------------------------------------------*/

#include "NHD_0216HZ.h"


DigitalOut SS(D9);     //slave select a.k.a. cs or latch for shift reg

//Initialise SPI
void init_spi(SPI *spi) {
    SS = 1;
    
    spi->format(8, 3);         //8bit spi->mode 2
    spi->frequency(100000);    //100 kHz spi->clock
}

//Initialise LCD
void init_lcd(SPI *spi) {
    /*
    Implement the LCD initialisation using the information
    from the ST7066U LCD driver datasheet (pages 25-26)
    */
    //Write your code here
    ThisThread::sleep_for(40ms);
    // Function set (interface to 8-bit, 2 lines, 5x8 dots)
    write_4bit(spi, 0x30, COMMAND_MODE); // First part of function set
    ThisThread::sleep_for(5ms); // Wait more than 4.1ms
    write_4bit(spi, 0x30, COMMAND_MODE); // Second part of function set
    ThisThread::sleep_for(1ms); // Wait more than 100us
    write_4bit(spi, 0x30, COMMAND_MODE); // Third part of function set
    write_4bit(spi, 0x20, COMMAND_MODE); // Set to 4-bit interface
    ThisThread::sleep_for(1ms);
    // Function Set: 4-bit interface, 2 lines, 5x8 font
    write_cmd(spi, 0x28); // 4-bit mode, 2 lines, 5x8 font
    ThisThread::sleep_for(1ms);
    // Display ON/OFF control: display ON, cursor OFF, blink cursor OFF
    write_cmd(spi, 0x0C);
    ThisThread::sleep_for(1ms);
    // Clear Display
    clear_lcd(spi);
    // Entry Mode Set: Increment cursor, no display shift
    write_cmd(spi, 0x06);
    ThisThread::sleep_for(1ms);
}

void clear_lcd(SPI *spi)
{
    // Clear Display
    write_cmd(spi, 0x01);
    ThisThread::sleep_for(2ms); // Clear command needs up to 1.64ms
}

//Write 4bits to the LCD
void write_4bit(SPI *spi, int nibble, int mode) {
    SS = 0;
    spi->write(nibble | ENABLE | mode);
    SS = 1;
    wait_us(1);
    SS = 0;
    spi->write(nibble & ~ENABLE);
    SS = 1;
}

//Write a command to the LCD
void write_cmd(SPI *spi, int data) {
    int hi_n;
    int lo_n;
    
    hi_n = hi_n = (data & 0xF0);
    lo_n = ((data << 4) &0xF0);
    
    write_4bit(spi, hi_n, COMMAND_MODE);
    write_4bit(spi, lo_n, COMMAND_MODE);
}

//Write data to the LCD
void write_data(SPI *spi, char c) {
    int hi_n;
    int lo_n;
    
    hi_n = hi_n = (c & 0xF0);
    lo_n = ((c << 4) &0xF0);
    
    write_4bit(spi, hi_n, DATA_MODE);
    write_4bit(spi, lo_n, DATA_MODE);
}

//Set cursor position
void set_cursor(SPI *spi, int column, int row) {
    int addr;
    
    addr = (row * LINE_LENGTH) + column;
    addr |= TOT_LENGTH;
    write_cmd(spi, addr);
}

//Print strings to the LCD
void print_lcd(SPI *spi, const char *string) {
    while(*string){
        write_data(spi, *string++);
    }
}

// *******************************ARM University Program Copyright (c) ARM Ltd 2014*************************************
