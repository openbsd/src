#	$OpenBSD: Makefile,v 1.232 2015/05/05 16:38:20 mikeb Exp $
#	$NetBSD: Makefile,v 1.4 1996/01/09 03:23:01 thorpej Exp $

#	Makefile for section 9 (kernel function and variable) manual pages.

MAN=	aml_evalnode.9 atomic_add_int.9 atomic_cas_uint.9 \
	atomic_dec_int.9 atomic_inc_int.9 atomic_setbits_int.9 \
	atomic_sub_int.9 atomic_swap_uint.9 \
	audio.9 autoconf.9 \
	bemtoh32.9 bio_register.9 boot.9 buffercache.9 bufq_init.9 bus_dma.9 \
	bus_space.9 \
	copy.9 config_attach.9 crypto.9 delay.9 \
	disk.9 disklabel.9 dma_alloc.9 dohooks.9 dopowerhooks.9 \
	domountroothooks.9 dostartuphooks.9 \
	evcount.9 extent.9 fb_setup.9 file.9 fork1.9 \
	getdevvp.9 getnewvnode.9 hash.9 hashinit.9 \
	hardclock.9 hook_establish.9 hz.9 hzto.9 idgen32.9 \
	ieee80211.9 ieee80211_crypto.9 ieee80211_input.9 ieee80211_ioctl.9 \
	ieee80211_node.9 ieee80211_output.9 ieee80211_proto.9 \
	ieee80211_radiotap.9 \
	if_rxr_init.9 iic.9 intro.9 inittodr.9 \
	kern.9 km_alloc.9 knote.9 kthread.9 ktrace.9 \
	loadfirmware.9 lock.9 log.9 \
	malloc.9 membar_sync.9 mbuf.9 mbuf_tags.9 md5.9 mi_switch.9 \
	microtime.9 mountroothook_establish.9 mutex.9 namei.9 \
	panic.9 pci_conf_read.9 pci_intr_map.9 pfind.9 physio.9 pmap.9 \
	pool.9 powerhook_establish.9 ppsratecheck.9 printf.9 psignal.9 \
	radio.9 arc4random.9 rasops.9 ratecheck.9 resettodr.9 rssadapt.9 \
	route.9 rt_ifa_add.9 rt_timer_add.9 rtalloc.9 rtable_add.9 \
	rtlabel_id2name.9 rtrequest1.9 rwlock.9 SipHash24.9 sensor_attach.9 \
	tsleep.9 spl.9 startuphook_establish.9 \
	socreate.9 sosplice.9 style.9 syscall.9 systrace.9 sysctl_int.9 \
	task_add.9 tc_init.9 time.9 timeout.9 tvtohz.9 uiomove.9 uvm.9 \
	usbd_close_pipe.9 usbd_open_pipe.9 usbd_transfer.9 \
	vfs.9 vfs_busy.9 \
	vfs_cache.9 vaccess.9 vclean.9 vcount.9 vdevgone.9 vfinddev.9 vflush.9 \
	vflushbuf.9 vget.9 vgone.9 vhold.9 vinvalbuf.9 vnode.9 vnsubr.9 \
	VOP_GETATTR.9 VOP_LOOKUP.9 vput.9 vrecycle.9 vref.9 vrele.9 \
	vwaitforio.9 vwakeup.9 wdog_register.9

MLINKS+=aml_evalnode.9 aml_evalname.9 aml_evalnode.9 aml_find_node.9 \
	aml_evalnode.9 aml_freevalue.9 aml_evalnode.9 aml_val2int.9
MLINKS+=atomic_setbits_int.9 atomic_clearbits_int.9
MLINKS+=atomic_add_int.9 atomic_add_long.9 \
	atomic_add_int.9 atomic_add_int_nv.9 \
	atomic_add_int.9 atomic_add_long_nv.9
MLINKS+=atomic_cas_uint.9 atomic_cas_ulong.9 \
	atomic_cas_uint.9 atomic_cas_ptr.9
MLINKS+=atomic_dec_int.9 atomic_dec_long.9 \
	atomic_dec_int.9 atomic_dec_int_nv.9 \
	atomic_dec_int.9 atomic_dec_long_nv.9
MLINKS+=atomic_inc_int.9 atomic_inc_long.9 \
	atomic_inc_int.9 atomic_inc_int_nv.9 \
	atomic_inc_int.9 atomic_inc_long_nv.9
MLINKS+=atomic_sub_int.9 atomic_sub_long.9 \
	atomic_sub_int.9 atomic_sub_int_nv.9 \
	atomic_sub_int.9 atomic_sub_long_nv.9
MLINKS+=atomic_swap_uint.9 atomic_swap_ulong.9 \
	atomic_swap_uint.9 atomic_swap_ptr.9
MLINKS+=autoconf.9 config_search.9 \
	autoconf.9 config_rootsearch.9 autoconf.9 config_found_sm.9 \
	autoconf.9 config_found.9 autoconf.9 config_rootfound.9 \
	autoconf.9 config_defer.9
MLINKS+=bemtoh32.9 bemtoh16.9 bemtoh32.9 bemtoh64.9 \
	bemtoh32.9 htobem16.9 bemtoh32.9 htobem32.9 bemtoh32.9 htobem64.9 \
	bemtoh32.9 lemtoh16.9 bemtoh32.9 lemtoh32.9 bemtoh32.9 lemtoh64.9 \
	bemtoh32.9 htolem16.9 bemtoh32.9 htolem32.9 bemtoh32.9 htolem64.9
MLINKS+=bio_register.9 bio_unregister.9
MLINKS+=buffercache.9 bread.9 buffercache.9 breadn.9 \
	buffercache.9 bwrite.9 buffercache.9 bawrite.9 \
	buffercache.9 bdwrite.9 buffercache.9 getblk.9 \
	buffercache.9 geteblk.9 buffercache.9 incore.9 \
	buffercache.9 brelse.9 \
	buffercache.9 biodone.9 buffercache.9 biowait.9
MLINKS+=bufq_init.9 bufq_switch.9 bufq_init.9 bufq_destroy.9 \
	bufq_init.9 bufq_queue.9 bufq_init.9 bufq_dequeue.9 \
	bufq_init.9 bufq_requeue.9 bufq_init.9 bufq_peek.9 \
	bufq_init.9 bufq_drain.9
MLINKS+=bus_dma.9 bus_dmamap_create.9 bus_dma.9 bus_dmamap_destroy.9 \
	bus_dma.9 bus_dmamap_load.9 bus_dma.9 bus_dmamap_load_mbuf.9 \
	bus_dma.9 bus_dmamap_load_uio.9 bus_dma.9 bus_dmamap_load_raw.9 \
	bus_dma.9 bus_dmamap_unload.9 bus_dma.9 bus_dmamap_sync.9 \
	bus_dma.9 bus_dmamem_alloc.9 bus_dma.9 bus_dmamem_alloc_range.9 \
	bus_dma.9 bus_dmamem_free.9 \
	bus_dma.9 bus_dmamem_map.9 bus_dma.9 bus_dmamem_unmap.9 \
	bus_dma.9 bus_dmamem_mmap.9
MLINKS+=bus_space.9 bus_space_alloc.9 bus_space.9 bus_space_barrier.9 \
	bus_space.9 bus_space_vaddr.9 \
	bus_space.9 bus_space_copy_1.9 bus_space.9 bus_space_copy_2.9 \
	bus_space.9 bus_space_copy_4.9 bus_space.9 bus_space_copy_8.9 \
	bus_space.9 bus_space_free.9 bus_space.9 bus_space_map.9 \
	bus_space.9 bus_space_mmap.9 \
	bus_space.9 bus_space_read_1.9 bus_space.9 bus_space_read_2.9 \
	bus_space.9 bus_space_read_4.9 bus_space.9 bus_space_read_8.9 \
	bus_space.9 bus_space_read_raw_2.9 \
	bus_space.9 bus_space_read_raw_4.9 \
	bus_space.9 bus_space_read_raw_8.9 \
	bus_space.9 bus_space_read_multi_1.9 \
	bus_space.9 bus_space_read_multi_2.9 \
	bus_space.9 bus_space_read_multi_4.9 \
	bus_space.9 bus_space_read_multi_8.9 \
	bus_space.9 bus_space_read_raw_multi_2.9 \
	bus_space.9 bus_space_read_raw_multi_4.9 \
	bus_space.9 bus_space_read_raw_multi_8.9 \
	bus_space.9 bus_space_read_region_1.9 \
	bus_space.9 bus_space_read_region_2.9 \
	bus_space.9 bus_space_read_region_4.9 \
	bus_space.9 bus_space_read_region_8.9 \
	bus_space.9 bus_space_read_raw_region_2.9 \
	bus_space.9 bus_space_read_raw_region_4.9 \
	bus_space.9 bus_space_read_raw_region_8.9 \
	bus_space.9 bus_space_set_multi_1.9 \
	bus_space.9 bus_space_set_multi_2.9 \
	bus_space.9 bus_space_set_multi_4.9 \
	bus_space.9 bus_space_set_multi_8.9 \
	bus_space.9 bus_space_set_region_1.9 \
	bus_space.9 bus_space_set_region_2.9 \
	bus_space.9 bus_space_set_region_4.9 \
	bus_space.9 bus_space_set_region_8.9 \
	bus_space.9 bus_space_subregion.9 bus_space.9 bus_space_unmap.9 \
	bus_space.9 bus_space_write_1.9 bus_space.9 bus_space_write_2.9 \
	bus_space.9 bus_space_write_4.9 bus_space.9 bus_space_write_8.9 \
	bus_space.9 bus_space_write_raw_2.9 \
	bus_space.9 bus_space_write_raw_4.9 \
	bus_space.9 bus_space_write_raw_8.9 \
	bus_space.9 bus_space_write_multi_1.9 \
	bus_space.9 bus_space_write_multi_2.9 \
	bus_space.9 bus_space_write_multi_4.9 \
	bus_space.9 bus_space_write_multi_8.9 \
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
MLINKS+=config_attach.9 config_detach.9 config_attach.9 config_detach_children.9
MLINKS+=copy.9 copyin.9 copy.9 copyout.9 copy.9 copystr.9 \
	copy.9 copyinstr.9 copy.9 copyoutstr.9 \
	copy.9 kcopy.9
MLINKS+=crypto.9 crypto_get_driverid.9 \
	crypto.9 crypto_register.9 \
	crypto.9 crypto_unregister.9 \
	crypto.9 crypto_done.9 \
	crypto.9 crypto_newsession.9 \
	crypto.9 crypto_freesession.9 \
	crypto.9 crypto_dispatch.9 \
	crypto.9 crypto_getreq.9 \
	crypto.9 crypto_freereq.9
MLINKS+=mi_switch.9 cpu_switchto.9
MLINKS+=disk.9 disk_init.9 disk.9 disk_attach.9 disk.9 disk_detach.9 \
	disk.9 disk_busy.9 disk.9 disk_unbusy.9
MLINKS+=disklabel.9 readdisklabel.9 disklabel.9 writedisklabel.9 \
	disklabel.9 setdisklabel.9 disklabel.9 bounds_check_with_label.9
MLINKS+=evcount.9 evcount_attach.9 evcount.9 evcount_detach.9
MLINKS+=extent.9 extent_create.9 extent.9 extent_destroy.9 \
	extent.9 extent_alloc.9 extent.9 extent_alloc_with_descr.9 \
	extent.9 extent_alloc_subregion.9 \
	extent.9 extent_alloc_subregion_with_descr.9 \
	extent.9 extent_alloc_region.9 extent.9 extent_free.9 \
	extent.9 extent_print.9
MLINKS+=fb_setup.9 fb_queue.9 fb_setup.9 fb_delete.9
MLINKS+=file.9 falloc.9 file.9 fdrelease.9 file.9 FREF.9 file.9 FRELE.9 \
	file.9 fd_getfile.9 file.9 getvnode.9 file.9 getsock.9
MLINKS+=getdevvp.9 bdevvp.9 getdevvp.9 cdevvp.9
MLINKS+=hook_establish.9 hook_disestablish.9
MLINKS+=hz.9 tick.9 hz.9 tickadj.9 hz.9 stathz.9 hz.9 profhz.9
MLINKS+=idgen32.9 idgen32_init.9
MLINKS+=ieee80211.9 ieee80211_ifattach.9 \
	ieee80211.9 ieee80211_ifdetach.9 \
	ieee80211.9 ieee80211_mhz2ieee.9 \
	ieee80211.9 ieee80211_chan2ieee.9 \
	ieee80211.9 ieee80211_ieee2mhz.9 \
	ieee80211.9 ieee80211_media_init.9 \
	ieee80211.9 ieee80211_media_change.9 \
	ieee80211.9 ieee80211_media_status.9 \
	ieee80211.9 ieee80211_watchdog.9 \
	ieee80211.9 ieee80211_setmode.9 \
	ieee80211.9 ieee80211_chan2mode.9 \
	ieee80211.9 ieee80211_rate2media.9 \
	ieee80211.9 ieee80211_media2rate.9 \
	ieee80211.9 ieee80211_rate2plcp.9 \
	ieee80211.9 ieee80211_plcp2rate.9
MLINKS+=ieee80211_crypto.9 ieee80211_crypto_attach.9 \
	ieee80211_crypto.9 ieee80211_crypto_detach.9 \
	ieee80211_crypto.9 ieee80211_wep_crypt.9
MLINKS+=ieee80211_input.9 ieee80211_decap.9 \
	ieee80211_input.9 ieee80211_recv_mgmt.9
MLINKS+=ieee80211_node.9 ieee80211_node_attach.9 \
	ieee80211_node.9 ieee80211_node_lateattach.9 \
	ieee80211_node.9 ieee80211_node_detach.9 \
	ieee80211_node.9 ieee80211_begin_scan.9 \
	ieee80211_node.9 ieee80211_next_scan.9 \
	ieee80211_node.9 ieee80211_create_ibss.9 \
	ieee80211_node.9 ieee80211_end_scan.9 \
	ieee80211_node.9 ieee80211_alloc_node.9 \
	ieee80211_node.9 ieee80211_dup_bss.9 \
	ieee80211_node.9 ieee80211_find_node.9 \
	ieee80211_node.9 ieee80211_release_node.9 \
	ieee80211_node.9 ieee80211_free_node.9 \
	ieee80211_node.9 ieee80211_free_allnodes.9 \
	ieee80211_node.9 ieee80211_iterate_nodes.9
MLINKS+=ieee80211_output.9 ieee80211_encap.9 \
	ieee80211_output.9 ieee80211_add_rates.9 \
	ieee80211_output.9 ieee80211_add_xrates.9 \
	ieee80211_output.9 ieee80211_compute_duration.9 \
	ieee80211_output.9 ieee80211_send_mgmt.9
MLINKS+=ieee80211_proto.9 ieee80211_proto_attach.9 \
	ieee80211_proto.9 ieee80211_proto_detach.9 \
	ieee80211_proto.9 ieee80211_print_essid.9 \
	ieee80211_proto.9 ieee80211_dump_pkt.9 \
	ieee80211_proto.9 ieee80211_fix_rate.9
MLINKS+=if_rxr_init.9 if_rxr_get.9 if_rxr_init.9 if_rxr_put.9 \
	if_rxr_init.9 if_rxr_inuse.9 if_rxr_init.9 if_rxr_ioctl.9 \
	if_rxr_init.9 if_rxr_info_ioctl.9
MLINKS+=iic.9 iic_acquire_bus.9 iic.9 iic_release_bus.9 iic.9 iic_exec.9 \
	iic.9 iic_smbus_write_byte.9 iic.9 iic_smbus_read_byte.9 \
	iic.9 iic_smbus_receive_byte.9
MLINKS+=kern.9 imax.9 kern.9 imin.9 kern.9 lmax.9 kern.9 lmin.9 \
	kern.9 max.9 kern.9 min.9 kern.9 ulmax.9 kern.9 ulmin.9 kern.9 abs.9 \
	kern.9 assert.9 kern.9 KASSERT.9 kern.9 KDASSERT.9 \
	kern.9 KASSERTMSG.9 kern.9 KDASSERTMSG.9 \
	kern.9 skpc.9 kern.9 scanc.9 kern.9 bcmp.9 \
	kern.9 memchr.9 kern.9 memcmp.9 kern.9 ffs.9 kern.9 fls.9 kern.9 flsl.9 \
	kern.9 strlen.9 kern.9 strncpy.9 kern.9 strlcpy.9 kern.9 strlcat.9 \
	kern.9 strcmp.9 kern.9 strncmp.9 kern.9 strncasecmp.9 \
	kern.9 strnlen.9 \
	kern.9 timingsafe_bcmp.9 kern.9 getsn.9
MLINKS+=km_alloc.9 km_free.9
MLINKS+=knote.9 KNOTE.9
MLINKS+=kthread.9 kthread_create.9 kthread.9 kthread_exit.9 \
	kthread.9 kthread_create_deferred.9
MLINKS+=ktrace.9 ktrcsw.9 ktrace.9 ktremul.9 ktrace.9 ktrgenio.9 \
	ktrace.9 ktrnamei.9 ktrace.9 ktrpsig.9 ktrace.9 ktrsyscall.9 \
	ktrace.9 ktrsysret.9 ktrace.9 KTRPOINT.9
MLINKS+=lock.9 lockinit.9 lock.9 lockmgr.9 lock.9 lockstatus.9
MLINKS+=log.9 addlog.9
MLINKS+=malloc.9 mallocarray.9 malloc.9 free.9
MLINKS+=membar_sync.9 membar_enter.9 membar_sync.9 membar_exit.9 \
	membar_sync.9 membar_producer.9 membar_sync.9 membar_consumer.9
MLINKS+=mbuf.9 m_copym2.9 mbuf.9 m_copym.9 mbuf.9 m_free.9 mbuf.9 MFREE.9 \
	mbuf.9 m_get.9 mbuf.9 MGET.9 mbuf.9 m_getclr.9 mbuf.9 m_gethdr.9 \
	mbuf.9 MGETHDR.9 mbuf.9 m_prepend.9 mbuf.9 M_PREPEND.9 \
	mbuf.9 m_pulldown.9 mbuf.9 m_pullup.9 \
	mbuf.9 m_split.9 \
	mbuf.9 m_inject.9 mbuf.9 m_getptr.9 mbuf.9 m_adj.9 mbuf.9 m_copyback.9 \
	mbuf.9 m_defrag.9 mbuf.9 m_freem.9 mbuf.9 m_reclaim.9 \
	mbuf.9 m_copydata.9 mbuf.9 m_cat.9 mbuf.9 m_devget.9 mbuf.9 m_apply.9 \
	mbuf.9 MCLGET.9 mbuf.9 MCLGETI.9  \
	mbuf.9 MEXTADD.9 mbuf.9 M_ALIGN.9 mbuf.9 MH_ALIGN.9 \
	mbuf.9 M_READONLY.9 mbuf.9 M_LEADINGSPACE.9 \
	mbuf.9 M_TRAILINGSPACE.9 mbuf.9 mtod.9 \
	mbuf.9 m_dup_pkthdr.9 \
	mbuf.9 ml_init.9 mbuf.9 ml_enqueue.9 mbuf.9 ml_dequeue.9 \
	mbuf.9 ml_dechain.9 mbuf.9 ml_filter.9 mbuf.9 ml_len.9 \
	mbuf.9 ml_empty.9 \
	mbuf.9 MBUF_LIST_INITIALIZER.9 mbuf.9 MBUF_LIST_FOREACH.9 \
	mbuf.9 mq_init.9 mbuf.9 mq_enqueue.9 mbuf.9 mq_dequeue.9 \
	mbuf.9 mq_enlist.9 mbuf.9 mq_delist.9 mbuf.9 mq_dechain.9 \
	mbuf.9 mq_filter.9 mbuf.9 mq_len.9 mbuf.9 mq_empty.9 \
	mbuf.9 mq_drops.9 mbuf.9 mq_set_maxlen.9 \
	mbuf.9 MBUF_QUEUE_INITIALIZER.9
MLINKS+=mbuf_tags.9 m_tag_get.9 mbuf_tags.9 m_tag_find.9 \
	mbuf_tags.9 m_tag_prepend.9 mbuf_tags.9 m_tag_delete.9 \
	mbuf_tags.9 m_tag_copy.9 mbuf_tags.9 m_tag_delete_chain.9 \
	mbuf_tags.9 m_tag_init.9 mbuf_tags.9 m_tag_copy_chain.9 \
	mbuf_tags.9 m_tag_first.9 mbuf_tags.9 m_tag_next.9
MLINKS+=md5.9 MD5Init.9 md5.9 MD5Transform.9
MLINKS+=microtime.9 getmicrotime.9 microtime.9 microuptime.9 \
	microtime.9 getmicrouptime.9 microtime.9 nanotime.9 \
	microtime.9 getnanotime.9 microtime.9 nanouptime.9 \
	microtime.9 getnanouptime.9 microtime.9 bintime.9 \
	microtime.9 binuptime.9
MLINKS+=mountroothook_establish.9 mountroothook_disestablish.9
MLINKS+=mutex.9 mtx_init.9 mutex.9 mtx_enter.9 mutex.9 mtx_leave.9 \
	mutex.9 MUTEX_ASSERT_LOCKED.9 mutex.9 MUTEX_ASSERT_UNLOCKED.9 \
	mutex.9 MUTEX_INITIALIZER.9
MLINKS+=mutex.9 mtx_enter_try.9
MLINKS+=namei.9 vfs_lookup.9 namei.9 vfs_relookup.9 namei.9 NDINIT.9
MLINKS+=namei.9 NDINITAT.9
MLINKS+=pci_conf_read.9 pci_conf_write.9 pci_conf_read.9 pci_make_tag.9 \
	pci_conf_read.9 pci_decompose_tag.9
MLINKS+=pci_intr_map.9 pci_intr_map_msi.9 pci_intr_map.9 pci_intr_string.9 \
	pci_intr_map.9 pci_intr_line.9 \
	pci_intr_map.9 pci_intr_establish.9 \
	pci_intr_map.9 pci_intr_disestablish.9
MLINKS+=pfind.9 pgfind.9
MLINKS+=pmap.9 pmap_init.9 pmap.9 pmap_enter.9 pmap.9 pmap_remove.9 \
	pmap.9 pmap_unwire.9 pmap.9 pmap_protect.9 pmap.9 pmap_page_protect.9 \
	pmap.9 pmap_is_modified.9 pmap.9 pmap_clear_modify.9 \
	pmap.9 pmap_is_referenced.9 pmap.9 pmap_clear_reference.9 \
	pmap.9 pmap_copy_page.9 pmap.9 pmap_zero_page.9 \
	pmap.9 pmap_create.9 pmap.9 pmap_reference.9 pmap.9 pmap_destroy.9 \
	pmap.9 pmap_steal_memory.9 pmap.9 pmap_growkernel.9 \
	pmap.9 pmap_update.9 pmap.9 pmap_collect.9 pmap.9 pmap_virtual_space.9 \
	pmap.9 pmap_copy.9 pmap.9 pmap_kenter_pa.9 pmap.9 pmap_kremove.9
MLINKS+=pool.9 pool_init.9 pool.9 pool_destroy.9 \
	pool.9 pool_get.9 pool.9 pool_put.9 pool.9 pool_prime.9 \
	pool.9 pool_sethiwat.9 pool.9 pool_setipl.9 pool.9 pool_setlowat.9 \
	pool.9 pool_sethardlimit.9
MLINKS+=dma_alloc.9 dma_free.9
MLINKS+=powerhook_establish.9 powerhook_disestablish.9
MLINKS+=printf.9 snprintf.9 printf.9 vsnprintf.9 printf.9 vprintf.9 \
	printf.9 uprintf.9 printf.9 ttyprintf.9 printf.9 db_printf.9 \
	printf.9 db_vprintf.9
MLINKS+=psignal.9 pgsignal.9 psignal.9 gsignal.9
MLINKS+=arc4random.9 add_true_randomness.9 \
	arc4random.9 add_timer_randomness.9 \
	arc4random.9 add_mouse_randomness.9 \
	arc4random.9 add_tty_randomness.9 \
	arc4random.9 add_net_randomness.9 \
	arc4random.9 add_disk_randomness.9 \
	arc4random.9 add_audio_randomness.9 \
	arc4random.9 arc4random_buf.9 \
	arc4random.9 arc4random_uniform.9
MLINKS+=rasops.9 rasops_init.9 rasops.9 rasops_reconfig.9
MLINKS+=rssadapt.9 ieee80211_rssadapt_choose.9 \
	rssadapt.9 ieee80211_rssadapt_input.9 \
	rssadapt.9 ieee80211_rssadapt_lower_rate.9 \
	rssadapt.9 ieee80211_rssadapt_raise_rate.9 \
	rssadapt.9 ieee80211_rssadapt_updatestats.9
MLINKS+=route.9 rt_lookup.9 \
	route.9 rt_setgate.9 route.9 rtredirect.9 route.9 rtdeletemsg.9
MLINKS+=rt_ifa_add.9 rt_ifa_del.9 rt_ifa_add.9 rt_ifa_addlocal.9 \
	rt_ifa_add.9 rt_ifa_dellocal.9
MLINKS+=rt_timer_add.9 rt_timer_queue_create.9 \
	rt_timer_add.9 rt_timer_queue_count.9 \
	rt_timer_add.9 rt_timer_queue_change.9 \
	rt_timer_add.9 rt_timer_queue_destroy.9 \
	rt_timer_add.9 rt_timer_remove_all.9
MLINKS+=rtalloc.9 rtalloc_mpath.9 rtalloc.9 rtfree.9
MLINKS+=rtable_add.9 rtable_exists.9 rtable_add.9 rtable_get.9 \
	rtable_add.9 rtable_l2.9 rtable_add.9 rtable_l2set.9
MLINKS+=rtlabel_id2name.9 rtlabel_name2id.9 \
	rtlabel_id2name.9 rtlabel_id2sa.9 rtlabel_id2name.9 rtlabel_unref.9
MLINKS+=rwlock.9 rw_init.9 rwlock.9 rw_enter.9 rwlock.9 rw_exit.9 \
	rwlock.9 rw_enter_read.9 rwlock.9 rw_enter_write.9 \
	rwlock.9 rw_exit_read.9 rwlock.9 rw_exit_write.9 \
	rwlock.9 rw_assert_wrlock.9 rwlock.9 rw_assert_rdlock.9 \
	rwlock.9 rw_assert_unlocked.9 rwlock.9 rw_status.9 \
	rwlock.9 RWLOCK_INITIALIZER.9 rwlock.9 rrw_init.9 \
	rwlock.9 rrw_enter.9 rwlock.9 rrw_exit.9 \
	rwlock.9 rrw_status.9
MLINKS+=SipHash24.9 SipHash24_Init.9 SipHash24.9 SipHash24_Update.9 \
	SipHash24.9 SipHash24_End.9 SipHash24.9 SipHash24_Final.9
MLINKS+=sensor_attach.9 sensordev_install.9 \
	sensor_attach.9 sensordev_deinstall.9 \
	sensor_attach.9 sensordev_get.9 \
	sensor_attach.9 sensor_detach.9 \
	sensor_attach.9 sensor_find.9 \
	sensor_attach.9 sensor_task_register.9 \
	sensor_attach.9 sensor_task_unregister.9
MLINKS+=socreate.9 sobind.9 socreate.9 soclose.9 socreate.9 soconnect.9 \
	socreate.9 sogetopt.9 socreate.9 soreceive.9 socreate.9 sosetopt.9 \
	socreate.9 sosend.9 socreate.9 soshutdown.9
MLINKS+=sosplice.9 somove.9
MLINKS+=spl.9 spl0.9 spl.9 splassert.9 spl.9 splbio.9 spl.9 splclock.9 \
	spl.9 splhigh.9 spl.9 spllowersoftclock.9 \
	spl.9 splnet.9 spl.9 splsched.9 spl.9 splserial.9 spl.9 splsoftclock.9 \
	spl.9 splsoftnet.9 spl.9 splsofttty.9 \
	spl.9 splstatclock.9 spl.9 spltty.9 spl.9 splvm.9 spl.9 splx.9
MLINKS+=startuphook_establish.9 startuphook_disestablish.9
MLINKS+=sysctl_int.9 sysctl_int_arr.9 sysctl_int.9 sysctl_quad.9 \
	sysctl_int.9 sysctl_string.9 sysctl_int.9 sysctl_tstring.9 \
	sysctl_int.9 sysctl_rdint.9 sysctl_int.9 sysctl_rdquad.9 \
	sysctl_int.9 sysctl_rdstring.9 sysctl_int.9 sysctl_rdstruct.9 \
	sysctl_int.9 sysctl_struct.9 sysctl_int.9 ctlname.9
MLINKS+=systrace.9 systrace_redirect.9 \
	systrace.9 systrace_fork.9 systrace.9 systrace_exit.9
MLINKS+=task_add.9 taskq_create.9 \
	task_add.9 taskq_destroy.9 \
	task_add.9 task_set.9 \
	task_add.9 task_del.9 \
	task_add.9 TASK_INITIALIZER.9
MLINKS+=time.9 boottime.9 time.9 mono_time.9 time.9 runtime.9
MLINKS+=timeout.9 timeout_add.9 timeout.9 timeout_set.9 \
	timeout.9 timeout_pending.9 timeout.9 timeout_del.9 \
	timeout.9 timeout_initialized.9 \
	timeout.9 timeout_triggered.9 timeout.9 timeout_add_tv.9 \
	timeout.9 timeout_add_ts.9 timeout.9 timeout_add_bt.9 \
	timeout.9 timeout_add_sec.9 \
	timeout.9 timeout_add_msec.9 \
	timeout.9 timeout_add_usec.9 \
	timeout.9 timeout_add_nsec.9 \
	timeout.9 TIMEOUT_INITIALIZER.9
MLINKS+=tsleep.9 wakeup.9 tsleep.9 wakeup_n.9 tsleep.9 wakeup_one.9 \
	tsleep.9 msleep.9 
MLINKS+=tvtohz.9 tstohz.9
MLINKS+=uiomove.9 uio.9 uiomove.9 uiomovei.9
MLINKS+=usbd_close_pipe.9 usbd_abort_pipe.9
MLINKS+=usbd_open_pipe.9 usbd_open_pipe_intr.9
MLINKS+=usbd_transfer.9 usbd_setup_xfer.9
MLINKS+=uvm.9 uvm_init.9 uvm.9 uvm_init_limits.9 uvm.9 uvm_setpagesize.9 \
	uvm.9 uvm_swap_init.9 uvm.9 uvm_map.9 uvm.9 uvm_map_pageable.9 \
	uvm.9 uvm_map_pageable_all.9 uvm.9 uvm_map_checkprot.9 \
	uvm.9 uvm_map_protect.9 uvm.9 uvm_deallocate.9 uvm.9 uvmspace_alloc.9 \
	uvm.9 uvmspace_exec.9 uvm.9 uvmspace_fork.9 uvm.9 uvmspace_free.9 \
	uvm.9 uvmspace_share.9 uvm.9 UVM_MAPFLAG.9 uvm.9 uvm_fault.9 \
	uvm.9 uvn_attach.9 uvm.9 uvm_vnp_setsize.9 uvm.9 uvm_vnp_sync.9 \
	uvm.9 uvm_vnp_terminate.9 uvm.9 uvm_vnp_uncache.9 uvm.9 uvm_io.9 \
	uvm.9 uvm_km_alloc.9 uvm.9 uvm_km_zalloc.9 uvm.9 uvm_km_alloc1.9 \
	uvm.9 uvm_km_kmemalloc.9 uvm.9 uvm_km_valloc.9 \
	uvm.9 uvm_km_valloc_wait.9 uvm.9 uvm_km_suballoc.9 \
	uvm.9 uvm_km_free.9 uvm.9 uvm_km_free_wakeup.9 uvm.9 uvm_pagealloc.9 \
	uvm.9 uvm_pagerealloc.9 uvm.9 uvm_pagefree.9 uvm.9 uvm_pglistalloc.9 \
	uvm.9 uvm_pglistfree.9 uvm.9 uvm_page_physload.9 \
	uvm.9 uvm_pageout.9 \
	uvm.9 uao_create.9 uvm.9 uao_detach.9 uvm.9 uao_reference.9 \
	uvm.9 uvm_chgkprot.9 uvm.9 uvm_kernacc.9 \
	uvm.9 uvm_vslock.9 uvm.9 uvm_vsunlock.9 uvm.9 uvm_meter.9 \
	uvm.9 uvm_sysctl.9 uvm.9 uvm_grow.9 \
	uvm.9 uvm_coredump.9
MLINKS+=vfs_busy.9 vfs_isbusy.9 vfs_busy.9 vfs_unbusy.9
MLINKS+=vfs_cache.9 cache_enter.9 vfs_cache.9 cache_lookup.9 \
	vfs_cache.9 cache_purge.9 vfs_cache.9 cache_purgevfs.9 \
	vfs_cache.9 cache_revlookup.9
MLINKS+=vgone.9 vgonel.9
MLINKS+=vnsubr.9 vn_close.9 vnsubr.9 vn_default_error.9 \
	vnsubr.9 vn_isunder.9 vnsubr.9 vn_lock.9 vnsubr.9 vn_open.9 \
	vnsubr.9 vn_rdwr.9 vnsubr.9 vn_stat.9 vnsubr.9 vn_writechk.9 \
	vnsubr.9 vn_marktext.9
MLINKS+=VOP_GETATTR.9 VOP_SETATTR.9
MLINKS+=VOP_LOOKUP.9 VOP_ABORTOP.9 VOP_LOOKUP.9 VOP_ACCESS.9 \
	VOP_LOOKUP.9 VOP_ADVLOCK.9 VOP_LOOKUP.9 VOP_BMAP.9 \
	VOP_LOOKUP.9 VOP_BWRITE.9 VOP_LOOKUP.9 VOP_CLOSE.9 \
	VOP_LOOKUP.9 VOP_CREATE.9 VOP_LOOKUP.9 VOP_FSYNC.9 \
	VOP_LOOKUP.9 VOP_INACTIVE.9 VOP_LOOKUP.9 VOP_IOCTL.9 \
	VOP_LOOKUP.9 VOP_ISLOCKED.9 VOP_LOOKUP.9 VOP_KQFILTER.9 \
	VOP_LOOKUP.9 VOP_LINK.9 \
	VOP_LOOKUP.9 VOP_LOCK.9 VOP_LOOKUP.9 VOP_MKDIR.9 \
	VOP_LOOKUP.9 VOP_MKNOD.9 VOP_LOOKUP.9 VOP_OPEN.9 \
	VOP_LOOKUP.9 VOP_PATHCONF.9 VOP_LOOKUP.9 VOP_POLL.9 \
	VOP_LOOKUP.9 VOP_PRINT.9 VOP_LOOKUP.9 VOP_READ.9 \
	VOP_LOOKUP.9 VOP_READDIR.9 VOP_LOOKUP.9 VOP_READLINK.9 \
	VOP_LOOKUP.9 VOP_REALLOCBLKS.9 VOP_LOOKUP.9 VOP_RECLAIM.9 \
	VOP_LOOKUP.9 VOP_REMOVE.9 VOP_LOOKUP.9 VOP_RENAME.9 \
	VOP_LOOKUP.9 VOP_REVOKE.9 VOP_LOOKUP.9 VOP_RMDIR.9 \
	VOP_LOOKUP.9 VOP_STRATEGY.9 VOP_LOOKUP.9 VOP_SYMLINK.9 \
	VOP_LOOKUP.9 VOP_UNLOCK.9 VOP_LOOKUP.9 VOP_WRITE.9
MLINKS+=vhold.9 vdrop.9

.include <bsd.prog.mk>
