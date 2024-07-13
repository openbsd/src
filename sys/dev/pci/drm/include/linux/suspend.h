/* Public domain. */

#ifndef _LINUX_SUSPEND_H
#define _LINUX_SUSPEND_H

typedef int suspend_state_t;

#define PM_SUSPEND_ON		0
#define PM_SUSPEND_MEM		1
#define PM_SUSPEND_TO_IDLE	2

extern suspend_state_t pm_suspend_target_state;

#endif
