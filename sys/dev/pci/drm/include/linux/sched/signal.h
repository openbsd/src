/* Public domain. */

#ifndef _LINUX_SCHED_SIGNAL_H
#define _LINUX_SCHED_SIGNAL_H

#include <sys/systm.h>
#include <sys/signalvar.h>

#define signal_pending_state(x, y) CURSIG(curproc)
#define signal_pending(y) CURSIG(curproc)

#endif
