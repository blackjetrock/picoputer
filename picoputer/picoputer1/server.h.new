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
 * server.h
 *
 * SP protocol definitions. Server function prototypes.
 *
 */

#define SP_OPEN        10
#define SP_CLOSE       11
#define SP_READ        12
#define SP_WRITE       13
#define SP_GETS        14
#define SP_PUTS        15
#define SP_FLUSH       16
#define SP_SEEK        17
#define SP_TELL        18
#define SP_EOF         19
#define SP_FERROR      20
#define SP_REMOVE      21
#define SP_RENAME      22

#define SP_GETKEY      30
#define SP_POLLKEY     31
#define SP_GETENV      32
#define SP_TIME        33
#define SP_SYSTEM      34
#define SP_EXIT        35

#define SP_COMMANDLINE 40
#define SP_CORE        41
#define SP_VERSION     42

#define SP_DOS         50
#define SP_HELIOS      51
#define SP_VMS         52
#define SP_SUNOS       53
#define SP_TDS         65
#define SP_OTHER       128


#define SP_OK               0
#define SP_NOT_IMPLEMENTED  1
#define SP_ERROR            128


#define SP_BINARY 1
#define SP_TEXT   2


#define SP_INPUT           1
#define SP_OUTPUT          2
#define SP_APPEND          3
#define SP_EXISTING_UPDATE 4
#define SP_NEW_UPDATE      5
#define SP_APPEND_UPDATE   6


/* Function prototypes. */
int server(void);
void message(void);
void error_packet (void);
void sp_open (void);
void sp_close (void);
void sp_read (void);
void sp_write (void);
void sp_gets (void) ;
void sp_puts (void);
void sp_flush (void);
void sp_seek (void);
void sp_tell (void);
void sp_eof (void);
void sp_ferror (void);
void sp_remove (void);
void sp_rename (void);

void sp_getkey (void);
void sp_pollkey (void);
void sp_getenv (void);
void sp_time (void);
void sp_system (void);
void sp_exit (void);

void sp_commandline (void);
void sp_core (void);
void sp_version (void);

