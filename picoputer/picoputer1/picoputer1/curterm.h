#ifndef _CURTERM_H
#define _CURTERM_H

void prepterm(int);
int	 has_key(void);
int  getkey(void);
void kbflush(void);
int  has_ctrlc(void);
void cbreak(int enable);

void gotoxy(int x, int y);
void clrscr(void);

#endif
