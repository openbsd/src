#	$OpenBSD: Makefile,v 1.13 1999/09/22 03:16:46 csapuntz Exp $
#       $NetBSD: Makefile,v 1.4 1996/01/09 03:23:01 thorpej Exp $

#	Makefile for section 9 (kernel function and variable) manual pages.

MAN=	access.9 arp.9 audio.9 boot.9 bus_space.9 bus_dma.9 copy.9 ctxsw.9 \
	disk.9 disklabel.9 doshutdownhooks.9 ethersubr.9 fetch.9 fork1.9 \
	extent.9 hz.9 hzto.9 intro.9 inittodr.9 log.9 kthread.9 \
	malloc.9 mbuf.9 md5.9 microtime.9 panic.9 pfind.9 pool.9 \
	printf.9 psignal.9 resettodr.9 random.9 \
	shutdownhook_establish.9 sleep.9 spl.9 \
	store.9 style.9 time.9 timeout.9 uiomove.9 uvm.9 vslock.9 wdc.9 \
	vm_allocate.9 vm_map_copy.9 vm_deallocate.9 \
	vm_map_inherit.9 vm_map_protect.9

MLINKS+=access.9 kernacc.9 access.9 useracc.9
MLINKS+=arp.9 arp_ifinit.9 arp.9 arpresolve.9 arp.9 arpintr.9
MLINKS+=bus_space.9 bus_space_alloc.9 bus_space.9 bus_space_barrier.9 \
	bus_space.9 bus_space_copy_region_1.9 \
	bus_space.9 bus_space_copy_region_2.9 \
	bus_space.9 bus_space_copy_region_4.9 \
	bus_space.9 bus_space_copy_region_8.9 \
	bus_space.9 bus_space_free.9 bus_space.9 bus_space_map.9 \
	bus_space.9 bus_space_read_1.9 bus_space.9 bus_space_read_2.9 \
	bus_space.9 bus_space_read_4.9 bus_space.9 bus_space_read_8.9 \
	bus_space.9 bus_space_read_multi_1.9 \
	bus_space.9 bus_space_read_multi_2.9 \
	bus_space.9 bus_space_read_multi_4.9 \
	bus_space.9 bus_space_read_multi_8.9 \
	bus_space.9 bus_space_read_region_1.9 \
	bus_space.9 bus_space_read_region_2.9 \
	bus_space.9 bus_space_read_region_4.9 \
	bus_space.9 bus_space_read_region_8.9 \
	bus_space.9 bus_space_set_region_1.9 \
	bus_space.9 bus_space_set_region_2.9 \
	bus_space.9 bus_space_set_region_4.9 \
	bus_space.9 bus_space_set_region_8.9 \
	bus_space.9 bus_space_subregion.9 bus_space.9 bus_space_unmap.9 \
	bus_space.9 bus_space_write_1.9 bus_space.9 bus_space_write_2.9 \
	bus_space.9 bus_space_write_4.9 bus_space.9 bus_space_write_8.9 \
	bus_space.9 bus_space_write_multi_1.9 \
	bus_space.9 bus_space_write_multi_2.9 \
	bus_space.9 bus_space_write_multi_4.9 \
	bus_space.9 bus_space_write_multi_8.9 \
	bus_space.9 bus_space_write_region_1.9 \
	bus_space.9 bus_space_write_region_2.9 \
	bus_space.9 bus_space_write_region_4.9 \
	bus_space.9 bus_space_write_region_8.9
MLINKS+=bus_dma.9 bus_dmamap_create.9 bus_dma.9 bus_dmamap_destroy.9 \
	bus_dma.9 bus_dmamap_load.9 bus_dma.9 bus_dmamap_load_mbuf.9 \
	bus_dma.9 bus_dmamap_load_uio.9 bus_dma.9 bus_dmamap_load_raw.9 \
	bus_dma.9 bus_dmamap_unload.9 bus_dma.9 bus_dmamap_sync.9 \
	bus_dma.9 bus_dmamem_alloc.9 bus_dma.9 bus_dmamem_free.9 \
	bus_dma.9 bus_dmamem_map.9 bus_dma.9 bus_dmamem_unmap.9 \
	bus_dma.9 bus_dmamem_mmap.9

MLINKS+=copy.9 copyin.9 copy.9 copyout.9 copy.9 copystr.9 \
	copy.9 copyinstr.9 copy.9 copyoutstr.9
MLINKS+=ctxsw.9 cpu_switch.9 ctxsw.9 mi_switch.9
MLINKS+=disk.9 disk_init.9 disk.9 disk_attach.9 disk.9 disk_detach.9 \
	disk.9 disk_busy.9 disk.9 disk_unbusy.9 disk.9 disk_find.9 \
	disk.9 disk_resetstat.9
MLINKS+=disklabel.9 readdisklabel.9 disklabel.9 writedisklabel.9 \
	disklabel.9 setdisklabel.9 disklabel.9 bounds_check_with_label.9
MLINKS+=ethersubr.9 ether_ifattach.9 ethersubr.9 ether_addmulti.9 \
	ethersubr.9 ether_delmulti.9 ethersubr.9 ETHER_IS_MULTICAST.9 \
	ethersubr.9 ETHER_FIRST_MULTI.9 ethersubr.9 ETHER_NEXT_MULTI.9 \
	ethersubr.9 fddi_ifattach.9 \
	ethersubr.9 fddi_addmulti.9 ethersubr.9 fddi_delmulti.9
MLINKS+=extent.9 extent_create.9 extent.9 extent_destroy.9 \
	extent.9 extent_alloc.9 extent.9 extent_alloc_subregion.9 \
	extent.9 extent_alloc_region.9 extent.9 extent_free.9 \
	extent.9 extent_free.9
MLINKS+=fetch.9 fubyte.9 fetch.9 fuibyte.9 fetch.9 fusword.9 \
	fetch.9 fuswintr.9 fetch.9 fuword.9 fetch.9 fuiword.9
MLINKS+=kthread.9 kthread_create.9 kthread.9 kthread_exit.9 \
	kthread.9 kthread_create_deferred.9
MLINKS+=log.9 addlog.9
MLINKS+=malloc.9 free.9
MLINKS+=mbuf.9 m_get.9 mbuf.9 m_getclr.9 mbuf.9 m_gethdr.9 mbuf.9 m_devget.9 \
	mbuf.9 m_copym.9 mbuf.9 m_copypacket.9 mbuf.9 m_copydata.9 \
	mbuf.9 m_copyback.9 mbuf.9 m_cat.9 mbuf.9 m_prepend.9 \
	mbuf.9 m_pullup.9 mbuf.9 m_split.9 mbuf.9 m_adj.9 mbuf.9 m_free.9 \
	mbuf.9 m_freem.9 mbuf.9 mtod.9 mbuf.9 mtocl.9 mbuf.9 cltom.9 \
	mbuf.9 MGET.9 mbuf.9 MGETHDR.9 mbuf.9 MEXTMALLOC.9 \
	mbuf.9 MEXTADD.9 mbuf.9 MCLGET.9 mbuf.9 M_COPY_PKTHDR.9 \
	mbuf.9 M_ALIGN.9 mbuf.9 MH_ALIGN.9 mbuf.9 M_LEADINGSPACE.9 \
	mbuf.9 M_TRAILINGSPACE.9 mbuf.9 M_PREPEND.9 mbuf.9 MCHTYPE.9 \
	mbuf.9 MEXTREMOVE.9 mbuf.9 MFREE.9
MLINKS+=md5.9 MD5Init.9 md5.9 MD5Transform.9
MLINKS+=pfind.9 pgfind.9
MLINKS+=printf.9 sprintf.9 printf.9 vprintf.9 printf.9 uprintf.9 \
	printf.9 ttyprintf.9 printf.9 db_printf.9
MLINKS+=psignal.9 pgsignal.9 psignal.9 gsignal.9
MLINKS+=pool.9 pool_create.9 pool.9 pool_destroy.9 pool.9 pool_get.9 \
	pool.9 pool_put.9 pool.9 pool_prime.9 pool.9 pool_sethiwat.9 \
	pool.9 pool_setlowat.9
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
MLINKS+=uvm.9 uvm_init.9 uvm.9 uvm_init_limits.9 uvm.9 uvm_setpagesize.9 \
	uvm.9 uvm_swap_init.9 uvm.9 uvm_map.9 uvm.9 uvm_map_pageable.9 \
	uvm.9 uvm_map_checkprot.9 uvm.9 uvm_map_protect.9 \
	uvm.9 uvm_deallocate.9 uvm.9 uvmspace_alloc.9 uvm.9 uvmspace_exec.9 \
	uvm.9 uvmspace_fork.9 uvm.9 uvmspace_free.9 uvm.9 uvmspace_share.9 \
	uvm.9 uvmspace_unshare.9 uvm.9 uvm_fault.9 uvm.9 uvn_attach.9 \
	uvm.9 uvm_vnp_setsize.9 uvm.9 uvm_vnp_sync.9 uvm.9 uvm_vnp_terminate.9 \
	uvm.9 uvm_io.9 uvm.9 uvm_km_alloc.9 uvm.9 uvm_km_zalloc.9 \
	uvm.9 uvm_km_alloc1.9 uvm.9 uvm_km_kmemalloc.9 uvm.9 uvm_km_valloc.9 \
	uvm.9 uvm_km_valloc_wait.9 uvm.9 uvm_km_suballoc.9 uvm.9 uvm_km_free.9 \
	uvm.9 uvm_km_free_wakeup.9 uvm.9 uvm_pagealloc.9 \
	uvm.9 uvm_pagerealloc.9 uvm.9 uvm_pagefree.9 uvm.9 uvm_pglistalloc.9 \
	uvm.9 uvm_pglistfree.9 uvm.9 uvm_page_physload.9 uvm.9 uvm_pageout.9 \
	uvm.9 uvm_scheduler.9 uvm.9 uvm_swapin.9 uvm.9 uao_create.9 \
	uvm.9 uao_detach.9 uvm.9 uao_reference.9 uvm.9 uvm_chgkprot.9 \
	uvm.9 uvm_kernacc.9 uvm.9 uvm_useracc.9 uvm.9 uvm_vslock.9 \
	uvm.9 uvm_vsunlock.9 uvm.9 uvm_meter.9 uvm.9 uvm_sysctl.9 \
	uvm.9 uvm_fork.9 uvm.9 uvm_grow.9 uvm.9 uvm_coredump.9
MLINKS+=vslock.9 vsunlock.9

SUBDIR= man9.i386

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
