/*	$OpenBSD: aacvar.h,v 1.2 2002/03/14 01:26:53 millert Exp $	*/

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: /c/ncvs/src/sys/dev/aac/aacvar.h,v 1.1 2000/09/13 03:20:34 msmith Exp $
 */

/*
 * This driver would not have rewritten for OpenBSD if it was not for the
 * hardware dontion from Nocom.  I want to thank them for their support.
 * Of course, credit should go to Mike Smith for the original work he did
 * in the FreeBSD driver where I found lots of inspiration.
 * - Niklas Hallqvist
 */

/* Debugging */
#ifdef AAC_DEBUG
#define AAC_DPRINTF(mask, args) if (aac_debug & (mask)) printf args
#define AAC_D_INTR	0x01
#define AAC_D_MISC	0x02
#define AAC_D_CMD	0x04
#define AAC_D_QUEUE	0x08
#define AAC_D_IO	0x10
extern int aac_debug;

#define AAC_PRINT_FIB(sc, fib)	aac_print_fib((sc), (fib), __FUNCTION__)
#else
#define AAC_DPRINTF(mask, args)
#define AAC_PRINT_FIB(sc, fib)
#endif

struct aac_code_lookup {
	char	*string;
	u_int32_t code;
};

struct aac_softc;

/*
 * We allocate a small set of FIBs for the adapter to use to send us messages.
 */
#define AAC_ADAPTER_FIBS	8

/*
 * Firmware messages are passed in the printf buffer.
 */
#define AAC_PRINTF_BUFSIZE	256

/*
 * We wait this many seconds for the adapter to come ready if it is still
 * booting.
 */
#define AAC_BOOT_TIMEOUT	(3 * 60)

/*
 * Wait this long for a lost interrupt to get detected.
 */
#define AAC_WATCH_TIMEOUT	10000		/* 10000 * 1ms = 10s */

/*
 * Timeout for immediate commands.
 */
#define AAC_IMMEDIATE_TIMEOUT	30

/*
 * Delay 20ms after the qnotify in sync operations.  Experimentally deduced.
 */
#define AAC_SYNC_DELAY 20000

/*
 * The firmware interface allows for a 16-bit s/g list length.  We limit 
 * ourselves to a reasonable maximum and ensure alignment.
 */
#define AAC_MAXSGENTRIES	64	/* max S/G entries, limit 65535 */		
/*
 * We gather a number of adapter-visible items into a single structure.
 *
 * The ordering of this strucure may be important; we copy the Linux driver:
 *
 * Adapter FIBs
 * Init struct
 * Queue headers (Comm Area)
 * Printf buffer
 *
 * In addition, we add:
 * Sync Fib
 */
struct aac_common {
	/* fibs for the controller to send us messages */
	struct aac_fib ac_fibs[AAC_ADAPTER_FIBS];

	/* the init structure */
	struct aac_adapter_init	ac_init;

	/* arena within which the queue structures are kept */
	u_int8_t ac_qbuf[sizeof(struct aac_queue_table) + AAC_QUEUE_ALIGN];

	/* buffer for text messages from the controller */
	char	ac_printf[AAC_PRINTF_BUFSIZE];
    
	/* fib for synchronous commands */
	struct aac_fib ac_sync_fib;
};

/*
 * Interface operations
 */
struct aac_interface {
	int	(*aif_get_fwstatus)(struct aac_softc *);
	void	(*aif_qnotify)(struct aac_softc *, int);
	int	(*aif_get_istatus)(struct aac_softc *);
	void	(*aif_set_istatus)(struct aac_softc *, int);
	void	(*aif_set_mailbox)(struct aac_softc *, u_int32_t,
	    u_int32_t, u_int32_t, u_int32_t, u_int32_t);
	int	(*aif_get_mailboxstatus)(struct aac_softc *);
	void	(*aif_set_interrupts)(struct aac_softc *, int);
};
extern struct aac_interface aac_rx_interface;
extern struct aac_interface aac_sa_interface;

#define AAC_GET_FWSTATUS(sc)		((sc)->sc_if.aif_get_fwstatus(sc))
#define AAC_QNOTIFY(sc, qbit) \
	((sc)->sc_if.aif_qnotify((sc), (qbit)))
#define AAC_GET_ISTATUS(sc)		((sc)->sc_if.aif_get_istatus(sc))
#define AAC_CLEAR_ISTATUS(sc, mask) \
	((sc)->sc_if.aif_set_istatus((sc), (mask)))
#define AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3) \
	do {								\
		((sc)->sc_if.aif_set_mailbox((sc), (command), (arg0),	\
		    (arg1), (arg2), (arg3)));				\
	} while(0)
#define AAC_GET_MAILBOXSTATUS(sc) \
	((sc)->sc_if.aif_get_mailboxstatus(sc))
#define	AAC_MASK_INTERRUPTS(sc)	\
	((sc)->sc_if.aif_set_interrupts((sc), 0))
#define AAC_UNMASK_INTERRUPTS(sc) \
	((sc)->sc_if.aif_set_interrupts((sc), 1))

#define AAC_SETREG4(sc, reg, val) \
	bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, (reg), (val))
#define AAC_GETREG4(sc, reg) \
	bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, (reg))
#define AAC_SETREG2(sc, reg, val) \
	bus_space_write_2((sc)->sc_memt, (sc)->sc_memh, (reg), (val))
#define AAC_GETREG2(sc, reg) \
	bus_space_read_2((sc)->sc_memt, (sc)->sc_memh, (reg))
#define AAC_SETREG1(sc, reg, val) \
	bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (reg), (val))
#define AAC_GETREG1(sc, reg) \
	bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (reg))

/*
 * Per-container data structure
 */
struct aac_container
{
	struct aac_mntobj co_mntobj;
	struct device co_disk;
};

/*
 * A command contol block, one for each corresponding command index of the
 * controller.
 */
struct aac_ccb {
	TAILQ_ENTRY(aac_ccb) ac_chain;
	struct scsi_xfer *ac_xs;
	struct aac_fib *ac_fib;		/* FIB associated with this command */
	bus_addr_t ac_fibphys;		/* bus address of the FIB */
	bus_dmamap_t ac_dmamap_xfer;
	struct aac_sg_table *ac_sgtable;/* pointer to s/g table in command */
	int ac_timeout;
	u_int32_t ac_blockno;
	u_int32_t ac_blockcnt;
	u_int8_t ac_flags;
#define AAC_ACF_WATCHDOG 	0x1
#define AAC_ACF_COMPLETED 	0x2
};

/*
 * Per-controller structure.
 */
struct aac_softc {
	struct device sc_dev;
	void   *sc_ih;
	struct	scsi_link sc_link;	/* Virtual SCSI bus for cache devs */

	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;
	bus_dma_tag_t sc_dmat;		/* parent DMA tag */

	/* controller features, limits and status */
	int	sc_state;
#define AAC_STATE_SUSPEND	(1<<0)
#define	AAC_STATE_OPEN		(1<<1)
#define AAC_STATE_INTERRUPTS_ON	(1<<2)
#define AAC_STATE_AIF_SLEEPER	(1<<3)
	struct FsaRevision sc_revision;

	int	sc_hwif;	/* controller hardware interface */
#define AAC_HWIF_I960RX		0
#define AAC_HWIF_STRONGARM	1

	struct aac_common *sc_common;
	u_int32_t sc_common_busaddr;
	struct aac_interface sc_if;

	/* XXX This should really be dynamic.  It is very wasteful now. */
	struct aac_ccb sc_ccbs[AAC_ADAP_NORM_CMD_ENTRIES];
	TAILQ_HEAD(, aac_ccb) sc_free_ccb, sc_ccbq;
	/* commands on hold for controller resources */
	TAILQ_HEAD(, aac_ccb) sc_ready;
	/* commands which have been returned by the controller */
	TAILQ_HEAD(, aac_ccb) sc_completed;
	LIST_HEAD(, scsi_xfer) sc_queue;
	struct scsi_xfer *sc_queuelast;

	/* command management */
	struct aac_queue_table *sc_queues;
	struct aac_queue_entry *sc_qentries[AAC_QUEUE_COUNT];

	struct {
		u_int8_t hd_present;
		u_int8_t hd_is_logdrv;
		u_int8_t hd_is_arraydrv;
		u_int8_t hd_is_master;
		u_int8_t hd_is_parity;
		u_int8_t hd_is_hotfix;
		u_int8_t hd_master_no;
		u_int8_t hd_lock;
		u_int8_t hd_heads;
		u_int8_t hd_secs;
		u_int16_t hd_devtype;
		u_int32_t hd_size;
		u_int8_t hd_ldr_no;
		u_int8_t hd_rw_attribs;
		u_int32_t hd_start_sec;
	} sc_hdr[AAC_MAX_CONTAINERS];
};

/* XXX These have to become spinlocks in case of SMP */
#define AAC_LOCK(sc) splbio()
#define AAC_UNLOCK(sc, lock) splx(lock)
typedef int aac_lock_t;

void	aacminphys(struct buf *);
int	aac_attach(struct aac_softc *);
int	aac_intr(void *);

#ifdef __GNUC__
/* These all require correctly aligned buffers */
static __inline__ void aac_enc16(u_int8_t *, u_int16_t);
static __inline__ void aac_enc32(u_int8_t *, u_int32_t);
static __inline__ u_int16_t aac_dec16(u_int8_t *);
static __inline__ u_int32_t aac_dec32(u_int8_t *);

static __inline__ void
aac_enc16(addr, value)
	u_int8_t *addr;
	u_int16_t value;
{
	*(u_int16_t *)addr = htole16(value);
}

static __inline__ void
aac_enc32(addr, value)
	u_int8_t *addr;
	u_int32_t value;
{
	*(u_int32_t *)addr = htole32(value);
}

static __inline__ u_int16_t
aac_dec16(addr)
	u_int8_t *addr;
{
	return letoh16(*(u_int16_t *)addr);
}

static __inline__ u_int32_t
aac_dec32(addr)
	u_int8_t *addr;
{
	return letoh32(*(u_int32_t *)addr);
}

/*
 * Queue primitives
 *
 * These are broken out individually to make statistics gathering easier.
 */

static __inline__ void
aac_enqueue_completed(struct aac_ccb *ccb)
{
	struct aac_softc *sc = ccb->ac_xs->sc_link->adapter_softc;
	aac_lock_t lock;

	lock = AAC_LOCK(sc);
	TAILQ_INSERT_TAIL(&sc->sc_completed, ccb, ac_chain);
	AAC_UNLOCK(sc, lock);
}

static __inline__ struct aac_ccb *
aac_dequeue_completed(struct aac_softc *sc)
{
	struct aac_ccb *ccb;
	aac_lock_t lock;

	lock = AAC_LOCK(sc);
	if ((ccb = TAILQ_FIRST(&sc->sc_completed)) != NULL)
		TAILQ_REMOVE(&sc->sc_completed, ccb, ac_chain);
	AAC_UNLOCK(sc, lock);
	return (ccb);
}
#endif
