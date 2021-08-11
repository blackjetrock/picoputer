////////////////////////////////////////////////////////////////////////////////
//
// Runs on Mega Embedded and monitors 8 data bits and the two control
// bits from an INMOS C011 link adapter IC.
//
//

#define OLED_TRAFFIC 0
#define OLED_SCROLL  0

const int SDA_PIN    = 32;
const int SCL_PIN    = 33;

// Control lines
const int RESET_PIN  = 34;

// I control lines
const int IValid_PIN  = 35;
const int IAck_PIN    = 36;

// Q Control lines
const int QValid_PIN  = 37;
const int QAck_PIN    = 38;

const int Q0_PIN    = 8;
const int Q1_PIN    = 9;
const int Q2_PIN    = 10;
const int Q3_PIN    = 11;
const int Q4_PIN    = 12;
const int Q5_PIN    = 13;
const int Q6_PIN    = 14;
const int Q7_PIN    = 15;

const int I0_PIN    = 18;
const int I1_PIN    = 19;
const int I2_PIN    = 20;
const int I3_PIN    = 21;
const int I4_PIN    = 22;
const int I5_PIN    = 23;
const int I6_PIN    = 24;
const int I7_PIN    = 25;



/*
 * i2c_functions.h
 *
 *  Created on: 23 Jul 2016
 *      Author: menadue
 */

#ifndef I2C_FUNCTIONS_H_
#define I2C_FUNCTIONS_H_

typedef unsigned char BYTE;

//--------------------------------------------------------------------------------

// I2C Port descriptor
typedef struct _I2C_PORT_DESC
{
  unsigned char sdaport;
  unsigned char sdabit;
  unsigned char sclport;
  unsigned char sclbit;
} I2C_PORT_DESC;

// Slave device descriptor
typedef struct _I2C_SLAVE_DESC
{
  I2C_PORT_DESC *port;             // Port the device is on
  unsigned char slave_7bit_addr;        // SLave address
} I2C_SLAVE_DESC;

// I2C functions
void i2c_release(I2C_PORT_DESC *port);
void i2c_delay(I2C_PORT_DESC *port);
void i2c_sda_low(I2C_PORT_DESC *port);
void i2c_sda_high(I2C_PORT_DESC *port);
void i2c_scl_low(I2C_PORT_DESC *port);
void i2c_scl_high(I2C_PORT_DESC *port);
void i2c_start(I2C_PORT_DESC *port);
void i2c_stop(I2C_PORT_DESC *port);
int i2c_send_byte(I2C_PORT_DESC *port, BYTE b);
int i2c_read_bytes(I2C_SLAVE_DESC *slave, int n, BYTE *data);
void i2c_send_bytes(I2C_SLAVE_DESC *slave, int n, BYTE *data);
#

#endif /* I2C_FUNCTIONS_H_ */


/*
 * i2c_functions.c
 *
 *  Created on: 23 Jul 2016
 *      Author: menadue
 */
//#include "chip.h"
//#include "board.h"
//#include "i2c_functions.h"


// I2C functions

// Release the bus
void i2c_release(I2C_PORT_DESC *port) {
  // All inputs
  pinMode(SDA_PIN,INPUT);
  pinMode(SCL_PIN,INPUT);

  //  Chip_GPIO_SetPinDIRInput(LPC_GPIO, port->sdaport, port->sdabit);
  //Chip_GPIO_SetPinDIRInput(LPC_GPIO, port->sclport, port->sclbit);
}

// Delay to slow down to I2C bus rates
void i2c_delay(I2C_PORT_DESC *port) {
  
  //delay(1);
#if 0  
  sleep_us(10);
  return;
  volatile int i;
  for(i=0; i<1; i++)
    {
    }
#endif
}

void i2c_sda_low(I2C_PORT_DESC *port) {
  // Take SCL low by driving a 0 on to the bus
  digitalWrite(SDA_PIN, LOW);
  pinMode(SDA_PIN,OUTPUT);
  //Chip_GPIO_SetPinState(LPC_GPIO, port->sdaport, port->sdabit, 0);
  
  // Make sure bit is an output
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, port->sdaport, port->sdabit);
}

void i2c_sda_high(I2C_PORT_DESC *port) {
  // Make sure bit is an intput
  pinMode(SDA_PIN,INPUT);

  //  Chip_GPIO_SetPinDIRInput(LPC_GPIO, port->sdaport, port->sdabit);
}

void i2c_scl_low(I2C_PORT_DESC *port) {

  digitalWrite(SCL_PIN, LOW);
  pinMode(SCL_PIN,OUTPUT);

  //  Chip_GPIO_SetPinState(LPC_GPIO, port->sclport, port->sclbit, 0);

  // Make sure bit is an output
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, port->sclport, port->sclbit);
  }

void i2c_scl_high(I2C_PORT_DESC *port)
{
  // Make sure bit is an input
  pinMode(SCL_PIN,INPUT);
  //Chip_GPIO_SetPinDIRInput(LPC_GPIO, port->sclport, port->sclbit);
}

// Read ACK bit

int i2c_read_sda(I2C_PORT_DESC *port)
{
  return(digitalRead(SDA_PIN));
  //  return(Chip_GPIO_GetPinState(LPC_GPIO, port->sdaport, port->sdabit));

  //  return( ((*(port->port)) & (port->sda_mask)) == port->sda_mask);
}

// I2C start condition

void i2c_start(I2C_PORT_DESC *port)
{
  //
  i2c_sda_low(port);

  i2c_delay(port);
  i2c_scl_low(port);
  i2c_delay(port);
}

void i2c_stop(I2C_PORT_DESC *port)
{
  i2c_scl_high(port);
  i2c_delay(port);
  i2c_sda_high(port);
  i2c_delay(port);

}

// Send 8 bits and read ACK
// Returns number of acks received

int i2c_send_byte(I2C_PORT_DESC *port, BYTE b)
{
  int i;
  int ack=0;
  int retries = 100;
  int rc =1;

  for (i = 0; i < 8; i++)
    {
      // Set up data
      if ((b & 0x80) == 0x80)
	{
	  i2c_sda_high(port);
	} else
	{
	  i2c_sda_low(port);
	}

      // Delay
      i2c_delay(port);

      // Take clock high and then low
      i2c_scl_high(port);

      // Delay
      i2c_delay(port);

      // clock low again
      i2c_scl_low(port);

      // Delay
      i2c_delay(port);

      // Shift next data bit in
      b <<= 1;
    }

  // release data line
  i2c_sda_high(port);

  // Now get ACK
  i2c_scl_high(port);

  i2c_delay(port);

  // read ACK

  while( ack = (i2c_read_sda(port)==HIGH) ) // @suppress("Assignment in condition")
    {
      retries--;

      if ( retries == 0 )
	{
	  rc = 0;
	  break;
	}
    }

  i2c_scl_low(port);

  i2c_delay(port);
  return (rc);
}


// Receive 8 bits and set ACK
// Ack as specified
void i2c_recv_byte(I2C_PORT_DESC *port, BYTE *data, int ack)
{
  int i, b;

  b = 0;

  // Make data an input
  i2c_sda_high(port);

  for (i = 0; i < 8; i++)
    {
      // Delay
      i2c_delay(port);

      // Take clock high and then low
      i2c_scl_high(port);

      // Delay
      i2c_delay(port);

      // Shift next data bit in
      b <<= 1;
      b += (i2c_read_sda(port) & 0x1);

      // clock low again
      i2c_scl_low(port);

      // Delay
      i2c_delay(port);

    }

  // ACK is as we are told 
  if ( ack )
    {
      // Data low for ACK
      i2c_sda_low(port);
    }
  else
    {
      i2c_sda_high(port);
    }

  // Now send ACK
  i2c_scl_high(port);

  i2c_delay(port);

  i2c_scl_low(port);

  i2c_delay(port);

  *data = b;

}

// Reads a block of bytes from a slave

int i2c_read_bytes(I2C_SLAVE_DESC *slave, int n, BYTE *data)
{
  int i;
  BYTE byte;

  i2c_start(slave->port);

  // Send slave address with read bit
  if ( !i2c_send_byte(slave->port, ((slave->slave_7bit_addr) << 1) | 1) )
    {
      i2c_stop(slave->port);
      return(0);
    }


  //
  for (i = 0; i < n; i++)
    {
      i2c_recv_byte(slave->port, &byte, (i==(n-1))? 0 : 1);
      *(data++) = byte;
    }

  i2c_stop(slave->port);

  return(1);
}

// Sends a block of data to I2C slave
void i2c_send_bytes(I2C_SLAVE_DESC *slave, int n, BYTE *data)
{
  int i;

  i2c_start(slave->port);

  // Send slave address with read bit
  i2c_send_byte(slave->port, ((slave->slave_7bit_addr) << 1) | 0);

  //
  for (i = 0; i < n; i++)
    {
      i2c_send_byte(slave->port, *(data++));
    }

  i2c_stop(slave->port);
}

void i2c_init(I2C_PORT_DESC *port)
{
  pinMode(SDA_PIN, INPUT);
  pinMode(SCL_PIN, INPUT);
  
  //	gpio_init(SDA_PIN);
  //	gpio_init(SCL_PIN);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 19);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 2);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 18);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 0, 7);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 19);
  //Chip_GPIO_SetPinDIROutput(LPC_GPIO, 1, 20);
}

/*
 * oled096.h
 *
 *  Created on: 23 Jul 2016
 *      Author: menadue
 */

#ifndef OLED096_H_
#define OLED096_H_

#define I2C_CMD       1
#define I2C_DATA      0
#define I2C_REPEAT    1
#define I2C_NO_REPEAT 0

void oled_send_cmd(I2C_SLAVE_DESC *slave, int n, unsigned char *data, int command, int repeat);
void oled_set_xy(I2C_SLAVE_DESC *slave, int x, int y);
void oled_set_pixel_xy(I2C_SLAVE_DESC *slave, int x, int y);
void oled_set_byte_xy(I2C_SLAVE_DESC *slave, int x, int y, int b);
void oled_gap(I2C_SLAVE_DESC *slave);
void oled_display_int(I2C_SLAVE_DESC *slave, long int n, int num_digits);
void oled_display_string(I2C_SLAVE_DESC *slave, char *string);
void oled_clear_display(I2C_SLAVE_DESC *slave);

void oled_setup(I2C_SLAVE_DESC *slave);
void oled_display_scaled_string_xy(I2C_SLAVE_DESC *slave, char *string, int x, int y, int scale);

extern const unsigned char font_5x7_letters[];

#endif /* OLED096_H_ */

/*
 * oled096.c
 *
 *  Created on: 23 Jul 2016
 *      Author: menadue
 */

//
// 0.96" OLED display functions
//


// Sends a block of commands to the SSD1306
// command causes 80 to be sent before each byte
// command = 0 causes 0x40 and just data bytes
// Repeat = 1, repeat data byte 0

//#include <string.h>
//#include "i2c_functions.h"
//#include "oled096.h"

void oled_send_cmd(I2C_SLAVE_DESC *slave, int n, unsigned char *data, int command, int repeat)
{
  int i;

  i2c_start(slave->port);

  // Send slave address
  i2c_send_byte(slave->port, (slave->slave_7bit_addr)<<1);

  if ( !command )
    {
      i2c_send_byte(slave->port, 0x40);
    }

  // Send the command bytes, each preceded bu 0x80 (continuation)
  for (i = 0; i < n; i++) {
    if ( command )
      {
        i2c_send_byte(slave->port, 0x80);
      }
    i2c_send_byte(slave->port, *data);

    if ( !repeat )
      {
	data++;
      }
  }

  i2c_stop(slave->port);
}

//--------------------------------------------------------------------------------
// OLED display functions
//

const unsigned char init_seq[] = {
  0xae,                       // ???
  0xd5, 0x80,

  0xa8,0x3f,

  0xd3,0x00,
  0x8d,0x14,0x40,0xa6,0xa4,0xa1,0xc8,0xda,0x12,0x81,0x8f,
  0xd9,         //??

  0xf1,0xdb,0x40,0xaf,
};

// From data sheet
const unsigned char init_seq2[] = {
  //  0xae,          // Display Off
  0xa8,0x3f,       // MUX ratio works for now
  0xd3,0x00,       // Set display offset 0
  0x40,            // Set display start Line
  0x8d,0x14,       // Charge pump ON, needed or blank display
  0xa1,            // Seg remap 180 rotate
  0xc8,            // Inverted COM scan
  0xda, 0x12,      // COM config: A5:Disable left/right remap, A4:Alternate COM pin config
  0x81,0x2f,      // Contrast value
  0xd9, 0xf1,     // Precharge, quite important
  0xdb, 0x40,     // Set Vcomh level, leave it out and inverted display.

  0xa6,            // Normal display (not inverted)
  0xd5,0x80,       // Set display oscillator

  0x20, 0x02,        // Page addressing mode
  0x21,0x00,0x7f,               // Set display RAM start and end address
                                // seems to have effect in page addressing mode.
  0x22,0x00,0x07,               // Set display page address

  0xa4,
  0xaf,             // Display ON
};

const unsigned char display_setup_seq[] = {
  0x21,0x00,0x7f,               // Set display RAM start and end address
  0x22,0x00,0x3f,               // Set display page address
};


const unsigned char display_text_seq[] = {
  0x7e,0x11,0x11,0x11,0x7e,0x00,0x7f,0x49,0x49,0x49,0x36,0x00,0x00,0x3e,0x41,0x41,0x41,0x22,0x00,0x7f,0x41,0x41,0x41,0x22,0x1c,0x00,0x7f,0x49,0x49,0x49,0x41,0x00,
};

// Set XY to given position
// We attempt to position to the byte that holds the pixel (x,y)
// Page addressing mode

void oled_set_xy(I2C_SLAVE_DESC *slave, int x, int y)
{
  unsigned char seq[3];

  x = x % 128;
  y = y % 64;
  seq[0] = 0xB0+y/8;      // Set page
  seq[1] = 0x00+x%16;     // Set low part of start address
  seq[2] = 0x10+x/16;     // Set high part

  oled_send_cmd(slave, sizeof(seq), &(seq[0]), I2C_CMD, I2C_NO_REPEAT);
}

unsigned char byteval;

// Writes bit pattern at x,y
void oled_set_pixel_xy(I2C_SLAVE_DESC *slave, int x, int y)

{
  byteval = 1<< (y % 8);

  oled_set_xy(slave, x, y);
  oled_send_cmd(slave, 1, &byteval, I2C_DATA, I2C_NO_REPEAT);
}

void oled_set_byte_xy(I2C_SLAVE_DESC *slave, int x, int y, int b)
{
  byteval = b;

  oled_set_xy(slave, x, y);
  oled_send_cmd(slave, 1, &byteval, I2C_DATA, I2C_NO_REPEAT);
}


// Font

const unsigned char font_5x7_letters[] = {
  0x00, 0x00, 0x00, 0x00, 0x00,// (space)
  0x00, 0x00, 0x5F, 0x00, 0x00,// !
  0x00, 0x07, 0x00, 0x07, 0x00,// "
  0x14, 0x7F, 0x14, 0x7F, 0x14,// #
  0x24, 0x2A, 0x7F, 0x2A, 0x12,// $
  0x23, 0x13, 0x08, 0x64, 0x62,// %
  0x36, 0x49, 0x55, 0x22, 0x50,// &
  0x00, 0x05, 0x03, 0x00, 0x00,// '
  0x00, 0x1C, 0x22, 0x41, 0x00,// (
  0x00, 0x41, 0x22, 0x1C, 0x00,// )
  0x08, 0x2A, 0x1C, 0x2A, 0x08,// *
  0x08, 0x08, 0x3E, 0x08, 0x08,// +
  0x00, 0x50, 0x30, 0x00, 0x00,// ,
  0x08, 0x08, 0x08, 0x08, 0x08,// -
  0x00, 0x60, 0x60, 0x00, 0x00,// .
  0x20, 0x10, 0x08, 0x04, 0x02,// /
  0x3E, 0x51, 0x49, 0x45, 0x3E,// 0
  0x00, 0x42, 0x7F, 0x40, 0x00,// 1
  0x42, 0x61, 0x51, 0x49, 0x46,// 2
  0x21, 0x41, 0x45, 0x4B, 0x31,// 3
  0x18, 0x14, 0x12, 0x7F, 0x10,// 4
  0x27, 0x45, 0x45, 0x45, 0x39,// 5
  0x3C, 0x4A, 0x49, 0x49, 0x30,// 6
  0x01, 0x71, 0x09, 0x05, 0x03,// 7
  0x36, 0x49, 0x49, 0x49, 0x36,// 8
  0x06, 0x49, 0x49, 0x29, 0x1E,// 9
  0x00, 0x36, 0x36, 0x00, 0x00,// :
  0x00, 0x56, 0x36, 0x00, 0x00,// ;
  0x00, 0x08, 0x14, 0x22, 0x41,// <
  0x14, 0x14, 0x14, 0x14, 0x14,// =
  0x41, 0x22, 0x14, 0x08, 0x00,// >
  0x02, 0x01, 0x51, 0x09, 0x06,// ?
  0x32, 0x49, 0x79, 0x41, 0x3E,// @
  0x7E, 0x11, 0x11, 0x11, 0x7E,// A
  0x7F, 0x49, 0x49, 0x49, 0x36,// B
  0x3E, 0x41, 0x41, 0x41, 0x22,// C
  0x7F, 0x41, 0x41, 0x22, 0x1C,// D
  0x7F, 0x49, 0x49, 0x49, 0x41,// E
  0x7F, 0x09, 0x09, 0x01, 0x01,// F
  0x3E, 0x41, 0x41, 0x51, 0x32,// G
  0x7F, 0x08, 0x08, 0x08, 0x7F,// H
  0x00, 0x41, 0x7F, 0x41, 0x00,// I
  0x20, 0x40, 0x41, 0x3F, 0x01,// J
  0x7F, 0x08, 0x14, 0x22, 0x41,// K
  0x7F, 0x40, 0x40, 0x40, 0x40,// L
  0x7F, 0x02, 0x04, 0x02, 0x7F,// M
  0x7F, 0x04, 0x08, 0x10, 0x7F,// N
  0x3E, 0x41, 0x41, 0x41, 0x3E,// O
  0x7F, 0x09, 0x09, 0x09, 0x06,// P
  0x3E, 0x41, 0x51, 0x21, 0x5E,// Q
  0x7F, 0x09, 0x19, 0x29, 0x46,// R
  0x46, 0x49, 0x49, 0x49, 0x31,// S
  0x01, 0x01, 0x7F, 0x01, 0x01,// T
  0x3F, 0x40, 0x40, 0x40, 0x3F,// U
  0x1F, 0x20, 0x40, 0x20, 0x1F,// V
  0x7F, 0x20, 0x18, 0x20, 0x7F,// W
  0x63, 0x14, 0x08, 0x14, 0x63,// X
  0x03, 0x04, 0x78, 0x04, 0x03,// Y
  0x61, 0x51, 0x49, 0x45, 0x43,// Z
  0x00, 0x00, 0x7F, 0x41, 0x41,// [
  0x02, 0x04, 0x08, 0x10, 0x20,// "\"
  0x41, 0x41, 0x7F, 0x00, 0x00,// ]
  0x04, 0x02, 0x01, 0x02, 0x04,// ^
  0x40, 0x40, 0x40, 0x40, 0x40,// _
  0x00, 0x01, 0x02, 0x04, 0x00,// `
  0x20, 0x54, 0x54, 0x54, 0x78,// a
  0x7F, 0x48, 0x44, 0x44, 0x38,// b
  0x38, 0x44, 0x44, 0x44, 0x20,// c
  0x38, 0x44, 0x44, 0x48, 0x7F,// d
  0x38, 0x54, 0x54, 0x54, 0x18,// e
  0x08, 0x7E, 0x09, 0x01, 0x02,// f
  0x08, 0x14, 0x54, 0x54, 0x3C,// g
  0x7F, 0x08, 0x04, 0x04, 0x78,// h
  0x00, 0x44, 0x7D, 0x40, 0x00,// i
  0x20, 0x40, 0x44, 0x3D, 0x00,// j
  0x00, 0x7F, 0x10, 0x28, 0x44,// k
  0x00, 0x41, 0x7F, 0x40, 0x00,// l
  0x7C, 0x04, 0x18, 0x04, 0x78,// m
  0x7C, 0x08, 0x04, 0x04, 0x78,// n
  0x38, 0x44, 0x44, 0x44, 0x38,// o
  0x7C, 0x14, 0x14, 0x14, 0x08,// p
  0x08, 0x14, 0x14, 0x18, 0x7C,// q
  0x7C, 0x08, 0x04, 0x04, 0x08,// r
  0x48, 0x54, 0x54, 0x54, 0x20,// s
  0x04, 0x3F, 0x44, 0x40, 0x20,// t
  0x3C, 0x40, 0x40, 0x20, 0x7C,// u
  0x1C, 0x20, 0x40, 0x20, 0x1C,// v
  0x3C, 0x40, 0x30, 0x40, 0x3C,// w
  0x44, 0x28, 0x10, 0x28, 0x44,// x
  0x0C, 0x50, 0x50, 0x50, 0x3C,// y
  0x44, 0x64, 0x54, 0x4C, 0x44,// z
  0x00, 0x08, 0x36, 0x41, 0x00,// {
  0x00, 0x00, 0x7F, 0x00, 0x00,// |
  0x00, 0x41, 0x36, 0x08, 0x00,// }
  0x08, 0x08, 0x2A, 0x1C, 0x08,// ->
  0x08, 0x1C, 0x2A, 0x08, 0x08 // <-
};

// Turn display off
const unsigned char off_seq = {
  0xae
};

// Turn display on
const unsigned char on_seq = {
  0xaf
};


// Prints a character gap
void oled_gap(I2C_SLAVE_DESC *slave)
{
  unsigned char zero = 0;

  oled_send_cmd(slave, 1, &zero, I2C_DATA, I2C_NO_REPEAT);
}

// Displays an integer in decimal, number of digits displayed is specified, MS digits dropped

void oled_display_int(I2C_SLAVE_DESC *slave, long int n, int num_digits)
{
  int i;
  int dig;
  int mul = 10000;

  if( n < 0 )
    {
      oled_display_string(slave, "-");
      n = -n;
    }
  else
    {
      oled_display_string(slave, " ");
    }

  for(i=5; i>0; i--)
    {
      dig = n / mul;
      n = n % mul;
      mul /=10;

      // Display digit
      if ( i <= num_digits )
	{
	  oled_send_cmd(slave, 5, font_5x7_letters+('0'-' ')*5+dig*5, I2C_DATA, I2C_NO_REPEAT);
	  oled_gap(slave);
	}
    }
}

void oled_display_string(I2C_SLAVE_DESC *slave, char *string)
{
  int j;
  int len = strlen(string);

  for(j=0; j<len; j++)
    {
      oled_send_cmd(slave, 5, font_5x7_letters+((*string++) - ' ')*5, I2C_DATA, I2C_NO_REPEAT);
      oled_gap(slave);
    }
}


void oled_clear_display(I2C_SLAVE_DESC *slave)
{
  int y;
  unsigned char zero = 0;

  for(y=0; y<64; y+=8)
    {
      oled_set_xy(slave, 0, y);
      oled_send_cmd(slave, 128, &zero, I2C_DATA, I2C_REPEAT);
    }

}

//
// Displays a string with pixel scaling
//
unsigned char scaled_line[100];

void oled_display_scaled_string_xy(I2C_SLAVE_DESC *slave, char *string, int x, int y, int scale)
{
  unsigned char i;
  int  j, k, s;
  int scale_i;
  int len = strlen(string);
  int b;

  // We now display the string a character at a time, each character being scaled
  scale_i = 0;

  for(k=0; k<len; k++)
    {
      // Scale character
      // We start with 5 bytes, each of 8 bits, these are vertical slices of the bit pattern
      // We need to stretch the character vertically by scale

      for(i=0; i<5; i++)
	{
	  // For each byte we build a new bitmap that is scaled
	  for(j=0; j<8; j++)
	    {
	      // Get next bit
	      b  = (*(font_5x7_letters+((*string)-' ')*5+i));
	      b &= (1 << (7-j));
	      b >>= (7-j);

	      // Shift more bits in to scaled data
	      for(s=0; s<scale;s++)
		{
		  scaled_line[scale_i/8] <<= 1;
		  scaled_line[scale_i/8] |= b;
		  scale_i++;
		}
	    }
#if 0
	  // Move to next byte
	  while( (scale_i % 8) != 0 )
	    {
	      scale_i++;
	    }
#endif
	}

      // Put a gap between chars
      for(s=0; s<scale;s++)
	{
	  scaled_line[scale_i/8] = 0;
	  scale_i+=8;
	}

      string++;
    }

  // Send scaled data to display
  for(j=0; j < scale; j++)
    {
      oled_set_xy(slave, x, y+8*((scale-1)-j));
      for(i=0; i<5*len*scale; i+=scale)
	{
	  for(s=0; s<scale;s++)
	    {
	      oled_send_cmd(slave, 1, &(scaled_line[i+j]), I2C_DATA, I2C_NO_REPEAT);
	    }
	}
      oled_gap(slave);
    }

#if 0
  oled_set_xy(slave->port, 0,0);
  for(j=0;j<scale_i/8;j++)
    {
      oled_send_cmd(slave, 1, &(scaled_line[j]), I2C_DATA, I2C_NO_REPEAT);
    }


  oled_set_xy(slave, 30, 24);
  oled_display_int(slave, scale_i,5);
  oled_set_xy(slave, 30, 32);
  oled_display_int(slave, len,5);
#endif
}

void oled_setup(I2C_SLAVE_DESC *slave)
{
    int i;

  // Release bus
  i2c_release(slave->port);

  // Delay to allow things to start up.
  for(i=0; i<10000; i++)
    {
    }

  // Then clear any glitched I2C commands with a start and stop
  i2c_start(slave->port);
  i2c_stop(slave->port);

  // Initialise display
  oled_send_cmd(slave, sizeof(init_seq2), &(init_seq2[0]), 1, 0);

  // Clear display
  oled_clear_display(slave);


}
I2C_PORT_DESC i2c_bus_0 =
  {
    0, 18,
    0, 7,
  };

// Dummy structure
I2C_SLAVE_DESC oled0 =
  {
    &i2c_bus_0,
    0x78 >>1,
  };


////////////////////////////////////////////////////////////////////////////////
//

// Send debug messages back, as they aren't packets they will get ignored
// and just slow comms

void debug_message(char *msg)
{
  return;
  Serial.println(msg);
}


int count = 0;
int qcount = 0;

////////////////////////////////////////////////////////////////////////////////
//
//
//
////////////////////////////////////////////////////////////////////////////////

void setup() {

  Serial.begin(115200);
  debug_message("Link Monitor");
  
  i2c_init(&i2c_bus_0);
  
  oled_setup(&oled0);

  oled_set_xy(&oled0, 0,0);
  oled_display_string(&oled0, "Link Monitor");

  oled_set_xy(&oled0, 0,14);
  oled_display_string(&oled0, "I2C OLED Display");

  // Set up GPIOs
  pinMode(QValid_PIN, INPUT);
  pinMode(QAck_PIN, OUTPUT);
  pinMode(Q0_PIN, INPUT);
  pinMode(Q1_PIN, INPUT);
  pinMode(Q2_PIN, INPUT);
  pinMode(Q3_PIN, INPUT);
  pinMode(Q4_PIN, INPUT);
  pinMode(Q5_PIN, INPUT);
  pinMode(Q6_PIN, INPUT);
  pinMode(Q7_PIN, INPUT);

  pinMode(I0_PIN, OUTPUT);
  pinMode(I1_PIN, OUTPUT);
  pinMode(I2_PIN, OUTPUT);
  pinMode(I3_PIN, OUTPUT);
  pinMode(I4_PIN, OUTPUT);
  pinMode(I5_PIN, OUTPUT);
  pinMode(I6_PIN, OUTPUT);
  pinMode(I7_PIN, OUTPUT);

  pinMode(IValid_PIN, OUTPUT);
  pinMode(IAck_PIN, INPUT);
  
  pinMode(RESET_PIN, OUTPUT);

  digitalWrite(QAck_PIN, LOW);
  
  // reset the IMSC011
  digitalWrite(RESET_PIN, LOW);
  digitalWrite(RESET_PIN, HIGH);

  delay(100);
  digitalWrite(RESET_PIN, LOW);
  

}

const int q_data_pin[] =
  {
   Q0_PIN,
   Q1_PIN,
   Q2_PIN,
   Q3_PIN,
   Q4_PIN,
   Q5_PIN,
   Q6_PIN,
   Q7_PIN,
  };

// Read Q data lines
int read_q(void)
{
  int i;
  int val = 0;
  
  for(i=0; i<8; i++)
    {
      if( digitalRead(q_data_pin[i]) == HIGH )
	{
	  val |= 1 << i;
	}
      else
	{
	}
    }
  
  return(val);
}

const int i_data_pin[] =
  {
   I0_PIN,
   I1_PIN,
   I2_PIN,
   I3_PIN,
   I4_PIN,
   I5_PIN,
   I6_PIN,
   I7_PIN,
  };

// Read Q data lines
void write_i(int value)
{
  int i;
  int val = 0;
  
  for(i=0; i<8; i++)
    {
      if( value & (1 << i) )
	{
	  digitalWrite(i_data_pin[i], HIGH );
	}
      else
	{
	  digitalWrite(i_data_pin[i], LOW );
	}
    }
}



////////////////////////////////////////////////////////////////////////////////
//
//

// Buffer for ASCII decode
//

#define ASCII_DEC_LEN 20

char ascii_decode[ASCII_DEC_LEN*2] = "";

// Shift another byte in for ascii decode

void shift_ascii(char c)
{
  char dc;
  char temp_buf[ASCII_DEC_LEN*2];
  int idx;
  char line[80];
  
  if( isprint(c) )
    {
      dc = c;
    }
  else
    {
      dc = '.';
    }

  // Shift in to array
  temp_buf[0] = dc;
  temp_buf[1] = '\0';

  strncat(ascii_decode, temp_buf, ASCII_DEC_LEN);
  sprintf(line, "A<%s>(%d)\nT<%s>(%d)", ascii_decode, strlen(ascii_decode),temp_buf, strlen(temp_buf));

  //  debug_message(line);
  
  // Take last block of chars
  strcpy(temp_buf, ascii_decode);
  sprintf(line, "A<%s>(%d)\nT<%s>(%d)", ascii_decode, strlen(ascii_decode),temp_buf, strlen(temp_buf));

  //debug_message(line);

  idx = strlen(temp_buf)-ASCII_DEC_LEN;
  if( idx <= 0 )
    {
      idx = 0;
    }

  strncpy(ascii_decode, &(temp_buf[idx]), ASCII_DEC_LEN);
  ascii_decode[ASCII_DEC_LEN] = '\0';
  sprintf(line, "A<%s>(%d)\nT<%s>(%d)", ascii_decode, strlen(ascii_decode),temp_buf, strlen(temp_buf));
  //debug_message(line);
}

//////////////////////////////////////////////////////////////////////////////// 
//
// The loop acts as a pipe to the PC server
// We get the code working, then make it fast
//
// Bytes from the link are sent on,
//
// ^AB&        Where AB is the hex ascii of the data byte
//
// ^^&        ACK  Sent when ack received in either direction
//
////////////////////////////////////////////////////////////////////////////////

// Buffer for input packets
#define IN_BUF_LEN 40

char input_buffer[IN_BUF_LEN];
char last_buffer[IN_BUF_LEN] = {0};
int num_in = 0;
int in_packet = 0;

#define PACKET_START '^'
#define PACKET_END '&'

int loop_count = 0;

#if OLED_TRAFFIC
int tx_count = 0;
int rx_count = 0;
#endif

void loop()
{
  char line[40];
  int qval = 0;
  int i;
  
  int num;

  loop_count++;
  
#if OLED_TRAFFIC
      sprintf(line, "L%d", loop_count);
      //      sprintf(line, "%02X", qval);
      oled_set_xy(&oled0, 0,20);
      oled_display_string(&oled0, line);

#endif

  // See how many characters are ready
  if( num = Serial.available() )
    {
      int data;
      char line[40];
      sprintf(line, "R:avail(%d)", num);
      debug_message(line);

      // Process all of them now
      debug_message("[");
      for(i = 0; i<num; i++)
	{

	  data = Serial.read();
	  sprintf(line, "-%02X-(%c)", data, data);
	  debug_message(line);
	  
	  // If not in packet then look for packet start
	  if( !in_packet )
	    {
	      if( data == PACKET_START)
		{
		  in_packet = 1;
		  num_in = 0;
		  input_buffer[num_in++] = data;
		  debug_message("R:PKT ST");
		}
	    }
	  else
	    {
	      // If in a packet then add to buffer
	      // Until end of packet
	      if( data == PACKET_END )
		{
		  debug_message("R:PKT END");
		  input_buffer[num_in++] = data;
		  input_buffer[num_in] = '\0';
		  
		  //printf(" PKT:[%s]", input_buffer);
		  
#if OLED_TRAFFIC
		  tx_count++;
		  sprintf(line, "T%d [%s]%s]", tx_count, input_buffer, last_buffer);
		  strcpy(last_buffer, input_buffer);
		  oled_set_xy(&oled0, 0,28);
		  oled_display_string(&oled0, line);
		  
#endif

		  // All done, process packet
		  // If ACK then pass ack on to link
		  if( (input_buffer[0] == '^') && (input_buffer[1] == '^') )
		    {
		      // ACK low again
		      digitalWrite(QAck_PIN, LOW);

		      debug_message("R:ACK");
		    }
		  
		  if( (input_buffer[0] == '^') && (input_buffer[3] == '&') )
		    {
		      int byteval;

		      sscanf(input_buffer, "^%x&", &byteval);

		      sprintf(line, "R:Data:%d", byteval);
		      debug_message(line);

#if 1	      
		      // Send data to link
		      write_i(byteval);

		      // Signal data valid
		      digitalWrite(IValid_PIN, HIGH);
#else

		      // Temp ack of data
		      //Serial.println("^^&\n");
#endif
		    }
		  
		  num_in = 0;
		  in_packet = 0;
		}
	      else
		{
		  // Add to buffer
		  input_buffer[num_in++] = data;
		}
	    }
	}
      
	  debug_message("]");      
    }
  
  // Wait for IACK to go high
  // If it does then we send the ACK to the server and drive IValid low

  if( digitalRead(IAck_PIN) == HIGH )
    {
      digitalWrite(IValid_PIN, LOW);

      // Send ACK to the Server
      Serial.println("^^&");

      // Now ready to send another byte if needed
    }

  // We monitor the QValid line. When it is high we capture the data
  // values (8 bits)
  // we then display that value and send it on
  // When ACK comes in from server then we assert QACK

  // Is QValid active?
  if( digitalRead(QValid_PIN) == HIGH )
    {
#if OLED_TRAFFIC
      rx_count++;
      sprintf(line, "R%d", rx_count);
      //      sprintf(line, "%02X", qval);
      oled_set_xy(&oled0, 0,28);
      oled_display_string(&oled0, line);
      
#endif
      
      //Serial.println("QV high");
      
      //      delay(1);
      qcount++;
      
      // Yes, latch data
      qval = read_q();

      // Display the value we sent and the value we have received
      // They should be the same as the Pico echoes what it saw
#if 0
      sprintf(line, "V%d", count);
      //      sprintf(line, "%02X", qval);
      oled_set_xy(&oled0, 0,35);
      oled_display_string(&oled0, line);

#endif

#if OLED_SCROLL      
      shift_ascii(qval);      
      oled_set_xy(&oled0, 0,35);
      oled_display_string(&oled0, ascii_decode);
#endif
      
#if 1
      
      // ACK it
      digitalWrite(QAck_PIN, HIGH);

      // Wait for Qvalid to go low
      while( digitalRead(QValid_PIN) == HIGH )
	{
	}
#endif

      // Send the data byte on
      sprintf(line, "^%02X&", qval);
      Serial.println(line);

      // We wait for ACK from the host which will perform a
      // QAck when we receive it
      

      //delay(100);
    }
}
