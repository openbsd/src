#	$OpenBSD: Makefile,v 1.56 2002/08/24 03:34:20 wcobb Exp $
#	$NetBSD: Makefile,v 1.4 1996/01/09 03:23:01 thorpej Exp $

#	Makefile for section 9 (kernel function and variable) manual pages.

MAN=	altq.9 audio.9 boot.9 bus_dma.9 bus_space.9 copy.9 crypto.9 ctxsw.9 \
	disk.9 disklabel.9 \
	dohooks.9 dopowerhooks.9 doshutdownhooks.9 dostartuphooks.9 \
	extattr.9 file.9 \
	fetch.9 fork1.9 extent.9 getdevvp.9 getnewvnode.9 hash.9 hashinit.9 \
	hardclock.9 hook_establish.9 hz.9 hzto.9 intro.9 inittodr.9 log.9 \
	kern.9 kthread.9 lock.9 malloc.9 mbuf.9 mbuf_tags.9 md5.9 microtime.9 \
	panic.9 pfind.9 physio.9 \
	pool.9 powerhook_establish.9 ppsratecheck.9 printf.9 psignal.9 \
	radio.9 random.9 rasops.9 ratecheck.9 resettodr.9 \
	shutdownhook_establish.9 sleep.9 spl.9 startuphook_establish.9 \
	store.9 style.9 \
	time.9 timeout.9 tvtohz.9 uiomove.9 uvm.9 \
	vaccess.9 vclean.9 vcount.9 vdevgone.9 vfinddev.9 vflush.9 vget.9 \
	vgone.9 vhold.9 vinvalbuf.9 vput.9 vref.9 vrele.9 \
	vm_allocate.9 vm_map_copy.9 vm_deallocate.9 \
	vm_map_inherit.9 vm_map_protect.9 vnode.9 vn_lock.9 \
	vwaitforio.9 vwaitforio.9

MLINKS+=bus_dma.9 bus_dmamap_create.9 bus_dma.9 bus_dmamap_destroy.9 \
	bus_dma.9 bus_dmamap_load.9 bus_dma.9 bus_dmamap_load_mbuf.9 \
	bus_dma.9 bus_dmamap_load_uio.9 bus_dma.9 bus_dmamap_load_raw.9 \
	bus_dma.9 bus_dmamap_unload.9 bus_dma.9 bus_dmamap_sync.9 \
	bus_dma.9 bus_dmamem_alloc.9 bus_dma.9 bus_dmamem_free.9 \
	bus_dma.9 bus_dmamem_map.9 bus_dma.9 bus_dmamem_unmap.9 \
	bus_dma.9 bus_dmamem_mmap.9
MLINKS+=bus_space.9 bus_space_alloc.9 bus_space.9 bus_space_barrier.9 \
	bus_space.9 bus_space_copy_1.9 bus_space.9 bus_space_copy_2.9 \
	bus_space.9 bus_space_copy_4.9 bus_space.9 bus_space_copy_8.9 \
	bus_space.9 bus_space_free.9 bus_space.9 bus_space_map.9 \
	bus_space.9 bus_space_read_1.9 bus_space.9 bus_space_read_2.9 \
	bus_space.9 bus_space_read_4.9 bus_space.9 bus_space_read_8.9 \
	bus_space.9 bus_space_read_multi_1.9 bus_space.9 bus_space_read_multi_2.9 \
	bus_space.9 bus_space_read_multi_4.9 bus_space.9 bus_space_read_multi_8.9 \
	bus_space.9 bus_space_read_raw_multi_2.9 bus_space.9 bus_space_read_raw_multi_4.9 \
	bus_space.9 bus_space_read_raw_multi_8.9 bus_space.9 bus_space_read_region_1.9 \
	bus_space.9 bus_space_read_region_2.9 bus_space.9 bus_space_read_region_4.9 \
	bus_space.9 bus_space_read_region_8.9 bus_space.9 bus_space_read_raw_region_2.9 \
	bus_space.9 bus_space_read_raw_region_4.9 bus_space.9 bus_space_read_raw_region_8.9 \
	bus_space.9 bus_space_set_multi_1.9 bus_space.9 bus_space_set_multi_2.9 \
	bus_space.9 bus_space_set_multi_4.9 bus_space.9 bus_space_set_multi_8.9 \
	bus_space.9 bus_space_set_region_1.9 bus_space.9 bus_space_set_region_2.9 \
	bus_space.9 bus_space_set_region_4.9 bus_space.9 bus_space_set_region_8.9 \
	bus_space.9 bus_space_subregion.9 bus_space.9 bus_space_unmap.9 \
	bus_space.9 bus_space_write_1.9 bus_space.9 bus_space_write_2.9 \
	bus_space.9 bus_space_write_4.9 bus_space.9 bus_space_write_8.9 \
	bus_space.9 bus_space_write_multi_1.9 bus_space.9 bus_space_write_multi_2.9 \
	bus_space.9 bus_space_write_multi_4.9 bus_space.9 bus_space_write_multi_8.9 \
	bus_space.9 bus_space_write_raw_multi_2.9 \
	bus_space.9 bus_space_write_raw_multi_4.9 \
	bus_space.9 bus_space_write_raw_multi_8.9 \
	bus_space.9 bus_space_write_region_1.9 \
	bus_space.9 bus_space_write_region_2.9 \
	bus_space.9 bus_space_write_region_4.9 \
	bus_space.9 bus_space_write_region_8.9 \
	bus_space.9 bus_space_write_raw_region_2.9 \
	bus_space.9 bus_space_write_raw_region_4.9 \
	bus_space.9 bus_space_write_raw_region_8.9
MLINKS+=crypto.9 crypto_register.9 \
	crypto.9 crypto_kregister.9 \
	crypto.9 crypto_unregister.9 \
	crypto.9 crypto_done.9 \
	crypto.9 crypto_kdone.9 \
	crypto.9 crypto_newsession.9 \
	crypto.9 crypto_freesession.9 \
	crypto.9 crypto_dispatch.9 \
	crypto.9 crypto_kdispatch.9 \
	crypto.9 crypto_getreq.9 \
	crypto.9 crypto_freereq.9
MLINKS+=copy.9 copyin.9 copy.9 copyout.9 copy.9 copystr.9 \
	copy.9 copyinstr.9 copy.9 copyoutstr.9
MLINKS+=ctxsw.9 cpu_switch.9 ctxsw.9 mi_switch.9
MLINKS+=disk.9 disk_init.9 disk.9 disk_attach.9 disk.9 disk_detatch.9 \
	disk.9 disk_busy.9 disk.9 disk_unbusy.9 disk.9 disk_find.9 \
	disk.9 disk_resetstat.9
MLINKS+=disklabel.9 readdisklabel.9 disklabel.9 writedisklabel.9 \
	disklabel.9 setdisklabel.9 disklabel.9 bounds_check_with_label.9
MLINKS+=extent.9 extent_create.9 extent.9 extent_destroy.9 \
	extent.9 extent_alloc.9 extent.9 extent_alloc_subregion.9 \
	extent.9 extent_alloc_region.9 extent.9 extent_free.9 \
	extent.9 extent_free.9 extent.9 extent_print.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuibyte.9 fetch.9 fusword.9 \
	fetch.9 fuswintr.9 fetch.9 fuword.9 fetch.9 fuiword.9
MLINKS+=file.9 falloc.9 file.9 fdrelease.9 file.9 FREF.9 file.9 FRELE.9 \
	file.9 fd_getfile.9 file.9 getvnode.9 file.9 getsock.9
MLINKS+=hook_establish.9 hook_disestablish.9
MLINKS+=hz.9 tick.9 hz.9 tickadj.9 hz.9 stathz.9 hz.9 profhz.9
MLINKS+=kthread.9 kthread_create.9 kthread.9 kthread_exit.9 \
	kthread.9 kthread_create_deferred.9
MLINKS+=lock.9 lockinit.9 lock.9 lockmgr.9 lock.9 lockstatus.9 \
	lock.9 lockmgr_printinfo.9 \
	lock.9 simple_lock_init.9 lock.9 simple_lock.9 \
	lock.9 simple_lock_try.9 lock.9 simple_unlock.9
MLINKS+=log.9 addlog.9
MLINKS+=md5.9 MD5Init.9 md5.9 MD5Transform.9
MLINKS+=mbuf.9 m_copym2.9 mbuf.9 m_copym.9 mbuf.9 m_free.9 mbuf.9 MFREE.9 \
	mbuf.9 m_get.9 mbuf.9 MGET.9 mbuf.9 m_getclr.9 mbuf.9 m_gethdr.9 \
	mbuf.9 MGETHDR.9 mbuf.9 m_prepend.9 mbuf.9 M_PREPEND.9 \
	mbuf.9 m_pulldown.9 mbuf.9 m_pullup.9 mbuf.9 m_pullup2.9 \
	mbuf.9 m_retry.9 mbuf.9 m_retryhdr.9 mbuf.9 m_split.9 \
	mbuf.9 m_inject.9 mbuf.9 m_getptr.9 mbuf.9 m_adj.9 mbuf.9 m_copyback.9 \
	mbuf.9 m_freem.9 mbuf.9 m_reclaim.9 mbuf.9 m_copydata.9 \
	mbuf.9 m_cat.9 mbuf.9 m_devget.9 mbuf.9 m_zero.9 mbuf.9 m_apply.9 \
	mbuf.9 MEXTALLOC.9 mbuf.9 MCLGET.9 mbuf.9 MEXTADD.9 mbuf.9 M_ALIGN.9 \
	mbuf.9 MH_ALIGN.9 mbuf.9 M_READONLY.9 mbuf.9 M_LEADINGSPACE.9 \
	mbuf.9 M_TRAILINGSPACE.9 mbuf.9 MCHTYPE.9
MLINKS+=random.9 add_true_randomness.9 \
	random.9 add_timer_randomness.9 \
	random.9 add_mouse_randomness.9 \
	random.9 add_tty_randomness.9 \
	random.9 add_net_randomness.9 \
	random.9 add_disk_randomness.9 \
	random.9 add_audio_randomness.9 \
	random.9 get_random_bytes.9 \
	random.9 arc4random.9
MLINKS+=pfind.9 pgfind.9
MLINKS+=pool.9 pool_init.9 pool.9 pool_destroy.9 pool.9 pool_get.9 \
	pool.9 pool_put.9 pool.9 pool_prime.9 pool.9 pool_sethiwat.9 \
	pool.9 pool_setlowat.9
MLINKS+=powerhook_establish.9 powerhook_disestablish.9
MLINKS+=printf.9 sprintf.9 printf.9 vprintf.9 printf.9 uprintf.9 \
	printf.9 ttyprintf.9 printf.9 db_printf.9
MLINKS+=psignal.9 pgsignal.9 psignal.9 gsignal.9
MLINKS+=shutdownhook_establish.9 shutdownhook_disestablish.9
MLINKS+=sleep.9 tsleep.9 sleep.9 wakeup.9
MLINKS+=spl.9 spl0.9 spl.9 splassert.9 spl.9 splbio.9 spl.9 splclock.9 \
	spl.9 splhigh.9 spl.9 splimp.9 spl.9 splnet.9 spl.9 splsched.9 \
	spl.9 splserial.9 spl.9 splsoftclock.9 spl.9 splsoftnet.9 \
	spl.9 splsoftserial.9 spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splx.9
MLINKS+=startuphook_establish.9 startuphook_disestablish.9
MLINKS+=store.9 subyte.9 store.9 suibyte.9 store.9 susword.9 \
	store.9 suswintr.9 store.9 suword.9 store.9 suiword.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 timeout_add.9 timeout.9 timeout_set.9 \
	timeout.9 timeout_pending.9 timeout.9 timeout_del.9 \
	timeout.9 timeout_initialized.9 timeout.9 untimeout.9 \
	timeout.9 timeout_triggered.9
MLINKS+=uiomove.9 uio.9
MLINKS+=vm_map_copy.9 vm_copy.9
MLINKS+=vm_map_inherit.9 vm_inherit.9
MLINKS+=vm_map_protect.9 vm_protect.9
MLINKS+=malloc.9 MALLOC.9 malloc.9 free.9 malloc.9 FREE.9

MLINKS+=kern.9 imax.9 kern.9 imin.9 kern.9 lmax.9 kern.9 lmin.9 \
	kern.9 max.9 kern.9 min.9 kern.9 ulmax.9 kern.9 ulmin.9 kern.9 abs.9 \
	kern.9 assert.9 kern.9 KASSERT.9 kern.9 KDASSERT.9 \
	kern.9 locc.9 kern.9 skpc.9 kern.9 scanc.9 kern.9 bcmp.9 \
	kern.9 memchr.9 kern.9 memcmp.9 kern.9 ffs.9 \
	kern.9 strlen.9 kern.9 strcat.9 kern.9 strcpy.9 \
	kern.9 strncpy.9 kern.9 strlcpy.9 kern.9 strlcat.9 \
	kern.9 strcmp.9 kern.9 strncmp.9 kern.9 strncasecmp.9 \
	kern.9 random.9 kern.9 srandom.9 kern.9 getsn.9

.include <bsd.prog.mk>
