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
 * Macintosh stdin equivalents.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Types.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Menus.h>
#include <Windows.h>
#include <SegLoad.h>
#include <Scrap.h>
#include <SIOUX.h>
#include <LowMem.h>
#include "mac_input.h"

/* Globals */
extern unsigned char *mem;
char input_buf[MYBUFSIZ];
int  input_buf_count;
int  init_input = false;

void doAboutBox (void);

/* Update inbuf routine. */
void check_input (void)
{
	int value;
	int key;
	int modifier;
	int menu;
	WindowPtr window;
	EventRecord input_event;
	Str255 name;
	Handle scrap;
	long offset;
	int length;
	
	if (init_input == false)
	{
		/* Initialise event systems etc.. */
		input_buf_count = 0;
		init_input = true;
	}

	/* Avoid overflow. */
	if (input_buf_count == (MYBUFSIZ - 1))
		return;
	
	/* Look for keydown events. */
	value = GetNextEvent (everyEvent, &input_event);
	if (value == false)
		return;
							
	/* Must be an event of interest. */
	switch (input_event.what)
	{
		case autoKey:
		case keyDown:	key = (input_event.message & charCodeMask);
						modifier = (input_event.modifiers & cmdKey);
						if (modifier == cmdKey)
						{
							menu = MenuKey (key);
							HiliteMenu (0);
							switch ((menu & 0xffff0000) >> 16)
							{
								case 0:		/* Not a menu equivalent. */
											break;
								case 128:	/* Apple Menu. */
											if ((menu & 0xffff) == 1)
												doAboutBox ();
											else
											{
												GetItem (GetMHandle (128), (menu & 0xffff), name);
												OpenDeskAcc (name);
											}
											break;
								case 129:	/* File Menu. */
											if ((menu & 0xffff) == 3)
											{
												free (mem);
												ExitToShell ();
											}
											break;
								case 130:	/* Edit Menu. */
											if ((menu & 0xffff) == 5)
											{
												scrap = NewHandle (0);
												if ((length = GetScrap (scrap, 'TEXT', &offset)) > 0)
												{
													/* Put the data into the input buffer. */
													strncpy (&input_buf[input_buf_count], (char *) *scrap, length);
													input_buf_count = input_buf_count + length;
													/*for (offset = 0; offset < length; offset++)
														printf ("%c", *(offset + (char *) *scrap));*/
												}
												DisposHandle (scrap);
											}
											/* SIOUX handles Copy. */
											else if ((menu & 0xffff) == 4)
											{
												SIOUXHandleOneEvent(&input_event);
											}
											break;
								default:	break;
							}
						}
						else
						{
							/* Put character into input buffer. */
							input_buf[input_buf_count] = key;
							input_buf_count++;
						}
						break;
		case mouseDown:	value = FindWindow (input_event.where, &window);
						if (value == 1)
						{
							menu = MenuSelect (input_event.where);
							switch ((menu & 0xffff0000) >> 16)
							{
								case 0:		break;
								case 128:	/* Apple Menu. */
											if ((menu & 0xffff) == 1)
												doAboutBox ();
											else
											{
												GetItem (GetMHandle (128), (menu & 0xffff), name);
												OpenDeskAcc (name);
											}
											HiliteMenu (0);
											break;
								case 129:	/* File Menu. */
											HiliteMenu (0);
											if ((menu & 0xffff) == 3)
											{
												free (mem);
												ExitToShell ();
											}
											break;
								case 130:	/* Edit Menu. */
											if ((menu & 0xffff) == 5)
											{
												scrap = NewHandle (0);
												if ((length = GetScrap (scrap, 'TEXT', &offset)) > 0)
												{
													/* Put the data into the input buffer. */
													strncpy (&input_buf[input_buf_count], (char *) **scrap, length);
													input_buf_count = input_buf_count + length;
													/*for (offset = 0; offset < length; offset++)
														printf ("%c", *(offset + (char *) *scrap));*/
												}
												DisposHandle (scrap);
											}
											/* SIOUX handles Copy. */
											else if ((menu & 0xffff) == 4)
											{
												/* Send SIOUX a cmd-c event! */
												input_event.what = keyDown;
												input_event.message = 0x20863;
												input_event.modifiers = 0x0180;
												SIOUXHandleOneEvent(&input_event);
												
												/* Send SIOUX a cmd-c key up event! 
												input_event.what = keyUp;
												input_event.message = 0x20863;
												input_event.modifiers = 0x0180;
												SIOUXHandleOneEvent(&input_event);*/
											}
											HiliteMenu (0);
											break;
								default:	break;
							}
						}
						else
						{
							SIOUXHandleOneEvent(&input_event);
						}
						break;
		case mouseUp:
		case updateEvt:	SIOUXHandleOneEvent(&input_event);
						break;
		case keyUp:		break;
		default:		SIOUXHandleOneEvent(&input_event);
						break;
	}
}

/* Getchar equivalent routine. Blocking keyboard input. */
int mygetchar (void)
{
	int character;

	/* Is there a character waiting? */
	while (input_buf_count == 0)
	{
		/* No character waiting. Wait until one arrives. */
		check_input ();
	}
	
	/* Return the character. */
	character = input_buf[0];
	input_buf_count--;
	memmove (&input_buf[0], &input_buf[1], input_buf_count);
	
	return (character);
}

/* Non-blocking getchar routine. */
int mypollchar (void)
{
	int character;

	/* Check for no inout available. */
	if (input_buf_count == 0)
		return (-1);
	
	/* Return the waiting character. */
	character = input_buf[0];
	input_buf_count--;
	memmove (&input_buf[0], &input_buf[1], input_buf_count);
	
	return (character);
}

void doAboutBox (void)
{
	DialogPtr dialog;
	short itemHit = 0;

	dialog = GetNewDialog (128, nil, (void *) -1);
	
	while (itemHit == 0)
	{
		ModalDialog (nil, &itemHit);
	}
	
	DisposDialog (dialog);
}

void doOpenDialog (int *done)
{
	Point where = {50, 50};
	Str255 obsolete;
	SFReply which;
	extern int argc;
	extern char argv[4][40];
	extern int exitonerror;
	short vRefNum;
	long parID, ProcID;

	SFGetFile (where, obsolete, nil, -1, nil, nil, &which);

	if (which.good == 1)
	{
		/* Set up bootable file name. */
		if (exitonerror)
		{
			argc = 4;
			strcpy (argv[0], "jserver");
			strcpy (argv[1], "-se");
			strcpy (argv[2], "-sb");
			strncpy (argv[3], (char *) (which.fName + 1), StrLength(which.fName));
			argv[3][StrLength (which.fName)] = '\0';
		}
		else
		{
			argc = 3;
			strcpy (argv[0], "jserver");
			strcpy (argv[1], "-sb");
			strncpy (argv[2], (char *) (which.fName + 1), StrLength(which.fName));
			argv[2][StrLength(which.fName)] = '\0';
		}
		
		*done = true;
	}
	
	/* Set working directory. */
	if (!GetWDInfo (which.vRefNum, &vRefNum, &parID, &ProcID))
		HSetVol (NULL, vRefNum, parID);
}

void menu (void)
{
	int done = false;
	int key;
	int modifier;
	int value;
	int menu;
	Handle menubar;
	WindowPtr window;
	EventRecord input_event;
	Str255 name;
	SInt32 wd;
	
	extern int exitonerror;
	extern int profiling;
	
	extern tSIOUXSettings	SIOUXSettings;

	/* Initialise. */
	MaxApplZone ();
	MoreMasters ();
	InitGraf (&qd.thePort);
	InitFonts ();
	InitWindows ();
	InitMenus ();
	TEInit ();
	InitDialogs (nil);
	InitCursor ();
	
	FlushEvents (everyEvent, 0);
	menubar = GetNewMBar (128);
	SetMenuBar (menubar);
	//DisposHandle (menubar);
	AddResMenu (GetMHandle (128), 'DRVR');
	DrawMenuBar ();
	
	/*SetSIOUXBufferMode (SIOUXLineBuffering);*/
	
	/* Malloc 2M transputer memory space. */
	mem = (unsigned char *) malloc (2*1024*1024);
	if (mem == NULL)
		ExitToShell ();

	while (!done)
	{
		/* Look for events. */
		value = GetNextEvent (everyEvent, &input_event);

		if (value != false)
		{	
			switch (input_event.what)
			{
				case nullEvent:	break;
				case keyDown:	key = (input_event.message & charCodeMask);
								modifier = (input_event.modifiers & cmdKey);
								if (modifier == cmdKey)
								{
									menu = MenuKey (key);
									HiliteMenu (0);
									switch ((menu & 0xffff0000) >> 16)
									{
										case 0:		break;
										case 128:	/* Apple Menu. */
													if ((menu & 0xffff) == 1)
														doAboutBox ();
													else
													{
														GetItem (GetMHandle (128), (menu & 0xffff), name);
														OpenDeskAcc (name);
													}
													break;
										case 129:	/* File Menu. */
													if ((menu & 0xffff) == 1)
														doOpenDialog (&done);
													else if ((menu & 0xffff) == 3)
													{
														free (mem);
														ExitToShell ();
													}
													break;
										case 131:	/* Options Menu. */
													if ((menu & 0xffff) == 1)
													{
														/* Toggle -se. */
														if (exitonerror == true)
														{
															exitonerror = false;
															CheckItem (GetMHandle (131), 1, false);
														}
														else
														{
															exitonerror = true;
															CheckItem (GetMHandle (131), 1, true);
														}
													}
													else if ((menu & 0xffff) == 2)
													{
														/* Toggle profiling. */
														if (profiling == true)
														{
															profiling = false;
															CheckItem (GetMHandle (131), 2, false);
														}
														else
														{
															profiling = true;
															CheckItem (GetMHandle (131), 2, true);
														}
													}
													break;
										default:	break;
									}
								}
								break;
				case mouseDown:	value = FindWindow (input_event.where, &window);
								if (value == inMenuBar)
								{
									menu = MenuSelect (input_event.where);
									switch ((menu & 0xffff0000) >> 16)
									{
										case 0:		break;
										case 128:	/* Apple Menu. */
													if ((menu & 0xffff) == 1)
														doAboutBox ();
													else
													{
														GetItem (GetMHandle (128), (menu & 0xffff), name);
														OpenDeskAcc (name);
													}
													HiliteMenu (0);
													break;
										case 129:	/* File Menu. */
													HiliteMenu (0);
													if ((menu & 0xffff) == 1)
														doOpenDialog (&done);
													else if ((menu & 0xffff) == 3)
													{
														free (mem);
														ExitToShell ();
													}
													break;
										case 131:	/* Options Menu. */
													if ((menu & 0xffff) == 1)
													{
														/* Toggle -se. */
														if (exitonerror == true)
														{
															exitonerror = false;
															CheckItem (GetMHandle (131), 1, false);
														}
														else
														{
															exitonerror = true;
															CheckItem (GetMHandle (131), 1, true);
														}
													}
													else if ((menu & 0xffff) == 2)
													{
														/* Toggle profiling. */
														if (profiling == true)
														{
															profiling = false;
															CheckItem (GetMHandle (131), 2, false);
														}
														else
														{
															profiling = true;
															CheckItem (GetMHandle (131), 2, true);
														}
													}
													HiliteMenu (0);
													break;
										default:	break;
									}
								}
								else if (value == inSysWindow)
									SystemClick (&input_event, window);
								break;
				default:		break;
			}
		}
	}
	
	/* Stop SIOUX creating its own menus? */
	SIOUXSettings.initializeTB = false;
	SIOUXSettings.setupmenus = false;
	SIOUXSettings.showstatusline = false;
	
	/* Since SIOUX creates its own menus, trigger it and then recreate our menus. */
	printf ("\n");
	ClearMenuBar ();
	DrawMenuBar ();
	menubar = GetNewMBar (128);
	SetMenuBar (menubar);
	//DisposHandle (menubar);
	
	/* Disable Open. */
	DisableItem (GetMHandle (129), 1);
	
	/* Disable Options Menu. */
	DisableItem (GetMHandle (131), 0);
	
	DrawMenuBar ();
	
	/* Initialise the scrap manager. */
	if (ZeroScrap () != 0)
		exit (-1);
}
