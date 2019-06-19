/* Public domain. */

#ifndef _LINUX_WAIT_BIT_H
#define _LINUX_WAIT_BIT_H

int wait_on_bit(unsigned long *, int, unsigned);
int wait_on_bit_timeout(unsigned long *, int, unsigned, int);
void wake_up_bit(void *, int);

#endif
