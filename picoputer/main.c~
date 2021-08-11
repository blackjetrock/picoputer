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
 * main.c
 *
 * Read and parse command line. Start emulator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _MSC_VER
#include <io.h>
#include <fcntl.h>
#endif
#include "processor.h"

#ifdef __MWERKS__
  #include "mac_input.h"
  #include <profiler.h>
#else
  #define true TRUE
  #define false FALSE
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
#endif

#undef TRUE
#undef FALSE
#define TRUE  0x0001
#define FALSE 0x0000

int analyse     = FALSE;
int copy        = FALSE;
int exitonerror = false;
int peeksize    = 8;

int membits     = 0;

int tracing     = 0;
int emudebug    = TRUE;
int memdebug    = FALSE;
int memnotinit  = FALSE;
int msgdebug    = FALSE;
char *dbgtrigger = NULL;
int usetvs      = FALSE;

extern int32_t quit;
extern int32_t quitstatus;

char CommandLineAll[256]  = "\0";
char CommandLineMost[256] = "\0";

FILE *CopyIn;
FILE *InpFile, *OutFile;

extern unsigned char *core;
extern unsigned char *mem;
extern uint32_t WPtr;
extern uint32_t ProcPriority;

/* Profile flag. */
int profiling = false;

/* Profile array. */
uint32_t profile[10];

/* Profile file. */
FILE *ProfileFile;

/* Signal handler. */
void handler (int);

#ifdef __MWERKS__
int argc; /*= 4;*/
char argv[4][40]; /* = {"jserver","-se","-sb","solport.b4"};*/
#else
#ifdef CURTERM
#include "curterm.h"
#else
/* Terminal settings. */
struct termios t_init;
struct termios t_getk;
struct termios t_poll;
enum {INIT, GETK, POLL} t_state;
#endif
#endif

/* 201113AP Gavin Crate */
int isswopt(int c)
{
        return (c == '-') || (c == '/');
}

void set_debug (void)
{
        emudebug = (tracing & 1) ? TRUE : FALSE;
        msgdebug = (tracing & 2) ? TRUE : FALSE;
        memdebug = (tracing & 4) ? TRUE : FALSE;
}

#ifdef __MWERKS__
void main(void)
#else
int main (int argc, char **argv)
#endif
{
	static char CopyFileName[256];
        static char InpFileName[256], OutFileName[256];
        char IBoardSize[32];
	int verbose     = FALSE;
	int reset       = FALSE;
	int serve       = FALSE;
	int arg;
	int temp;
	int temp2;

#ifdef _MSC_VER
	_set_fmode (_O_BINARY);
#endif	

        msgdebug = NULL != getenv ("MSGDEBUG");
        CopyIn = InpFile = OutFile = (FILE *) NULL;

#ifdef __MWERKS__
	/* Create some menus, etc. */
	menu ();
#endif
	if (argc<2)
	{
		printf("\n");
		printf("Usage : t4 [options] [program arguments]\n\n");
		printf("t4 V1.5.1   13/11/2020\n\n");
		printf("Options: use either -sX or /sX\n");
		printf("    -sa                  Analyse transputer.\n");
		printf("    -sb filename         Boot program \"filename\".\n");
		printf("    -sc filename         Copy file \"filename\" to transputer.\n");
		printf("    -se                  Terminate on transputer error.\n");
		printf("    -si                  Output progress messages.\n");
		printf("    -sr                  Reset transputer.\n");
		printf("    -sp number           Peek \"number\" kilobytes on analyse.\n");
		printf("    -ss                  Provide host services to transputer.\n");
                printf("Extra options:\n");
                printf("    -s4                  Select T414 mode. (default)\n");
                printf("    -s8                  Select T800 mode.\n");
                printf("    -sg                  Halt on uninitialized memory read.\n");
                printf("    -sm #bits            Memory size in address bits (default 21, 2Mbyte).\n");
                printf("    -su                  Instruction profiling.\n");
                printf("    -sv inp.tbo inp.bin out.bin\n");
                printf("                         Select Mike's TVS: T800 + T414 FP support.\n");
                printf("    -sw \"string\"         Trigger execution trace on SP_WRITE (string).\n");
                printf("    -sx [number]         Execution trace (4 - mem ld/st, 2 - iserver, 1 - instructions).\n");
		printf("\n");
		handler (-1);
	}

	/* Set up signal handler. */
	signal (SIGINT, handler);

	/* Build full command line string for server to return on request. */
	for (arg=0;arg<argc;arg++)
	{
		if (arg>0) strcat(CommandLineAll, " ");
		strcat(CommandLineAll, argv[arg]);
	}
        if (emudebug)
	  printf("\nFull command line is : %s\n", CommandLineAll);

	for (arg=1;arg<argc;arg++)
	{
		if (isswopt(argv[arg][0]) && (argv[arg][1]=='s'))
		{
			switch (argv[arg][2])
			{
				case '4': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
                                          {
                                                Txxx=414;
                                                CoreSize    = 2 * 1024;
                                                MemStart    = 0x80000048;
                                                ExtMemStart = 0x80000800;
                                          }
					  break;
				case '8': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
                                          {
                                                Txxx=800;
                                                CoreSize    = 4 * 1024;
                                                MemStart    = 0x80000070;
                                                ExtMemStart = 0x80001000;
                                          }
					  break;
				case 'a': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else analyse=TRUE;
					  break;
				case 'b': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						arg++;
						if (arg>=argc)
						{
							printf("\nMissing filename after -sb\n");
							handler (-1);
						}
						strcpy (CopyFileName, argv[arg]);
						reset=TRUE;
						serve=TRUE;
						verbose=TRUE;
						copy=TRUE;
					  }
					  break;
				case 'c': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						arg++;
						if (arg>=argc)
						{
							printf("\nMissing filename after -sc\n");
							handler (-1);
						}
						strcpy (CopyFileName, argv[arg]);
						copy=TRUE;
					  }
					  break;
				case 'e': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else exitonerror=true;
					  break;
				case 'g': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else memnotinit=true;
					  break;
				case 'i': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else verbose=TRUE;
					  break;
				case 'm': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
                                          else
                                          {
						arg++;
						if (arg>=argc)
						{
							printf ("\nMissing number after -sm\n");
							handler (-1);
						}
						if (sscanf(argv[arg], "%d", &membits)!=1)
						{
							printf ("\nBad number after -sm\n");
							handler (-1);
						}
						if ((membits < 16) || (membits > 30))
						{
							printf ("\nMemory bits should be in [16,30] range!\n");
							handler (-1);
						}
                                          }
					  break;
				case 'r': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else reset=TRUE;
					  break;
				case 'p': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						arg++;
						if (arg>=argc)
						{
							printf("\nMissing number after -sp\n");
							handler (-1);
						}
						if (sscanf(argv[arg], "%d", &peeksize)!=1)
						{
							printf("\nBad number after -sp\n");
							handler (-1);
						}
						if (peeksize > 16)
						{
							printf("\nCannot peek that much memory.\n");
							handler (-1);
						}
					  }
					  break;
				case 's': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else serve=TRUE;
					  break;
				case 'u': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else profiling=true;
					  break;
                                case 'v': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						if (arg + 3 >= argc)
						{
							printf("\nMissing parameters after -sv\n");
							handler (-1);
						}
                                                strcpy (CopyFileName, argv[arg + 1]);
                                                strcpy (InpFileName, argv[arg + 2]);
                                                strcpy (OutFileName, argv[arg + 3]);
                                                arg += 3;
                                                usetvs = TRUE;
                                                Txxx   = 1144;
                                                CoreSize    = 4 * 1024;
                                                MemStart    = 0x80000070;
                                                ExtMemStart = 0x80001000;
					  }
					  break;
				case 'w': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						arg++;
						if (arg>=argc)
						{
							printf("\nMissing string after -sw\n");
							handler (-1);
						}
                                                dbgtrigger = argv[arg];
					  }
					  break;
				case 'x': if (argv[arg][3]!='\0')
					  {
						strcat (CommandLineMost, argv[arg]);
						strcat (CommandLineMost, " ");
					  }
					  else
					  {
						arg++;
						if (arg >= argc)
                                                        tracing = 1;
                                                else if (isswopt(argv[arg][0]))
                                                {
                                                        tracing = 1;
                                                        arg--;
                                                }
						else 
						{
                                                        if (sscanf(argv[arg], "%d", &tracing)!=1)
                                                        {
							        printf("\nBad number after -sx\n");
							        handler (-1);
                                                        }
						}
					  }
					  break;
				default : strcat(CommandLineMost, argv[arg]);
					  strcat (CommandLineMost, " ");
					  break;
			}
		}
		else
		{
			strcat(CommandLineMost, argv[arg]);
			strcat (CommandLineMost, " ");
		}
	}
	if (strlen(CommandLineMost))
		CommandLineMost[strlen(CommandLineMost)-1] = '\0';

        /* Allocate internal memory. */
	core = malloc (CoreSize);
	if (core == NULL)
	{
		printf ("\nFailed to allocate internal memory!\n");
		exit (-1);
	}

        /* Recalculate external memory settings, when membits set. */
        if (membits)
        {
                MemSize     = 1U << membits;
                MemByteMask = MemSize - 1;
                MemWordMask = MemByteMask & (~3U);
        }

        /* Set IBOARDSIZE to MemSize. */
        sprintf (IBoardSize, "IBOARDSIZE=#%X", MemSize);
        putenv (IBoardSize);

        /* Allocate external memory. */
	mem = malloc (MemSize);
	if (mem == NULL)
	{
		printf ("\nFailed to allocate %dK of external memory!\n", MemSize / 1024);
		exit (-1);
	}

        if (tracing && (dbgtrigger == NULL))
                set_debug ();

        if (emudebug)
        {
	        printf("Most command line is : %s\n", CommandLineMost);
	        printf("T%d (%d/%dKB); analyse %d; copy %d; exit %d; verbose %d; reset %d; peek %d; serve %d\n",
                        Txxx,CoreSize/1024,MemSize/1024,analyse,copy,exitonerror,verbose,reset,peeksize,serve);
        }

        /* Initialize memory with 'random' values. */
        init_memory ();

	/* Open boot file. */
	if ((CopyIn = fopen(CopyFileName, "rb"))==NULL)
	{
		printf("Failed to open file %s for copy to link!\n", CopyFileName);
		handler (-1);
	}

	/* Load bootstrap into emulator. */
        if (usetvs)
        {
                printf ("-I-EMUTVS: Running test %s\n", CopyFileName);
                /* Optional InpFileName. */
                if ((0 != strlen (InpFileName)) &&
                    (InpFile = fopen (InpFileName, "rb")) == NULL)
                {
                        printf ("-E-EMUTVS: Error - failed to open file %s for input!\n", InpFileName);
                        handler (-1);
                }
                if ((OutFile = fopen (OutFileName, "wb")) == NULL)
                {
                        printf ("-E-EMUTVS: Error - failed to open file %s for output!\n", OutFileName);
                        handler (-1);
                }
        }

        /* TVS tbo code is bootstrap code. */
	temp = getc (CopyIn);
	if (temp < 2)
	{
		printf ("\nFile does not start with bootstrap code!\n");
		handler (-1);
	}

	for (temp2=0; temp2<temp; temp2++)
	{
		writebyte_int ((MemStart+temp2), getc (CopyIn));
	}
	WPtr = MemStart + temp2;
	while ((WPtr & 0x00000003) != 0x00000000)
		WPtr++;
	ProcPriority = LoPriority;

	/* Initialise profiling array. */
	for (temp=0; temp<10; temp++)
		profile[temp] = 0;

#ifdef CURTERM
        /* Initialise terminal settings. */
        prepterm (1);
#else
#ifndef __MWERKS__
	/* Initialise t_init terminal settings. */
	if ((ioctl (0, TCGETS, &t_init)) != 0)
	{
		printf("\nBad ioctl - initialising t_init.\n");
		handler (-1);
	}
	t_state = INIT;

	/* Initialise t_getk terminal settings. */
	if ((ioctl (0, TCGETS, &t_getk)) != 0)
	{
		printf("\nBad ioctl - initialising t_getk.\n");
		handler (-1);
	}
	/* Turn off buffering and echoing. */
	t_getk.c_iflag &= ~ICRNL;
	t_getk.c_lflag &= ~(ICANON | ECHO);
	/* Get at least 1 char, wait as long as you have to. */
	t_getk.c_cc[VMIN] = 1;
	t_getk.c_cc[VTIME] = 0;

	/* Initialise t_poll terminal settings. */
	if ((ioctl (0, TCGETS, &t_poll)) != 0)
	{
		printf("\nBad ioctl - initialising t_poll.\n");
		handler (-1);
	}
	/* No buffering, no echoing. */
	t_poll.c_iflag &= ~ICRNL;
	t_poll.c_lflag &= ~(ICANON | ECHO);
	/* Don't necessarily get a charcter, wait 0.1s. */
	t_poll.c_cc[VMIN] = 0;
	t_poll.c_cc[VTIME] = 1;
#endif
#endif

	if (profiling)
	{
		/* Open profiling file. */
		if ((ProfileFile = fopen("profile", "w"))==NULL)
		{
			printf("Failed to open profile file!\n");
			handler (-1);
		}
	}


#if __profile__
	/* METROWERKS PROFILER. */
	if (ProfilerInit(collectDetailed,bestTimeBase,10,10) == noErr)
	{
#endif

	/* Now start the emulator. */
	mainloop ();

#if __profile__
	/* METROWERKS PROFILER. */
		ProfilerDump("\pTutorial.prof");
		ProfilerTerm();
	}
#endif

	/* Close boot file. */
	fclose (CopyIn);
        if (InpFile)
                fclose (InpFile);
        if (OutFile)
                fclose (OutFile);

	if (profiling)
	{
		/* Print out profile counts. */
                fprintf (ProfileFile, "\nInstructions           %9u\n", profile[0]);
                fprintf (ProfileFile, "Descheduling points    %9u\n", profile[1]);
                fprintf (ProfileFile, "Server calls           %9u\n", profile[2]);
                fprintf (ProfileFile, "StartProcess calls     %9u\n\n", profile[3]);

		print_profile ();

		fclose (ProfileFile);
	}

#ifdef CURTERM
        prepterm (0);
#else
#ifndef __MWERKS__
	if (t_state != INIT)
	{
		if ((ioctl (0, TCSETS, &t_init)) != 0)
		{
			printf("\nBad ioctl while resetting terminal.\n");
			handler (-1);
		}
	}
#endif
#endif

#ifdef __MWERKS__
	while (1)
		check_input ();
#endif
}


void handler (int signal)
{
#ifdef __MWERKS__
	DialogPtr dialog;
	short itemHit = 0;
#endif

	if (profiling)
		fclose (ProfileFile);

        if (CopyIn)
                fclose (CopyIn);
        if (InpFile)
                fclose (InpFile);
        if (OutFile)
                fclose (OutFile);

#ifdef CURTERM
   	prepterm (0);
	exit (-1);
#else
#ifdef __MWERKS__
	/* Exit dialog. */
	dialog = GetNewDialog (129, nil, (void *) -1);
	
	while (itemHit == 0)
	{
		ModalDialog (nil, &itemHit);
	}
	
	DisposDialog (dialog);

	free (mem);
	ExitToShell ();
#else
	if (t_state != INIT)
	{
		if ((ioctl (0, TCSETS, &t_init)) != 0)
		{
			printf("\nBad ioctl while resetting terminal.\n");
			/* handler (-1); */
		}
	}

	exit (-1);
#endif
#endif
}
