#	$OpenBSD: Makefile,v 1.15 2000/03/17 22:24:27 jakob Exp $
#       $NetBSD: Makefile,v 1.4 1996/01/09 03:23:01 thorpej Exp $

#	Makefile for section 9 (kernel function and variable) manual pages.

MAN=	boot.9 copy.9 disk.9 doshutdownhooks.9 fetch.9 fork1.9 \
	extent.9 hz.9 hzto.9 intro.9 inittodr.9 log.9 kthread.9 \
	malloc.9 md5.9 microtime.9 panic.9 pfind.9 printf.9 \
	psignal.9 ratecheck.9 resettodr.9 \
	random.9 shutdownhook_establish.9 sleep.9 spl.9 store.9 style.9 \
	time.9 timeout.9 vm_allocate.9 vm_map_copy.9 vm_deallocate.9 \
	vm_map_inherit.9 vm_map_protect.9

MLINKS+=copy.9 copyin.9 copy.9 copyout.9 copy.9 copystr.9 \
	copy.9 copyinstr.9 copy.9 copyoutstr.9
MLINKS+=disk.9 disk_init.9 disk.9 disk_attach.9 disk.9 disk_detatch.9 \
	disk.9 disk_busy.9 disk.9 disk_unbusy.9 disk.9 disk_find.9 \
	disk.9 disk_resetstat.9
MLINKS+=extent.9 extent_create.9 extent.9 extent_destroy.9 \
	extent.9 extent_alloc.9 extent.9 extent_alloc_subregion.9 \
	extent.9 extent_alloc_region.9 extent.9 extent_free.9 \
	extent.9 extent_free.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuibyte.9 fetch.9 fusword.9 \
	fetch.9 fuswintr.9 fetch.9 fuword.9 fetch.9 fuiword.9
MLINKS+=kthread.9 kthread_create.9 kthread.9 kthread_exit.9 \
	kthread.9 kthread_create_deferred.9
MLINKS+=log.9 addlog.9
MLINKS+=md5.9 MD5Init.9 md5.9 MD5Transform.9
MLINKS+=pfind.9 pgfind.9
MLINKS+=printf.9 sprintf.9 printf.9 vprintf.9 printf.9 uprintf.9 \
	printf.9 ttyprintf.9 printf.9 db_printf.9
MLINKS+=psignal.9 pgsignal.9 psignal.9 gsignal.9
MLINKS+=shutdownhook_establish.9 shutdownhook_disestablish.9
MLINKS+=sleep.9 tsleep.9 sleep.9 wakeup.9
MLINKS+=spl.9 spl0.9 spl.9 splbio.9 spl.9 splclock.9 spl.9 splhigh.9 \
	spl.9 splimp.9 spl.9 splnet.9 spl.9 splsched.9 spl.9 splserial.9 \
	spl.9 splsoftclock.9 spl.9 splsoftnet.9 spl.9 splsoftserial.9 \
	spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splx.9
MLINKS+=store.9 subyte.9 store.9 suibyte.9 store.9 susword.9 \
	store.9 suswintr.9 store.9 suword.9 store.9 suiword.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 untimeout.9
MLINKS+=vm_map_copy.9 vm_copy.9
MLINKS+=vm_map_inherit.9 vm_inherit.9
MLINKS+=vm_map_protect.9 vm_protect.9

.include <bsd.prog.mk>
