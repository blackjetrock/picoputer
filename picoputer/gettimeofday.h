#ifndef _GETTIMEOFDAY_H
#define _GETTIMEOFDAY_H

#define WIN32_LEAN_AND_MEAN	1
#include <windows.h>
#include <winsock2.h>

struct timezone
{
  	int  tz_minuteswest; /* minutes W of Greenwich */
  	int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz);

#endif
