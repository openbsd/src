/* Public domain. */

#ifndef _LINUX_SECURITY_H
#define _LINUX_SECURITY_H

#define LOCKDOWN_PCI_ACCESS 1

static inline int
security_locked_down(int v)
{
	return 1;
}

#endif
