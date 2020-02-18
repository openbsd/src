/* Public domain. */

#ifndef _LINUX_SCHED_SIGNAL_H
#define _LINUX_SCHED_SIGNAL_H

#include <sys/systm.h>
#include <sys/signalvar.h>

#define signal_pending_state(x, y) SIGPENDING(curproc)
#define signal_pending(y) SIGPENDING(curproc)

#endif
