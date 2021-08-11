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
 */

#define SUN 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __MWERKS__
#include <SIOUX.h>
#include "mac_input.h"
#else
#ifdef SUN
//#include <termio.h>
#else
#include <sys/termios.h>
#include <sys/ioctl.h>
//#define TCGETS TIOCGETA
//#define TCSETS TIOCSETA
//#define termio termios
#endif
#include <sys/types.h>
#include <unistd.h>
#endif

#include <time.h>
#include "processor.h"
#include "server.h"

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

extern unsigned char *mem;

#define MEM_BYTE_MASK 0x0001ffff

/* Write a byte to memory. */
inline void writebyte (unsigned long ptr, unsigned char value)
{
	/* Write byte, ensuring memory reference is in range. */
	mem[(ptr & MEM_BYTE_MASK)]   = value;
}

void server(void)
{
#ifdef PROFILE
	if (profiling)
		profile[2]++;
#endif
	if (copy==TRUE)
	{
#if 0	  
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
#endif
	}

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
			schedule ((Link0OutWdesc & 0xfffffffe), (Link0OutWdesc & 0x00000001));

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
				schedule ((Link0InWdesc & 0xfffffffe), (Link0InWdesc & 0x00000001));

				/* Reset channel. */
				Link0InWdesc = NotProcess_p;
				writeword (Link0In, NotProcess_p);
			}
		}
	}
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


void sp_open (void)
{
	char filename[256];
	char string[10];
	int  namelen;
	int  type;
	int  mode;
	return;
#if 0
	FILE *fd;

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


void sp_close (void)
{
	FILE *fd;

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
}


void sp_read (void)
{
	FILE *fd;
	int count;
	int length;

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
}


void sp_write (void)
{
	FILE *fd;
	int  datalen;
	int  length;
	int  pos;

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
}


void sp_gets (void)
{
	FILE *fd;
	int count;
	char *result;
	int length;
	int temp;

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
}


void sp_puts (void)
{
	FILE *fd;
	int  datalen;
	int  length;

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
}


void sp_flush (void)
{
	FILE *fd;
	int  length;

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
}


void sp_seek (void)
{
	FILE *fd;
	long offset;
	int origin;

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
}


void sp_tell (void)
{
	FILE *fd;
	long position;

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
}


void sp_eof (void)
{
	FILE *fd;

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
}


void sp_ferror (void)
{
	FILE *fd;

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
}


void sp_remove (void)
{
	char filename[256];
	int  namelen;

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
}


void sp_rename (void)
{
	char oldfilename[256];
	char newfilename[256];
	int  oldnamelen;
	int  newnamelen;

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
}


void sp_getkey (void)
{
	char ch;
#if 0
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

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	FromServerBuffer[3] = ch;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
#endif
}


void sp_pollkey (void)
{
	char ch;
#if 0
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

	ch = 'X';
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

	utctime = time (0);
	if (utctime == -1)
	{
		printf ("\nTime call failed - SP_TIME.\n");
		handler (-1);
	}

	localtime = utctime + timezone;

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
}


void sp_exit (void)
{
	long status;
	extern long quit;
	extern long quitstatus;

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
}


void sp_commandline (void)
{
	int all;
	int length;
	extern char CommandLineAll[256];
	extern char CommandLineMost[256];


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

