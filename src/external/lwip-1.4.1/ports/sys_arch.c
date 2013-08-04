#include "lwip/debug.h"
#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

#if 0
static struct timeval starttime;

void sys_init(void)
{
	gettimeofday(&starttime, NULL);
}

u32_t sys_now(void)
{
	struct timeval tv;
	u32_t sec, usec, msec;

	gettimeofday(&tv, NULL);

	sec = (u32_t) (tv.tv_sec - starttime.tv_sec);
	usec = (u32_t) (tv.tv_usec - starttime.tv_usec);
	msec = sec * 1000 + usec / 1000;

	return msec;
}

#ifndef MAX_JIFFY_OFFSET
#define MAX_JIFFY_OFFSET ((~0U >> 1)-1)
#endif

#ifndef HZ
#define HZ 100
#endif

u32_t sys_jiffies(void)
{
	struct timeval tv;
	unsigned long sec;
	long usec;

	gettimeofday(&tv, NULL);
	sec = tv.tv_sec - starttime.tv_sec;
	usec = tv.tv_usec;

	if (sec >= (MAX_JIFFY_OFFSET / HZ))
		return MAX_JIFFY_OFFSET;
	usec += 1000000L / HZ - 1;
	usec /= 1000000L / HZ;
	return HZ * sec + usec;
}
#else
void sys_init(void)
{
}

u32_t sys_now(void)
{
	return jiffies;
}

u32_t sys_jiffies(void)
{
	return jiffies;
}
#endif
