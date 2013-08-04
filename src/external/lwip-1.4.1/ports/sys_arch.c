#include "lwip/debug.h"
#include "lwip/sys.h"
#include "lwip/opt.h"
#include "lwip/stats.h"

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
