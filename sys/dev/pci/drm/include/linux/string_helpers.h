/* Public domain. */

#ifndef _LINUX_STRING_HELPERS_H
#define _LINUX_STRING_HELPERS_H

#include <linux/string_choices.h>

#define STRING_UNITS_2 0

int string_get_size(uint64_t, uint64_t, int, char *, int);

#endif
