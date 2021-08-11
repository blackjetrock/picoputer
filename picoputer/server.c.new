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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#endif

#ifdef __MWERKS__
  #include <SIOUX.h>
  #include "mac_input.h"
#else
  #ifdef CURTERM
    #include "curterm.h"
  #else
    #ifdef SUN
      #include <termio.h>
    #else
      #include <sys/termios.h>
      #include <sys/ioctl.h>
      #define TCGETS TIOCGETA
      #define TCSETS TIOCSETA
      #define termio termios
    #endif
  #endif
  #include <sys/types.h>
  #ifndef _MSC_VER
  #include <unistd.h>
  #endif
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
void dump_message   (char *msg, unsigned char *buffer, int len);
void notimpl_packet (void);
void error_packet   (void);
int DumpMessage;

#define MAX_FD  256
typedef struct _FileDesc_ {
        FILE *fd;
        int type;
} FileDesc;

FileDesc Files[MAX_FD];
char *MessageTags[129];

extern int32_t quit;
extern int32_t quitstatus;

extern int  copy;
extern FILE *CopyIn;
extern int  emudebug;
extern int  msgdebug;
extern char *dbgtrigger;
extern void set_debug (void);

extern int  usetvs;
extern FILE *InpFile, *OutFile;

extern int profiling;
extern uint32_t profile[10];

extern struct termios t_getk;
extern struct termios t_poll;
extern enum {INIT, GETK, POLL} t_state;

static
void init_server (void)
{
        int i;
        char msg[16];

        for (i = 0; i < MAX_FD; i++)
        {
                Files[i].fd = (FILE *)NULL;
                Files[i].type = -1;
        }

        Files[0].fd = stdin;
        Files[0].type = SP_TEXT;
        Files[1].fd = stdout;
        Files[1].type = SP_TEXT;
        Files[2].fd = stderr;
        Files[2].type = SP_TEXT;

        DumpMessage = FALSE;

        MessageTags[ SP_OPEN]    = "SP_OPEN";
        MessageTags[ SP_CLOSE]   = "SP_CLOSE";
        MessageTags[ SP_READ]    = "SP_READ";
        MessageTags[ SP_WRITE]   = "SP_WRITE";
        MessageTags[ SP_GETS]    = "SP_GETS";
        MessageTags[ SP_PUTS]    = "SP_PUTS";
        MessageTags[ SP_FLUSH]   = "SP_FLUSH";
        MessageTags[ SP_SEEK]    = "SP_PEEK";
        MessageTags[ SP_TELL]    = "SP_TELL";
        MessageTags[ SP_EOF]     = "SP_EOF";
        MessageTags[ SP_FERROR]  = "SP_FERROR";
        MessageTags[ SP_REMOVE]  = "SP_REMOVE";
        MessageTags[ SP_RENAME]  = "SP_RENAME";

        MessageTags[ SP_GETKEY]  = "SP_GETKEY";
        MessageTags[ SP_POLLKEY] = "SP_POLLKEY";
        MessageTags[ SP_GETENV]  = "SP_GETENV";
        MessageTags[ SP_TIME]    = "SP_TIME";
        MessageTags[ SP_SYSTEM]  = "SP_SYSTEM";
        MessageTags[ SP_EXIT]    = "SP_EXIT";

        MessageTags[ SP_COMMANDLINE] = "SP_COMMANDLINE";
        MessageTags[ SP_CORE]        = "SP_CORE";
        MessageTags[ SP_VERSION]     = "SP_VERSION";

        MessageTags[ SP_DOS]    = "SP_DOS";
        MessageTags[ SP_HELIOS] = "SP_HELIOS";
        MessageTags[ SP_VMS]    = "SP_VMS";
        MessageTags[ SP_SUNOS]  = "SP_SUNOS";
        MessageTags[ SP_TDS]    = "SP_TDS";
        MessageTags[ SP_OTHER]  = "SP_OTHER";

        for (i = 0; i < 129; i++)
        {
                if (MessageTags[i] == NULL)
                {
                        sprintf (msg, "MSG%X", i);
                        MessageTags[i] = strdup (msg);
                }
        }
}

static
unsigned int FromFile (FILE *fd)
{
        unsigned int i;

        i = 0;
        while (Files[i].fd && i < MAX_FD)
                i++;

        if (MAX_FD == i)
        {
                printf ("-E-EMUSRV: Help - files[] table overflow!\n");
                handler(-1);
        }
        Files[i].fd = fd;
        return i;
}

static
FILE* ToFile (unsigned int i)
{
        if (MAX_FD <= i || (FILE *)NULL == Files[i].fd)
        {
                printf ("-E-EMUSRV: Help - invalid file descriptor!\n");
                handler(-1);
        }
        return Files[i].fd;
}

static
char *msgtag (unsigned char tag)
{
        static char msg[16];

        if (tag < 129)
                return MessageTags[tag];
        sprintf (msg, "MSG%d", tag);
        return msg;
}


/* XXX support ALT construction on Link0 */
int server (void)
{
        static int do_init = 1;
        int activity;
        uint32_t LinkWdesc; 
        int data;

        /* M.Bruestle 15.2.2012
         *
         * No Wdesc register in Link hardware, use channel control word directly.
         *
         */

        if (do_init)
        {
                init_server();
                do_init = 0;
        }

	if (profiling)
		profile[2]++;

        activity = 0;
	if (copy==TRUE)
	{
		/* Still copying boot file to link. */
		boottemp = (16*1024) - FromServerLen;

                if (emudebug)
		        printf ("-I-EMUSRV: BootTemp = %d.", boottemp);
		bootlen  = fread ((char *) &(FromServerBuffer[FromServerLen]), 1, boottemp, CopyIn);
		FromServerLen = FromServerLen + bootlen;
                if (emudebug)
		        printf (" Loaded %d bytes.\n", bootlen);
		if ((bootlen < boottemp))
		{
			/* Met end of file. */
			copy = FALSE;
		}
                activity++;
	}

        if (msgdebug || emudebug)
        {
                if (ToServerLen + FromServerLen)
	                printf ("-I-EMUSRV: To server buffer %d; From server buffer %d.\n", ToServerLen, FromServerLen);
        }

	if ((!usetvs && (FromServerLen == 0)) ||
            ( usetvs && (Link0InLength == 0)))
	{
		/* No messages leaving server, so server can handle the next incoming message. */
                /* Check Link0Out for a valid process to see if there is a message.       */
                /* Check the length register, because the channel control word may be written. */
                /* Example: memory sizing in Minix demo. */
                LinkWdesc = word (Link0Out);
		if ((LinkWdesc != NotProcess_p) && Link0OutLength)
		{
                        activity++;
                        if (usetvs)
                        {
                                if (msgdebug || emudebug)
                                        printf ("-I-EMUSRV: TVS output. Link0OutLength = #%X.\n", Link0OutLength);
                                ToServerLen = 0;
			        for (loop1=0; loop1<Link0OutLength; loop1++)
			        {
                                        data = byte_int (Link0OutSource);
				        putc (data, OutFile);
				        Link0OutSource++;
                                        ToServerBuffer[ToServerLen++] = data;
			        }
                                if (msgdebug)
                                {
                                        DumpMessage = TRUE;
                                        dump_message ("TVS Output.", ToServerBuffer, ToServerLen);
                                }
                                ToServerLen = 0;
                        }
                        else
                        {
			        /* Move message to ToServerBuffer. */
			        for (loop1=0; loop1<Link0OutLength; loop1++)
			        {
				        ToServerBuffer[ToServerLen] = byte_int (Link0OutSource);
				        ToServerLen++;
				        Link0OutSource++;
				        if (ToServerLen == 514)
                                        {
					        printf ("-E-EMUSRV: Help - overflowing ToServerBuffer!\n");
                                                handler (-1);
                                        }
			        }
                        }

                        if (msgdebug || emudebug)
			        printf ("-I-EMUSRV: Satisfied comms request. Rescheduling process #%8X.\n", LinkWdesc);

			/* Reset Link0Out. */
                        reset_channel (Link0Out);

			/* Reschedule outputting process. */
			schedule (LinkWdesc);

			/* Check if incoming message is all here yet. */
			if (!usetvs && (ToServerLen>=2))
			{
          			length = ToServerBuffer[0] + (256 * ToServerBuffer[1]);
          			if (length<6)
          			{
          				printf ("-W-EMUSRV: Bad message packet to server - too short! (%d)\n", length);
          			}
                    		else if (length>510)
                    		{
                    			printf ("-W-EMUSRV: Bad message packet to server - too long! (%d)\n", length);
                    		}
                    		else if ((length&0x01)!=0)
                    		{
                    			printf ("-W-EMUSRV: Bad message packet to server - odd length! (%d)\n", length);
                    		}
				else if ((length+2)==ToServerLen)
				{
					/* Full message has arrived. Handle it. */
					message();

					/* XXX Delete handled message. */
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
                /* Check the length register too. See notes at Link0Out. */
                LinkWdesc = word (Link0In);
		if ((LinkWdesc != NotProcess_p) && Link0InLength)
		{
                        activity++;
			/* Move the requested amount of message. */
			loop1 = 0;
                        if (msgdebug || emudebug)
                                printf ("-I-EMUSRV: FromServerLen = #%X.\n", FromServerLen);
                        dump_message ("Reply.", FromServerBuffer, FromServerLen);
                        if (usetvs)
                        {
                                if (msgdebug || emudebug)
                                        printf ("-I-EMUSRV: TVS input. Link0InLength = #%X.\n", Link0InLength);

                                if (feof (InpFile))
                                {
                                        if (msgdebug || emudebug)
                                                printf ("-I-EMUSRV: EOF on TVS input.\n");
                                        quit = TRUE;
                                }
                                else
                                {
                                        FromServerLen = 0;
                                        while (loop1 < Link0InLength) 
                                        {
                                                if (feof (InpFile))
                                                {
                                                        if (msgdebug || emudebug)
                                                                printf ("-I-EMUSRV: EOF during TVS input.\n");
                                                        /* Terminate here. */
                                                        quit = TRUE;
                                                        break;
                                                }
                                                data = getc (InpFile); 
				                writebyte_int (Link0InDest, data);
				                Link0InDest++;
                                                FromServerBuffer[FromServerLen++] = data;
				                loop1++;
                                        }
                                }
                                loop1 = Link0InLength;

                                if (msgdebug)
                                {
                                        DumpMessage = TRUE;
                                        dump_message ("TVS Input.", FromServerBuffer, FromServerLen);
                                }
                                FromServerLen = 0;
                        }
                        else
                        {
			        while ((loop1 < Link0InLength) && (FromServerLen > 0))
			        {
				        writebyte_int (Link0InDest, FromServerBuffer[loop1]);
				        Link0InDest++;
				        FromServerLen--;
				        loop1++;
			        }

			        /* Moved loop bytes. Correct FromServerBuffer. */
			        for (loop2=0; loop2<FromServerLen; loop2++)
				        FromServerBuffer[loop2] = FromServerBuffer[loop2+loop1];
                        }

                        if (msgdebug || emudebug)
                                printf ("-I-EMUSRV: Link0InLength = #%X, loop = #%X.\n", Link0InLength, loop1);

			/* If message request was fully satisfied, reset channel. */
			Link0InLength = Link0InLength - loop1;
			if (Link0InLength == 0)
			{
                                if (msgdebug || emudebug)
				        printf ("-I-EMUSRV: Comms request satisfied. Reschedule process #%8X.\n", LinkWdesc);

				/* Reset channel. */
                                reset_channel (Link0In);

				/* Reschedule outputting process. */
				schedule (LinkWdesc);

			}
		}
	}
        return activity;
}

int printable (int ch)
{
        return (31 < ch) && (ch < 127) ? ch : '.';
}

void dump_message (char *msg, unsigned char *buffer, int len)
{
        int temp, i;

        if (!msgdebug)
                return;

        if (FALSE == DumpMessage)
                return;

        if (msg)
                printf ("-I-EMUSRV: %s\n", msg);
        printf ("-I-EMUSRV: Message dump.\n");
        for (temp = 0; temp < len; temp += 16)
        {
                for (i = 0; i < 16; i++)
                {
                        if (temp + i < len)
                                printf ("%02X ", buffer[temp + i]);
                        else
                                printf ("   ");
                }
                printf ("    ");
                for (i = 0; i < 16; i++)
                {
                        if (temp + i < len)
                                printf ("%c", printable (buffer[temp + i]));
                }
                printf ("\n");
        }

        DumpMessage = FALSE;
}

void message(void)
{
	unsigned char tag;

	tag = ToServerBuffer[2];
	
#ifdef __MWERKS__
	/* Update stdio stream. */
	check_input();
#endif

        if (msgdebug || emudebug)
	        printf ("-I-EMUSRV: Handling server command. Buffer = %d. Tag = #%02X (%s)\n", ToServerLen, tag, msgtag (tag));

        /* XXX: 1024byte getblock/putblock support */
        /* XXX: correctly respond to non-supported sp function request */

        DumpMessage = TRUE;
        dump_message (0, ToServerBuffer, ToServerLen);

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

		default:        /*	
                                printf ("-E-EMUSRV: Error - bad server command %02X.\n", tag);
                                dump_message (0, ToServerBuffer, ToServerLen);
				handler (-1);
                                */
                                notimpl_packet ();
				break;
	}
        DumpMessage = TRUE;
}


void notimpl_packet (void)
{
	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_NOT_IMPLEMENTED;

	FromServerBuffer[3] = 0;
	FromServerBuffer[4] = 0;
	FromServerBuffer[5] = 0;
	FromServerBuffer[6] = 0;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
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
	FILE *fd;
        unsigned int fdx;

	namelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(namelen+7))
	{
		printf ("-W-EMUSRV: Bad message packet - SP_OPEN - too short.\n");
		error_packet ();
		return;
	}

	if (namelen <= 0)
	{       printf ("-W-EMUSRV: Bad Packet.\n");
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
	{       
                if (emudebug)
                        printf ("-W-EMUSRV: Failed to open %s!\n",filename);
		error_packet ();
		return;
	}
	
#ifdef _MSC_VER
	if (type == SP_BINARY)
		_setmode( _fileno(fd), _O_BINARY);
#endif

	FromServerBuffer[0] = 6;
	FromServerBuffer[1] = 0;

	FromServerBuffer[2] = SP_OK;

	fdx = FromFile (fd);
	Files[fdx].type = type;

	FromServerBuffer[3] = ((unsigned int)fdx) & 0x000000ff;
	FromServerBuffer[4] = (((unsigned int)fdx) & 0x0000ff00) >>  8;
	FromServerBuffer[5] = (((unsigned int)fdx) & 0x00ff0000) >> 16;
	FromServerBuffer[6] = (((unsigned int)fdx) & 0xff000000) >> 24;
	
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}


void sp_close (void)
{
	FILE *fd;
        unsigned int fdx;

        fdx = ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24);
	fd = ToFile (fdx);

	if (fclose(fd) != 0)
	{
		error_packet ();
		return;
	}
        Files[fdx].fd   = (FILE *)NULL;
        Files[fdx].type = -1;

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
		printf ("-W-EMUSRV: Bad message packet - SP_READ - too short.\n");
		error_packet ();
		return;
	}

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	count = ToServerBuffer[7] + (ToServerBuffer[8]<<8);
	if (count > 16384)
	{
		printf ("-W-EMUSRV: SP_READ - Requested %d bytes - too many.\n", count);
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
        unsigned int fdx;

        fdx = ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24);
	fd = ToFile (fdx);

	datalen = ToServerBuffer[7]+(ToServerBuffer[8]<<8);
        if (ToServerLen<(datalen+9))
	{
		printf ("-W-EMUSRV: Bad message packet - SP_WRITE - too short.\n");
		error_packet ();
		return;
	}

	/* Massage output to avoid CRs. */
        if (Files[fdx].type == SP_TEXT)
	        for (pos = 9; pos < (9 + datalen); pos++)
		        if (ToServerBuffer[pos] == 0x0d)
			        ToServerBuffer[pos] = 0x00;

        if (dbgtrigger && (strncmp ((char *) &ToServerBuffer[9], dbgtrigger, strlen (dbgtrigger)) == 0))
        {
                set_debug ();
                dbgtrigger = NULL;
        }

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


void sp_gets (void) /* XXX */
{
	FILE *fd;
	int count;
	char *result;
	int length;
	int temp;

        if (ToServerLen<9)
	{
		printf ("-W-EMUSRV: Bad message packet - SP_GETS - too short.\n");
		error_packet ();
		return;
	}

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	count = ToServerBuffer[7] + (ToServerBuffer[8]<<8);
	if (count > 16384)
	{
		printf ("-W-EMUSRV: SP_GETS - Requested %d bytes - too many.\n", count);
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


void sp_puts (void) /* XXX */
{
	FILE *fd;
	int  datalen;
	int  length;

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

	datalen = ToServerBuffer[7]+(ToServerBuffer[8]<<8);
        if (ToServerLen<(datalen+9))
	{
		printf ("-W-EMUSRV: Bad message packet - SP_PUTS - too short.\n");
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


void sp_flush (void) /* XXX */
{
	FILE *fd;

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

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


void sp_seek (void) /* XXX */
{
	FILE *fd;
	int32_t offset;
	int origin;

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

        if (ToServerLen<15)
	{
		printf ("-W-EMUSRV: Bad message packet - SP_SEEK - too short.\n");
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
	int32_t position;

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

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

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

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

	fd = ToFile (ToServerBuffer[3] + (ToServerBuffer[4]<<8) + (ToServerBuffer[5]<<16) + (ToServerBuffer[6]<<24));

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
		printf ("-W-EMUSRV: Bad message packet - SP_REMOVE - too short.\n");
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
		printf ("-W-EMUSRV: Bad message packet - SP_RENAME - too short.\n");
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
		printf ("-W-EMUSRV: Bad message packet - SP_REMOVE - too short.\n");
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

#ifdef CURTERM
        while (!has_key ());
        ch = getkey ();
#else
#ifndef __MWERKS__
	if (t_state != GETK)
	if ((ioctl (0, TCSETS, &t_getk)) != 0)
	{
		printf ("-E-EMUSRV: Error - bad ioctl (SP_GETKEY).\n");
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

        // fprintf (stderr, "SP_GETKEY: ch = %d (#%X).\n", ch, ch);

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


void sp_pollkey (void) /* XXX */
{
	char ch;

#ifdef CURTERM
        if (has_key ())
                ch = getkey ();
        else
        {
                error_packet ();
                return;
        }
#else
#ifndef __MWERKS__
	if (t_state != POLL)
	if ((ioctl (0, TCSETS, &t_poll)) != 0)
	{
		printf ("-E-EMUSRV: Error - bad ioctl (SP_POLLKEY).\n");
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
        char temp[256];
	char *string;
	int  namelen;

	namelen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(namelen+5))
	{
		printf ("-W-EMUSRV: Bad message packet - SP_GETENV - too short.\n");
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

#if defined(__MWERKS__)
	string = NULL;
#else
	string = getenv (name);
#endif
	if ((0 == strcmp("IBOARDSIZE", name)) && (NULL == string))
	{
		strcpy (temp, "#200000");
		string = temp;
	}

	
	if (string == NULL)
	{
		error_packet ();
		return;
	}

	namelen = strlen (string);
	strncpy ((char *) &FromServerBuffer[5], string, namelen);
        if (emudebug)
                printf ("-I-EMUSRV: SP_GETENV - %s=%s\n", name, string);

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
	int32_t localtime;
	int32_t utctime;
	time_t timezone = 0;

	utctime = time (0);
	if (utctime == -1)
	{
		printf ("-W-EMUSRV: Time call failed - SP_TIME.\n");
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


void sp_system (void) /* XXX */
{
	char command[256];
	int  commandlen;
	int result;

	commandlen = ToServerBuffer[3]+(256*ToServerBuffer[4]);
        if (ToServerLen<(commandlen+7))
	{
		printf ("-W-EMUSRV: Bad message packet - SP_SYSTEM - too short.\n");
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
	int32_t status;

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

        if (emudebug)
	        printf ("-I-EMUSRV: ********** The server has exited **********\n");
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
		length = strlen (CommandLineAll);
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
	uint32_t offset, i;
	uint32_t length;
	extern int peeksize;
	extern int analyse;
	extern unsigned char *core;

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

        for (i = 0; i < length; i++)
                FromServerBuffer[5 + i] = core[offset + i];

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
#ifdef LITTLE_ENDIAN
        /* Sun 386 */
	FromServerBuffer[4] = 7;
#else
	/* Sun 4 */
	FromServerBuffer[4] = 5;
#endif
	/* SunOS */
	FromServerBuffer[5] = 4;
	/* B008 */
	FromServerBuffer[6] = 2;
	FromServerBuffer[7] = 0;

	FromServerLen = 8;
}

