/* Public domain. */

#ifndef _LINUX_POWER_SUPPLY_H
#define _LINUX_POWER_SUPPLY_H

static inline int
power_supply_is_system_supplied(void)
{
	/* XXX return 0 if on battery */
	return (1);
}

#endif
