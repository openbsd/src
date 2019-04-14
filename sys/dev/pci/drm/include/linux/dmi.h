/* Public domain. */

#ifndef _LINUX_DMI_H
#define _LINUX_DMI_H

#include <sys/types.h>
#include <linux/mod_devicetable.h>

int dmi_check_system(const struct dmi_system_id *);
bool dmi_match(int, const char *);

#endif
