/*
 * Copyright (c) 1993-1996 Julian Highfield. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Julian Highfield.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * server.c
 *
 * Handle message buffers to and from server.
 * Handle messages to server.
 *
 * This version is for RP Pico.
 * All in and out data is routed to the hardware links.
 *
 * The server passes commands to anoher server on an Arduino Mega attached via an IMSC011 link
 * adapter IC. The arduino then sends commands to a server running on a host PC.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "processor.h"
#include "server.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/binary_info.h"
#include "pico/sd_card.h"

/* Macros. */
#define index(a,b)		((a)+(4*(b)))

#define PICO 1
#define DEBUG 0
#define DEBUG_LINK_TRAFFIC  0

extern uint32_t picoputerlinkin_get(PIO pio, uint sm);
extern void picoputerlinkout_program_putc(PIO pio, uint sm, int c);

/* Signal handler. */
void handler (int);

#undef TRUE
#undef FALSE
#define TRUE  0x0001
#define FALSE 0x0000

int  length;
int  boottemp;
int  bootlen;
int  loop1;
int  loop2;
int  FromServerLen = 0;
int  ToServerLen = 0;
unsigned char FromServerBuffer[(16*1024)+5];
unsigned char ToServerBuffer[514];

extern int  copy;
extern FILE *CopyIn;

extern unsigned long Link0OutWdesc;
extern unsigned long Link0OutSource;
extern unsigned long Link0OutLength;
extern unsigned long Link0InWdesc;
extern unsigned long Link0InDest;
extern unsigned long Link0InLength;

extern int profiling;
extern long profile[10];

extern struct termios t_getk;
extern struct termios t_poll;
extern enum {INIT, GETK, POLL} t_state;
extern unsigned char mem[];

#define MEM_BYTE_MASK 0x0000ffff

#if 0
/* Write a byte to memory. */
inline void writebyte (unsigned long ptr, unsigned char value)
{
	/* Write byte, ensuring memory reference is in range. */
	mem[(ptr & MEM_BYTE_MASK)]   = value;
}
#endif

void old_server(void)
{
#ifdef PROFILE
	if (profiling)
		profile[2]++;
#endif
#if 0	  
	if (copy==TRUE)
	{
		/* Still copying boot file to link. */
		boottemp = (16*1024) - FromServerLen;
#ifdef DEBUG
		printf ("boottemp = %d",boottemp);
#endif
		bootlen  = fread ((char *) &(FromServerBuffer[FromServerLen]), 1, boottemp, CopyIn);
		FromServerLen = FromServerLen + bootlen;
#ifdef DEBUG
		printf ("\nLoaded %d bytes.\n", bootlen);
#endif
		if ((bootlen < boottemp))
		{
			/* Met end of file. */
			copy = FALSE;
		}

	}
#endif
	
#ifdef DEBUG
	printf("\nComms server: To server buffer %d; From server buffer %d.\n", ToServerLen, FromServerLen);
#endif

	if (FromServerLen==0)
	{
		/* No messages leaving server, so server can handle the next incoming message. */
                /* Check Link0OutWdesc for a valid process to see if there is a message.       */
		if (Link0OutWdesc != NotProcess_p)
		{
			/* Move message to ToServerBuffer. */
			for (loop1=0; loop1<Link0OutLength; loop1++)
			{
				ToServerBuffer[ToServerLen] = byte (Link0OutSource);
				ToServerLen++;
				Link0OutSource++;
				/* The C compiler is broken. Hence 514. */
				if (ToServerLen >= 514)
					printf("\nHelp - Overflowing ToServerBuffer!\n");
			}

#ifdef DEBUG
			printf ("\nSatisifed comms request. Rescheduling process %8X.\n", Link0OutWdesc);
#endif

			/* Reschedule outputting process. */
			schedule (Link0OutWdesc);

			/* Reset Link0Out. */
			Link0OutWdesc = NotProcess_p;
			writeword (Link0Out, NotProcess_p);

			/* Check if incoming message is all here yet. */
			if (ToServerLen>=2)
			{
          			length = ToServerBuffer[0] + (256 * ToServerBuffer[1]);
          			if (length<6)
          			{
          				printf("\nBad message packet to server - too short!\n");
          			}
                    		else if (length>510)
                    		{
                    			printf("\nBad message packet to server - too long!\n");
                    		}
                    		else if ((length&0x01)!=0)
                    		{
                    			printf("\nBad message packet to server - odd length!\n");
                    		}
				else if ((length+2)==ToServerLen)
				{
					/* Full message has arrived. Handle it. */
					message();

					/* Delete handled message. */
					for (loop1=0; loop1<(length+2); loop1++)
					{
						ToServerBuffer[loop1] = ToServerBuffer[loop1+length+2];
					}
					ToServerLen = ToServerLen - (length + 2);
				}
			}
		}
	}
	else
	{
		/* Can the next byte of outgoing message be transferred? */
		if (Link0InWdesc != NotProcess_p)
		{
			/* Move the requested amount of message. */
			loop1 = 0;
#ifdef DEBUG
printf ("\nFromServerLen = %8X", FromServerLen);
#endif
			while ((loop1 < Link0InLength) && (FromServerLen > 0))
			{
				writebyte (Link0InDest, FromServerBuffer[loop1]);
				Link0InDest++;
				FromServerLen--;
				loop1++;
			}

			/* Moved loop bytes. Correct FromServerBuffer. */
			for (loop2=0; loop2<FromServerLen; loop2++)
				FromServerBuffer[loop2] = FromServerBuffer[loop2+loop1];

#ifdef DEBUG
printf ("\nLink0InLength = %8X, loop = %8X.\n", Link0InLength, loop1);
#endif
			/* If message request was fully satisfied, reset channel. */
			Link0InLength = Link0InLength - loop1;
			if (Link0InLength == 0)
			{
#ifdef DEBUG
				printf ("Comms request satisfied. Reschedule process %8X.\n", Link0InWdesc);
#endif
				/* Reschedule outputting process. */
				schedule (Link0InWdesc);

				/* Reset channel. */
				Link0InWdesc = NotProcess_p;
				writeword (Link0In, NotProcess_p);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Server that takes message out requests and sends them to the hardware link
//
// Also polls the hardware linkin lines and returns data to the emulator.
//
// Handles transfers while emulator is running other processes.
// Handles multiple links
//
////////////////////////////////////////////////////////////////////////////////

// The link information
#define NUMBER_OF_LINKS 1

typedef struct _LINKOUT_DATA
{
  ;  uint32_t   w_desc;           // Holds process address (mirror of channel word)
  uint32_t   source;           // Where next byte of message is
  uint32_t   length;           // How many bytes to send
  uint32_t   channel_address;  // Where the channel word is
  PIO      pio;              // Which PIO is providing the link
  uint     sm;               // Which state machine in the PIO handles the link
  int      ack_wait;         // We have sent a byte, nd are waiting for an ACK
  int      done_on_ack;      // After next ACK we are done
} LINKOUT_DATA;

LINKOUT_DATA LinkOut[NUMBER_OF_LINKS];

typedef struct _LINKIN_DATA
{
  ;uint32_t   w_desc;           // Holds process address
  uint32_t   dest;              // Where next byte of message is stored
  uint32_t   length;            // How many bytes to send
  uint32_t   channel_address;   // Where the channel word is
  PIO      pio;                 // Which PIO is providing the link
  uint     sm;                  // Which state machine in the PIO handles the link
  bool     have_stored;         // Have we got a stored byte?
  int      stored_byte;         // Stored byte

} LINKIN_DATA;

LINKIN_DATA LinkIn[NUMBER_OF_LINKS];

int which_linkout(uint32_t a)
{
  int linkno;
  
  switch(a)
    {
    case Link0Out:
      linkno = 0;
      break;

    case Link1Out:
      linkno = 1;
      break;

    case Link2Out:
      linkno = 2;
      break;

    case Link3Out:
      linkno = 3;
      break;

      // What do we do? Use Link 0 for now
    default:
      break;
    }
  
  return linkno;
}

int which_linkin(uint32_t a)
{
  int linkno;
  
  switch(a)
    {
    case Link0In:
      linkno = 0;
      break;

    case Link1In:
      linkno = 1;
      break;

    case Link2In:
      linkno = 2;
      break;

    case Link3In:
      linkno = 3;
      break;
      
      // What do we do? Use Link 0 for now
    default:
      break;
    }
  
  return linkno;
}

uint32_t linkin_address[NUMBER_OF_LINKS] =
  {
   Link0In,   
   Link1In,   
   Link2In,   
   Link3In,   
  };

uint32_t linkout_address[NUMBER_OF_LINKS] =
  {
   Link0Out,   
   Link1Out,   
   Link2Out,   
   Link3Out,   
  };

////////////////////////////////////////////////////////////////////////////////
//
// Initialise the link structures
//
////////////////////////////////////////////////////////////////////////////////

void server_init(void)
{
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      LinkOut[i].pio = 0;
      LinkOut[i].sm = 0;
      LinkOut[i].ack_wait = 0;
      LinkOut[i].done_on_ack = 0;
      LinkOut[i].length = 0;
      LinkOut[i].channel_address = linkout_address[i];
    }

  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      LinkIn[i].pio = 0;
      LinkIn[i].sm = 0;
      LinkIn[i].length = 0;
      LinkIn[i].channel_address = linkin_address[i];
      LinkIn[i].have_stored = false;
    }
}


//------------------------------------------------------------------------------

// Set up the PIo and SM for a linkin or linkout

void server_linkout_init(int linkno, int pio, int sm)
{
  LinkOut[linkno].pio = pio;
  LinkOut[linkno].sm  = sm;      
}

void server_linkin_init(int linkno, int pio, int sm)
{
  LinkIn[linkno].pio = pio;
  LinkIn[linkno].sm  = sm;      
}

////////////////////////////////////////////////////////////////////////////////
//
// Sends a byte or a word out of a link
//
//
////////////////////////////////////////////////////////////////////////////////

void send_value_linkout_message(int length, uint32_t AReg, uint32_t BReg, uint32_t CReg, uint32_t WPtr, uint32_t IPtr, uint32_t CurPriority)
{
  int linkno = which_linkout(BReg);
  
  writeword (BReg, (WPtr | CurPriority));

  // Byte to output stored in workspace
  writeword (WPtr, AReg);
  
  //LinkOut[linkno].w_desc  = (WPtr | CurPriority);

  // Point to byte and send it
  LinkOut[linkno].source = WPtr;
  LinkOut[linkno].length = length;
  
  writeword (index (WPtr, -1), IPtr);
  
  start_process ();
}


void send_linkout_message(uint32_t AReg, uint32_t BReg, uint32_t CReg, uint32_t WPtr, uint32_t IPtr, uint32_t CurPriority)
{
#if DEBUG_LINK_TRAFFIC
  printf("LinkOut: %02X bytes from %08X.\n", AReg, CReg);
  for(int i=0; i<AReg; i++)
    {
      int b = byte_int(CReg+i);
      printf("%02X %c\n", b, isprint(b)?b:'.');
    }
#endif
  // Work out which channel it is
  int linkno = which_linkout(BReg);
  
  // Write channel word
  writeword (BReg, (WPtr | CurPriority));

  //LinkOut[linkno].w_desc  = (WPtr | CurPriority);
  LinkOut[linkno].source = CReg;
  LinkOut[linkno].length = AReg;
  
  writeword (index (WPtr, -1), IPtr);
  start_process ();
}

void receive_linkin_message(uint32_t AReg, uint32_t BReg, uint32_t CReg, uint32_t WPtr, uint32_t IPtr, uint32_t CurPriority)
{
  // Work out which channel it is
  int linkno = which_linkin(BReg);

  // Write channel word
  writeword (BReg, (WPtr | CurPriority));

  // Set up ready for new message coming in
  //LinkIn[linkno].w_desc    = (WPtr | CurPriority);
  LinkIn[linkno].dest      = CReg;
  LinkIn[linkno].length    = AReg;
  
  writeword (index (WPtr, -1), IPtr);
  start_process ();
}

//--------------------------------------------------------------------------------
// The server has to accept output requests from the emulator, and also receive
// data from the linkin line. This has to be done in the background, while processes
// are still being run on the emulator.
//--------------------------------------------------------------------------------

#define LINK_BYTE_NONE  600
#define LINK_BYTE_ACK   601


int get_data_from_link(int i)
{
  int data;
  
  if( data = picoputerlinkin_get(LinkIn[i].pio, LinkIn[i].sm) )
    {
      // We have data
      // We need to invert it and shift it
      data = data ^ 0xFFFFFFFF;
      data >>= 22;
      
#if 0	  
      sprintf(line, "\ndata= %08X", data);	  
      printf(line);
#endif

      if (data == 0 )
	{
	  // ACK
	  return(LINK_BYTE_ACK);
	}
      else
	{
	  // Data packet
	  // Remove second stop bit in LSB
	  data >>= 1;
	  
	  // Mask out data, just in case
	  data &= 0xff;

	  return(data);
	}
    }

  return(LINK_BYTE_NONE);
}

//--------------------------------------------------------------------------------
// This function handles the bootstrap sequence. It only exits
// when the code should be executed
//--------------------------------------------------------------------------------

void link_in_null(int link, int data)
{
  printf("%s:Link:%d, Data:%02X", __FUNCTION__, link, data);
}

void link_in_ack_null(int link)
{
  printf("%s:Link %d", __FUNCTION__, link);
}

// Called when data byte is received
LINK_IN_FP link_in_data_fp = link_in_null;

// Called when ACK is received
LINK_IN_ACK_FP link_in_ack_fp = link_in_ack_null;

//
// The latest emulator used uses core[] for internal RAM
// so we boot to that

void bootloop(void)
{
  // We do not send data unless it is a reply to a packet
  
  // If we receive anything then that link becomes the link for bootstrap
  
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      int data;
      char line[40];

      // ...and there is data...
      if( (data = get_data_from_link(i)) != LINK_BYTE_NONE )
	{
	  
	  if( data == LINK_BYTE_ACK )
	    {
	      printf("ACK\n");
	      // This is an ACK, handle it
	      (*link_in_ack_fp)(i);
	    }
	  else
	    {
	      printf("data:%02X\n", data);
	      // Process data if we are waiting for it
	      // This function performs any sending of an ACK
	      (*link_in_data_fp)(i, data);
	      
	    }
	}
    }
}


void send_ack_to_link(int i)
{
  // Send an ACK packet
  // This is a 10 packet, which when inverted and placed in the LSB
  // is 0x2ff
  //picoputerlinkout_program_putc(LinkOut[i].pio, LinkOut[i].sm, 0x2FF);
  
  // Start bits are zeros, but we need to shift data up to make room for the
  // stop bit which is a 1
  // Note: linkout line is inverted
  // LS bits shifted out first so start bits are at LS bits
  // Send back what we received
  //	  picoputerlinkout_program_putc(pio, linkout_sm, 0x200 | ((data ^ 0xff)<<1));
  pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, (uint32_t)0x2ff);
}

//--------------------------------------------------------------------------------
// This function is called in the main loop and handles all of the link
// requests that result from in and out instructions
// It does not handle bootstrap link traffic
//--------------------------------------------------------------------------------

#if 0
void linkloop2(void)
{
  // Handle all output links
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      int data;
      
      // If link is sending data, ... */
      
      if( LinkOut[i].length > 0 )
	{
	  /* ...and we have received an ACK for anything we have sent... */
	  if( !LinkOut[i].ack_wait ) 
	    {
	      /* ...then see if hardware can accept another byte */
	      if( !pio_sm_is_tx_fifo_full (LinkOut[i].pio, LinkOut[i].sm) )
		{
		  /* The fifo is not full so we can send a byte */
		  // We use a blocking call, but as the fifo isn't full,
		  // it should never block
		  data = byte(LinkOut[i].source);
		  pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, 0x200 | ((data ^ 0xff)<<1));
#if DEBUG
		  printf("\nSent %02X", byte(LinkOut[i].source));
#endif
		  
		  // Update the pointer to data for the next byte
		  (LinkOut[i].source)++;

		  // Update the length
		  (LinkOut[i].length)--;

		  if( LinkOut[i].length == 0 )
		    {
		      // We are done, but have to wait for the ACK
		      // Set a flag to indicate we are done when the ACK comes in
		      // as this clause won't be entered again due to the length
		      // being zero
		      LinkOut[i].done_on_ack = 1;
		    }
		  
		  /* Indicate that an ACK is required */
		  LinkOut[i].ack_wait = 1;		  
		}
	    }
	}
    }

  /* See if anything has been received */
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      int data;
      char line[40];

      // ...and there is data...
      if( data = picoputerlinkin_get(LinkIn[i].pio, LinkIn[i].sm) )
	{
	  // We have data
	  // We need to invert it and shift it
	  data = data ^ 0xFFFFFFFF;
	  data >>= 22;

#if 0	  
	  sprintf(line, "\ndata= %08X", data);	  
	  printf(line);
#endif
	  
	  if( data == 0 )
	    {
	      // This is an ACK, handle it
	      if( LinkOut[i].ack_wait )
		{
		  // The ACK is for a byte we have sent, clear the waiting flag
		  LinkOut[i].ack_wait = 0;
#if DEBUG		  
		  printf("\nACK rx ok");
#endif
		}
	      else
		{
#if 0
		  // Print a debug message
		  printf("\nACK rx on link %d when not waiting for ACK", i);
#endif
		}
	      
	      // We may now be done, check
	      if( LinkOut[i].done_on_ack )
		{
		  // This message has been sent
		  LinkOut[i].done_on_ack = 0;
		  
		  // Reschedule the process
		  schedule (word_int(LinkOut[i].channel_address));
		  
		  /* Reset Link0Out. */
		  //		  LinkOut[i].w_desc = NotProcess_p;
		  writeword (LinkOut[i].channel_address, NotProcess_p);
#if 0
		  printf("\nMessage Sent");
#endif
		}
	    }
	  else
	    {
	      // Process data if we are waiting for it
	      if( LinkIn[i].length > 0 )
		{
		  // Data packet
		  // Remove second stop bit in LSB
		  data >>= 1;
		  
		  // Mask out data, just in case
		  data &= 0xff;

#if DEBUG		  
		  printf("\nDATA:%02X", data);
#endif		  
		  // Store in buffer and move dest to next byte
		  writebyte ((LinkIn[i].dest)++, data);
		  
		  // Update length
		  (LinkIn[i].length)--;
		  
		  if( LinkIn[i].length == 0 )
		    {
		      /* Reset channel. */
		      //Link0InWdesc = NotProcess_p;
		      writeword (LinkIn[i].channel_address, NotProcess_p);

		      // All byte received, reschedule process
		      schedule ((LinkIn[i].channel_address));
		    }
		  
		  // Send an ACK packet
		  // This is a 10 packet, which when inverted and placed in the LSB
		  // is 0x2ff
		  //picoputerlinkout_program_putc(LinkOut[i].pio, LinkOut[i].sm, 0x2FF);
		  
		  // Start bits are zeros, but we need to shift data up to make room for the
		  // stop bit which is a 1
		  // Note: linkout line is inverted
		  // LS bits shifted out first so start bits are at LS bits
		  // Send back what we received
		  //	  picoputerlinkout_program_putc(pio, linkout_sm, 0x200 | ((data ^ 0xff)<<1));
		  pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, (uint32_t)0x2ff);
		}
	    }
	}
    }
}
#endif

void linkloop(void)
{
  // Handle all output links
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      int data;
      
      // If link is sending data, ... */
      
      if( LinkOut[i].length > 0 )
	{
	  /* ...and we have received an ACK for anything we have sent... */
	  if( !LinkOut[i].ack_wait ) 
	    {
	      /* ...then see if hardware can accept another byte */
	      if( !pio_sm_is_tx_fifo_full (LinkOut[i].pio, LinkOut[i].sm) )
		{
		  /* The fifo is not full so we can send a byte */
		  // We use a blocking call, but as the fifo isn't full,
		  // it should never block
		  data = byte(LinkOut[i].source);
		  pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, 0x200 | ((data ^ 0xff)<<1));
#if DEBUG
		  printf("\nSent %02X", byte(LinkOut[i].source));
#endif
		  
		  // Update the pointer to data for the next byte
		  (LinkOut[i].source)++;

		  // Update the length
		  (LinkOut[i].length)--;

		  if( LinkOut[i].length == 0 )
		    {
		      // We are done, but have to wait for the ACK
		      // Set a flag to indicate we are done when the ACK comes in
		      // as this clause won't be entered again due to the length
		      // being zero
		      LinkOut[i].done_on_ack = 1;
		    }
		  
		  /* Indicate that an ACK is required */
		  LinkOut[i].ack_wait = 1;		  
		}
	    }
	}
    }

  /* See if anything has been received */
  for(int i=0; i<NUMBER_OF_LINKS; i++)
    {
      int data;
      char line[40];

      // Are we waiting for data?
      if( LinkIn[i].length > 0 )
	{
	  // Yes, do we have a stored byte?
	  if( LinkIn[i].have_stored )
	    {
	      // Take stored byte and pass it on
	      // Store in buffer and move dest to next byte
	      writebyte ((LinkIn[i].dest)++, LinkIn[i].stored_byte);
	      
	      // Update length
	      (LinkIn[i].length)--;
	      
	      if( LinkIn[i].length == 0 )
		{
		  // All byte received, reschedule process
		  schedule (word_int(LinkIn[i].channel_address));
		  
		  /* Reset channel. */
		  //Link0InWdesc = NotProcess_p;
		  writeword (LinkIn[i].channel_address, NotProcess_p);
		}
	      
	      // Send an ACK packet
	      // This is a 10 packet, which when inverted and placed in the LSB
	      // is 0x2ff
	      //picoputerlinkout_program_putc(LinkOut[i].pio, LinkOut[i].sm, 0x2FF);
	      
	      // Start bits are zeros, but we need to shift data up to make room for the
	      // stop bit which is a 1
	      // Note: linkout line is inverted
	      // LS bits shifted out first so start bits are at LS bits
	      // Send back what we received
	      //	  picoputerlinkout_program_putc(pio, linkout_sm, 0x200 | ((data ^ 0xff)<<1));
	      pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, (uint32_t)0x2ff);

	      // Not stored any more
	      LinkIn[i].have_stored = false;
	      return;
	    }
	}

      // If we have a stored byte then don't get data as we have nowhere to store
      // it if it's not an ACK
      if( !LinkIn[i].have_stored )
	{
	  // Is there data?
	  // We have to process ACKs whether we are waiting for data on this
	  // link or not


	  if( (data = get_data_from_link(i)) != LINK_BYTE_NONE )
	    {
	      // is it an ACK?
	      if( data == LINK_BYTE_ACK )
		{
		  // This is an ACK, handle it
		  if( LinkOut[i].ack_wait )
		    {
		      // The ACK is for a byte we have sent, clear the waiting flag
		      LinkOut[i].ack_wait = 0;
#if DEBUG		  
		      printf("\nACK rx ok");
#endif
		    }
		  else
		    {
#if 0
		      // Print a debug message
		      printf("\nACK rx on link %d when not waiting for ACK", i);
#endif
		    }
	      
		  // We may now be done, check
		  if( LinkOut[i].done_on_ack )
		    {
		      // This message has been sent
		      LinkOut[i].done_on_ack = 0;
		  
		      // Reschedule the process
		      schedule (word_int(LinkOut[i].channel_address));
		  
		      /* Reset Link0Out. */
		      //		  LinkOut[i].w_desc = NotProcess_p;
		      writeword (LinkOut[i].channel_address, NotProcess_p);
#if 0
		      printf("\nMessage Sent");
#endif
		    }
		}
	      else
		{
		  // Process data if we are waiting for it
		  if( LinkIn[i].length > 0 )
		    {
#if 0		  
		      // Data packet
		      // Remove second stop bit in LSB
		      data >>= 1;
		  
		      // Mask out data, just in case
		      data &= 0xff;
#endif
		  
#if DEBUG		  
		      printf("\nDATA:%02X", data);
#endif		  
		      // Store in buffer and move dest to next byte
		      writebyte ((LinkIn[i].dest)++, data);
		  
		      // Update length
		      (LinkIn[i].length)--;
		  
		      if( LinkIn[i].length == 0 )
			{
			  // All byte received, reschedule process
			  schedule (word_int(LinkIn[i].channel_address));
		      
			  /* Reset channel. */
			  //Link0InWdesc = NotProcess_p;
			  writeword (LinkIn[i].channel_address, NotProcess_p);
			}
		  
		      // Send an ACK packet
		      // This is a 10 packet, which when inverted and placed in the LSB
		      // is 0x2ff
		      //picoputerlinkout_program_putc(LinkOut[i].pio, LinkOut[i].sm, 0x2FF);
		  
		      // Start bits are zeros, but we need to shift data up to make room for the
		      // stop bit which is a 1
		      // Note: linkout line is inverted
		      // LS bits shifted out first so start bits are at LS bits
		      // Send back what we received
		      //	  picoputerlinkout_program_putc(pio, linkout_sm, 0x200 | ((data ^ 0xff)<<1));
		      pio_sm_put_blocking(LinkOut[i].pio, LinkOut[i].sm, (uint32_t)0x2ff);
		    }
		  else
		    {
		      // We have had a byte, but we aren't waiting for it, so we store it
		      // ready for when we do want it. It will be ACK'd then
		      LinkIn[i].stored_byte = data;
		      LinkIn[i].have_stored = true;
		    }
		}
	    }
	}
    }
}

// The original server, does nothing as we do host communitcation
// in the main loop. We leave this call so the original code is happy
// and the start_process() and D_check functions do whatever other things they did

int server(void)
{
  linkloop();
  return(1);
}

void message(void)
{
	unsigned char tag;
	int temp;

	tag = ToServerBuffer[2];
	
#ifdef __MWERKS__
	/* Update stdio stream. */
	check_input();
#endif

#ifdef DEBUG
	printf ("\nHandling server command. Buffer = %d. Tag = %2X\n", ToServerLen, tag);
#endif
	switch (tag)
	{
		case SP_OPEN:   sp_open();
				break;
		case SP_CLOSE:  sp_close();
				break;
		case SP_READ:   sp_read();
				break;
		case SP_WRITE:  sp_write();
				break;
		case SP_GETS:   sp_gets();
				break;
		case SP_PUTS:   sp_puts();
				break;
		case SP_FLUSH:  sp_flush();
				break;
		case SP_SEEK:   sp_seek();
				break;
		case SP_TELL:   sp_tell();
				break;
		case SP_EOF:    sp_eof();
				break;
		case SP_FERROR: sp_ferror();
				break;
		case SP_REMOVE: sp_remove();
				break;
		case SP_RENAME: sp_rename();
				break;

		case SP_GETKEY:  sp_getkey();
				 break;
		case SP_POLLKEY: sp_pollkey();
				 break;
		case SP_GETENV:  sp_getenv();
				 break;
		case SP_TIME:    sp_time();
				 break;
		case SP_SYSTEM:  sp_system();
				 break;
		case SP_EXIT:    sp_exit();
				 break;

		case SP_COMMANDLINE: sp_commandline();
				     break;
		case SP_CORE:        sp_core();
				     break;
		case SP_VERSION:     sp_version();
				     break;

		default:	printf ("\nBad server command %2X.\n", tag);
				for (temp=0; temp<ToServerLen; temp++)
					printf ("To server byte %2X.\n", ToServerBuffer[temp]);
				handler (-1);
				break;
	}
}


void error_packet (void)
{
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_ERROR;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}

// We can't open a file

void sp_open (void)
{
	char filename[256];
	char string[10];
	int  namelen;
	int  type;
	int  mode;
	FILE *fd;

#if PICO
	error_packet();
	return;
#else  
	namelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(namelen+7))
	{
		printf("\nBad message packet - SP_OPEN - too short.\n");
		error_packet ();
		return;
	}

	if (namelen <= 0)
	{printf("\nBad Packet\n");
		error_packet ();
		return;
	}

	strncpy (filename, (char *) (ToServerBuffer+5), namelen);
	filename[namelen] = '\0';
	type = ToServerBuffer[5+namelen];
	mode = ToServerBuffer[6+namelen];

	switch (type)
	{
		case SP_BINARY: break;
		case SP_TEXT:   break;
		default:        error_packet ();
				return;
	}

	switch (mode)
	{
		case SP_INPUT:           strcpy (string, "r");
					 break;
		case SP_OUTPUT:		 strcpy (string, "w");
					 break;
		case SP_APPEND:		 strcpy (string, "a");
					 break;
		case SP_EXISTING_UPDATE: strcpy (string, "r+");
					 break;
		case SP_NEW_UPDATE:	 strcpy (string, "w+");
					 break;
		case SP_APPEND_UPDATE:   strcpy (string, "a+");
					 break;
		default:                 error_packet ();
					 return;
	}

	fd = fopen (filename, string);
	if (fd == NULL)
	{printf("\nFailed to open %s",filename);
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ((unsigned int)fd) & 0x000000ff;
	FromServerBuffer[4] = (((unsigned int)fd) & 0x0000ff00) >>  8;
	FromServerBuffer[5] = (((unsigned int)fd) & 0x00ff0000) >> 16;
	FromServerBuffer[6] = (((unsigned int)fd) & 0xff000000) >> 24;
	
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}

// Can't close

void sp_close (void)
{
	FILE *fd;

#if PICO
	error_packet();
	return;
#else
	
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	if (fclose(fd) != 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_read (void)
{
	FILE *fd;
	int count;
	int length;

#if PICO
	error_packet();
	return;
#else
        if (ToServerLen<9)
	{
		printf("\nBad message packet - SP_READ - too short.\n");
		error_packet ();
		return;
	}

	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	count = ToServerBuffer[7] + (ToServerBuffer[8]<<8);
	if (count > 16384)
	{
		printf("\nSP_READ - Requested %d bytes - too many.\n", count);
		handler (-1);
	}

	length = fread ((&FromServerBuffer[5]), 1, count, fd);

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (length & 0x000000ff);
	FromServerBuffer[4] = (length & 0x0000ff00) >> 8;

	length = length + 3;
	if ((length & 0x01) == 1) length++;

	FromServerBuffer[0] = (length & 0x000000ff);
	FromServerBuffer[1] = (length & 0x0000ff00) >> 8;

	FromServerLen = length + 2;
#endif
}


void sp_write (void)
{
	FILE *fd;
	int  datalen;
	int  length;
	int  pos;

#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	datalen = ToServerBuffer[7]+(ToServerBuffer[8]<<8);
        if (ToServerLen<(datalen+9))
	{
		printf("\nBad message packet - SP_WRITE - too short.\n");
		error_packet ();
		return;
	}

	/* Massage output to avoid CRs. */
	for (pos = 9; pos < (9 + datalen); pos++)
		if (ToServerBuffer[pos] == 0x0d)
			ToServerBuffer[pos] = 0x00;

	length = fwrite ((&ToServerBuffer[9]), 1, datalen, fd);

	/* Must flush screen output. Strange standard. */
	if (fd == stdout)
	{
		fflush (stdout);
		/* fflush doesn't work with SIOUX without this. */
		/*SetSIOUXBufferMode (SIOUXNoBuffering);
		printf ("\0");
		SetSIOUXBufferMode (SIOUXLineBuffering);*/
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (length & 0x000000ff);
	FromServerBuffer[4] = (length & 0x0000ff00) >> 8;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_gets (void)
{
	FILE *fd;
	int count;
	char *result;
	int length;
	int temp;
#if PICO
	error_packet();
	return;
#else
        if (ToServerLen<9)
	{
		printf("\nBad message packet - SP_GETS - too short.\n");
		error_packet ();
		return;
	}

	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	count = ToServerBuffer[7] + (ToServerBuffer[8]<<8);
	if (count > 16384)
	{
		printf("\nSP_GETS - Requested %d bytes - too many.\n", count);
		handler (-1);
	}

	/* Read count chars, or until EOF, or until newline. */
	result = fgets ((char *) &(FromServerBuffer[5]), (count+1), fd);
	if (result==NULL)
	{
		error_packet ();
		return;
	}

	length = strlen (result);

	/* Do not return newline characters. */
	for (temp=0; temp<length; temp++)
		if (FromServerBuffer[5+temp]=='\n')
			length = temp;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (length & 0x000000ff);
	FromServerBuffer[4] = (length & 0x0000ff00) >> 8;

	length = length + 3;
	if ((length & 0x01) == 1) length++;

	FromServerBuffer[0] = (length & 0x000000ff);
	FromServerBuffer[1] = (length & 0x0000ff00) >> 8;

	FromServerLen = length + 2;
#endif
}


void sp_puts (void)
{
	FILE *fd;
	int  datalen;
	int  length;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	datalen = ToServerBuffer[7]+(ToServerBuffer[8]<<8);
        if (ToServerLen<(datalen+9))
	{
		printf("\nBad message packet - SP_PUTS - too short.\n");
		error_packet ();
		return;
	}

	length = fwrite ((&ToServerBuffer[9]), 1, datalen, fd);

	/* Must flush stdout. */
	if (fd == stdout)
	{
		fflush (stdout);
		/* fflush doesn't work with SIOUX without this. */
		/*SetSIOUXBufferMode (SIOUXNoBuffering);
		printf ("\0");
		SetSIOUXBufferMode (SIOUXLineBuffering);*/
	}

	if (length==EOF)
	{
		error_packet ();
		return;
	}

	length = fputs ("\n", fd);
	if (length==EOF)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_flush (void)
{
	FILE *fd;
	int  length;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	if (fflush (fd) != 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_seek (void)
{
	FILE *fd;
	long offset;
	int origin;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

        if (ToServerLen<15)
	{
		printf("\nBad message packet - SP_SEEK - too short.\n");
		error_packet ();
		return;
	}

	offset = ToServerBuffer[7] + (ToServerBuffer[8]<<8);
	offset = offset + (ToServerBuffer[9]<<16) + (ToServerBuffer[10]<<24);

	origin = ToServerBuffer[11] + (ToServerBuffer[12]<<8);
	origin = origin + (ToServerBuffer[13]<<16) + (ToServerBuffer[14]<<24);
	origin--;

	if ((fseek (fd, offset, origin)) != 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_tell (void)
{
	FILE *fd;
	long position;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	position = ftell (fd);
	if (position < 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ((unsigned int) position) & 0x000000ff;
	FromServerBuffer[4] = (((unsigned int) position) & 0x0000ff00) >>  8;
	FromServerBuffer[5] = (((unsigned int) position) & 0x00ff0000) >> 16;
	FromServerBuffer[6] = (((unsigned int) position) & 0xff000000) >> 24;

	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_eof (void)
{
	FILE *fd;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	if ((feof (fd)) == 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_ferror (void)
{
	FILE *fd;
#if PICO
	error_packet();
	return;
#else
	fd = (FILE *)(ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	if ((int)fd==0)
		fd = stdin;
	else if ((int)fd==1)
		fd = stdout;
	else if ((int)fd==2)
		fd = stderr;

	if ((ferror (fd)) == 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 8;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;

	FromServerBuffer[7] = 0;
	FromServerBuffer[8] = 0;
	FromServerBuffer[9] = 0;

	FromServerLen = 10;
#endif
}


void sp_remove (void)
{
	char filename[256];
	int  namelen;
#if PICO
	error_packet();
	return;
#else
	namelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(namelen+5))
	{
		printf("\nBad message packet - SP_REMOVE - too short.\n");
		error_packet ();
		return;
	}

	if (namelen <= 0)
	{
		error_packet ();
		return;
	}

	strncpy (filename, (char *) (ToServerBuffer+5), namelen);
	filename[namelen] = '\0';

	if ((remove (filename)) != 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_rename (void)
{
	char oldfilename[256];
	char newfilename[256];
	int  oldnamelen;
	int  newnamelen;
#if PICO
	error_packet();
	return;
#else
	oldnamelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(oldnamelen+5))
	{
		printf("\nBad message packet - SP_RENAME - too short.\n");
		error_packet ();
		return;
	}

	if (oldnamelen <= 0)
	{
		error_packet ();
		return;
	}

	strncpy (oldfilename, (char *) (ToServerBuffer+5), oldnamelen);
	oldfilename[oldnamelen] = '\0';

	newnamelen = ToServerBuffer[5+oldnamelen]+(256*ToServerBuffer[6+oldnamelen]);
        if (ToServerLen<(oldnamelen+newnamelen+7))
	{
		printf("\nBad message packet - SP_REMOVE - too short.\n");
		error_packet ();
		return;
	}

	if (newnamelen <= 0)
	{
		error_packet ();
		return;
	}

	strncpy (newfilename, (char *) (ToServerBuffer+7+oldnamelen), newnamelen);
	newfilename[newnamelen] = '\0';

	if ((rename (oldfilename, newfilename)) != 0)
	{
		error_packet ();
		return;
	}

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_getkey (void)
{
	char ch;

#if PICO
	ch = getchar();
#else
#ifndef __MWERKS__
	if (t_state != GETK)
	if ((ioctl (0, TCSETS, &t_getk)) != 0)
	{
		printf("\nBad ioctl - SP_GETKEY.\n");
		handler (-1);
	}
	t_state = GETK;

	if ((read (0, &ch, 1)) != 1)
	{
		error_packet ();
		return;
	}
#else

	ch = mygetchar();
#endif
#endif
	
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ch;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}


void sp_pollkey (void)
{
	char ch;

#if PICO
	ch = 0;
#else	
#ifndef __MWERKS__
	if (t_state != POLL)
	if ((ioctl (0, TCSETS, &t_poll)) != 0)
	{
		printf("\nBad ioctl - SP_POLLKEY.\n");
		handler (-1);
	}
	t_state = POLL;

	if ((read (0, &ch, 1)) != 1)
	{
		error_packet ();
		return;
	}
#else

	if ((ch = mypollchar()) == -1)
	{
		error_packet ();
		return;
	}
#endif
#endif
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ch;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}


void sp_getenv (void)
{
	char name[256];
	char *string;
	int  namelen;
	FILE *fd;

	namelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(namelen+5))
	{
		printf("\nBad message packet - SP_GETENV - too short.\n");
		error_packet ();
		return;
	}

	if (namelen <= 0)
	{
		error_packet ();
		return;
	}

	strncpy (name, (char *) (ToServerBuffer+5), namelen);
	name[namelen] = '\0';

#if PICO
	namelen = strlen ("????");
	strncpy ((char *) &FromServerBuffer[5], string, namelen);

#else
	
#ifdef __MWERKS__
	if (strcmp("IBOARDSIZE",name)==0)
	{
		strcpy (name, "#200000");
		string = name;
	}
	else
		string = NULL;
#else
	string = getenv (name);
#endif
	
	if (string == NULL)
	{
		error_packet ();
		return;
	}

	namelen = strlen (string);
	strncpy ((char *) &FromServerBuffer[5], string, namelen);
#ifdef DEBUG
printf ("%s\n", string);
#endif
#endif
 
	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (namelen & 0x000000ff);
	FromServerBuffer[4] = (namelen & 0x0000ff00) >> 8;

	namelen = namelen + 3;
	if ((namelen & 0x01) == 1) namelen++;

	FromServerBuffer[0] = (namelen & 0x000000ff);
	FromServerBuffer[1] = (namelen & 0x0000ff00) >> 8;

	FromServerLen = namelen + 2;
}


void sp_time (void)
{
	long localtime;
	long utctime;
	time_t timezone = 0;
#if PICO
	error_packet();
	return;
#else
	utctime = time (0);
	if (utctime == -1)
	{
		printf ("\nTime call failed - SP_TIME.\n");
		handler (-1);
	}

	localtime = utctime + timezone;
#endif
	
	FromServerBuffer[0] = 10;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ((unsigned int) localtime) & 0x000000ff;
	FromServerBuffer[4] = (((unsigned int) localtime) & 0x0000ff00) >>  8;
	FromServerBuffer[5] = (((unsigned int) localtime) & 0x00ff0000) >> 16;
	FromServerBuffer[6] = (((unsigned int) localtime) & 0xff000000) >> 24;

	FromServerBuffer[7]  = ((unsigned int) utctime) & 0x000000ff;
	FromServerBuffer[8]  = (((unsigned int) utctime) & 0x0000ff00) >>  8;
	FromServerBuffer[9]  = (((unsigned int) utctime) & 0x00ff0000) >> 16;
	FromServerBuffer[10] = (((unsigned int) utctime) & 0xff000000) >> 24;

	FromServerBuffer[11] = 0;

	FromServerLen = 12;
}


void sp_system (void)
{
	char command[256];
	int  commandlen;
	int result;
#if PICO
	error_packet();
	return;
#else
	commandlen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(commandlen+7))
	{
		printf("\nBad message packet - SP_SYSTEM - too short.\n");
		error_packet ();
		return;
	}

	if (commandlen <= 0)
	{
		error_packet ();
		return;
	}

	strncpy (command, (char *) (ToServerBuffer+5), commandlen);
	command[commandlen] = '\0';

	result = system (command);

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ((unsigned int) result) & 0x000000ff;
	FromServerBuffer[4] = (((unsigned int) result) & 0x0000ff00) >>  8;
	FromServerBuffer[5] = (((unsigned int) result) & 0x00ff0000) >> 16;
	FromServerBuffer[6] = (((unsigned int) result) & 0xff000000) >> 24;
	
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_exit (void)
{
	long status;
	extern long quit;
	extern long quitstatus;
#if PICO
	error_packet();
	return;
#else
	status = ToServerBuffer[3] + (ToServerBuffer[4]<<8);
	status = status + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24);

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;

	quitstatus = status;
	quit = TRUE;

	printf ("\n********** The server has exited **********\n");
#endif
}


void sp_commandline (void)
{
	int all;
	int length;
	extern char CommandLineAll[256];
	extern char CommandLineMost[256];

#if PICO
	error_packet();
	return;
#else
	all = ToServerBuffer[3];
	if (all == 0)
	{
		strcpy ((char *) &FromServerBuffer[5], CommandLineMost);
		length = strlen (CommandLineMost);
	}
	else
	{
		strcpy ((char *) &FromServerBuffer[5], CommandLineAll);
		length = strlen (CommandLineMost);
	}

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (length & 0x000000ff);
	FromServerBuffer[4] = (length & 0x0000ff00) >> 8;

	length = length + 3;
	if ((length & 0x01) == 1) length++;

	FromServerBuffer[0] = (length & 0x000000ff);
	FromServerBuffer[1] = (length & 0x0000ff00) >> 8;

	FromServerLen = length + 2;
#endif
}


void sp_core (void)
{
	long offset;
	int  length;
	extern int peeksize;
	extern int analyse;
	extern unsigned char core[16*1024];

	offset = ToServerBuffer[3] + (ToServerBuffer[4]<<8);
	offset = offset + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24);

	length = ToServerBuffer[7]+(ToServerBuffer[8]<<8);

	if ((offset > (1024*peeksize)) || (analyse == FALSE))
	{
		error_packet ();
		return;
	}

	if ((offset+length) > (1024*peeksize))
		length = (1024*peeksize) - offset;

	memcpy (&FromServerBuffer[5], &core[offset], length);

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = (length & 0x000000ff);
	FromServerBuffer[4] = (length & 0x0000ff00) >> 8;

	length = length + 3;
	if ((length & 0x01) == 1) length++;

	FromServerBuffer[0] = (length & 0x000000ff);
	FromServerBuffer[1] = (length & 0x0000ff00) >> 8;

	FromServerLen = length + 2;
}


void sp_version (void)
{
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	/* Version * 10 */
	FromServerBuffer[3] = 10;
	/* Sun 4 */
	FromServerBuffer[4] = 5;
	/* SunOS */
	FromServerBuffer[5] = 4;
	/* B008 */
	FromServerBuffer[6] = 2;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}

void server_simkey(void)
{
  char ch;
  
	ch = 'Y';
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ch;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}
