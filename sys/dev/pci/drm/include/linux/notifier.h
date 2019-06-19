/* Public domain. */

#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

struct notifier_block {
	void *notifier_call;
};

#define ATOMIC_INIT_NOTIFIER_HEAD(x)

#define NOTIFY_DONE	0
#define NOTIFY_OK	1
#define NOTIFY_BAD	2

#endif
