/* $OpenBSD: db_structinfo.c,v 1.13 2015/05/05 02:13:46 guenther Exp $ */
/* public domain */
/*
 * This file is intended to be compiled with debug information,
 * which is then translated by parse_debug.awk into support data
 * for ddb.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/device.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/dirent.h>
#include <sys/dkbad.h>
#include <sys/evcount.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/exec.h>
#include <sys/extent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/gpio.h>
#include <sys/hotplug.h>
#include <sys/ipc.h>
#include <sys/kcore.h>
#include <sys/kthread.h>
#include <sys/ktrace.h>
#include <sys/localedef.h>
#include <sys/lock.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/memrange.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/msgbuf.h>
#include <sys/namei.h>
#include <sys/pipe.h>
#include <sys/pool.h>
#include <sys/protosw.h>
#include <sys/ptrace.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/select.h>
#include <sys/selinfo.h>
#include <sys/sem.h>
#include <sys/sensors.h>
#include <sys/shm.h>
#include <sys/siginfo.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/swap.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/timetc.h>
#include <sys/tprintf.h>
#include <sys/tree.h>
#include <sys/tty.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <sys/utsname.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/wait.h>

#include <machine/cpu.h>
#include <machine/conf.h>
#include <machine/mutex.h>

#include <uvm/uvm.h>

/* XXX add filesystem includes there */

#include <sys/ataio.h>
#include <sys/audioio.h>
#include <sys/cdio.h>
#include <sys/chio.h>
#include <sys/dkio.h>
#include <sys/filio.h>
#include <sys/mtio.h>
#include <sys/pciio.h>
#include <sys/radioio.h>
#include <sys/scanio.h>
#include <sys/scsiio.h>
#include <sys/sockio.h>
#include <sys/videoio.h>
