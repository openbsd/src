/*	$NetBSD: wd7000.c,v 1.22 1995/08/12 20:31:32 mycroft Exp $	*/

/* XXX THIS DRIVER IS BROKEN.  IT WILL NOT EVEN COMPILE. */

/*
 * UNFINISHED! UNFINISHED! UNFINISHED! UNFINISHED! UNFINISHED! UNFINISHED!
 *
 * deraadt@fsa.ca 93/04/02
 *
 * I was writing this driver for a wd7000-ASC. Yeah, the "-ASC" not the
 * "-FASST2". The difference is that the "-ASC" is missing scatter gather
 * support.
 *
 * In any case, the real reason why I never finished it is because the
 * motherboard I have has broken DMA. This card wants 8MHz 1 wait state
 * operation, and my board munges about 30% of the words transferred.
 *
 * Hopefully someone can finish this for the wd7000-FASST2. It should be
 * quite easy to do. Look at the Linux wd7000 device driver to see how
 * scatter gather is done by the board, then look at one of the Adaptec
 * drivers to finish off the job..
 */
#include "wds.h"
#if NWDS > 0

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <dev/isa/isadmavar.h>
#include <i386/isa/isa_device.h>	/* XXX BROKEN */

extern int delaycount;  /* from clock setup code */

#define PHYSTOKV(x)	((x) + KERNBASE)
#define KVTOPHYS(x)	vtophys(x)
#define PAGESIZ 	4096


/* WD7000 registers */
#define WDS_STAT		0	/* read */
#define WDS_IRQSTAT		1	/* read */

#define WDS_CMD			0	/* write */
#define WDS_IRQACK		1	/* write */
#define WDS_HCR			2	/* write */

/* WDS_STAT (read) defs */
#define WDS_IRQ			0x80
#define WDS_RDY			0x40
#define WDS_REJ			0x20
#define WDS_INIT		0x10

/* WDS_IRQSTAT (read) defs */
#define WDSI_MASK		0xc0
#define WDSI_ERR		0x00
#define WDSI_MFREE		0x80
#define WDSI_MSVC		0xc0

/* WDS_CMD (write) defs */
#define WDSC_NOOP		0x00
#define WDSC_INIT		0x01
#define WDSC_DISUNSOL		0x02
#define WDSC_ENAUNSOL		0x03
#define WDSC_IRQMFREE		0x04
#define WDSC_SCSIRESETSOFT	0x05
#define WDSC_SCSIRESETHARD	0x06
#define WDSC_MSTART(m)		(0x80 + (m))
#define WDSC_MMSTART(m)		(0xc0 + (m))

/* WDS_HCR (write) defs */
#define WDSH_IRQEN		0x08
#define WDSH_DRQEN		0x04
#define WDSH_SCSIRESET		0x02
#define WDSH_ASCRESET		0x01

struct wds_cmd {
	u_char cmd;
	u_char targ;
	struct scsi_generic scb;		/*u_char scb[12];*/
	u_char stat;
	u_char venderr;
	u_char len[3];
	u_char data[3];
	u_char next[3];
	u_char write;
	u_char xx[6];
};

struct wds_req {
	struct wds_cmd cmd;
	struct wds_cmd sense;
	struct scsi_xfer *sxp;
	int busy, polled;
	int done, ret, ombn;
};

#define WDSX_SCSICMD		0x00
#define WDSX_OPEN_RCVBUF	0x80
#define WDSX_RCV_CMD		0x81
#define WDSX_RCV_DATA		0x82
#define WDSX_RCV_DATASTAT	0x83
#define WDSX_SND_DATA		0x84
#define WDSX_SND_DATASTAT	0x85
#define WDSX_SND_CMDSTAT	0x86
#define WDSX_READINIT		0x88
#define WDSX_READSCSIID		0x89
#define WDSX_SETUNSOLIRQMASK	0x8a
#define WDSX_GETUNSOLIRQMASK	0x8b
#define WDSX_GETFIRMREV		0x8c
#define WDSX_EXECDIAG		0x8d
#define WDSX_SETEXECPARM	0x8e
#define WDSX_GETEXECPARM	0x8f

struct wds_mb {
	u_char stat;
	u_char addr[3];
};
/* ICMB status value */
#define ICMB_OK			0x01
#define ICMB_OKERR		0x02
#define ICMB_ETIME		0x04
#define ICMB_ERESET		0x05
#define ICMB_ETARCMD		0x06
#define ICMB_ERESEL		0x80
#define ICMB_ESEL		0x81
#define ICMB_EABORT		0x82
#define ICMB_ESRESET		0x83
#define ICMB_EHRESET		0x84

struct wds_setup {
	u_char cmd;
	u_char scsi_id;
	u_char buson_t;
	u_char busoff_t;
	u_char xx;
	u_char mbaddr[3];
	u_char nomb;
	u_char nimb;
};

#define WDS_NOMB	16
#define WDS_NIMB	8
#define MAXSIMUL	8
struct wds {
	int addr;
	struct wds_req wdsr[MAXSIMUL];
	struct wds_mb ombs[WDS_NOMB], imbs[WDS_NIMB];
} wds[NWDS];

static int wdsunit = 0;
int wds_debug = 0;

void p2x(u_char *, u_long);
u_char *x2p(u_char *);
int wdsprobe(struct isa_device *);
void wds_minphys(struct buf *);
struct wds_req *wdsr_alloc(int);
int wds_scsi_cmd(struct scsi_xfer *);
long wds_adapter_info(int);
int wdsintr(int);
int wds_done(int, struct wds_cmd *, u_char);
int wdsattach(struct isa_device *);
int wds_init(struct isa_device *);
int wds_cmd(int, u_char *, int);
void wds_wait(int, int, int);


struct scsi_switch wds_switch[NWDS];

struct isa_driver wdsdriver = {
	wdsprobe,
	wdsattach,
	"wds",
};


void
flushcache(void)
{
	extern main();
	volatile char *p, c;
	int i;

	for(p=(char *)main, i=0; i<256*1024; i++)
		c = *p++;
}

void
p2x(u_char *p, u_long x)
{
	p[0] = (x & 0x00ff0000) >> 16;
	p[1] = (x & 0x0000ff00) >> 8;
	p[2] = (x & 0x000000ff);
}

u_char *
x2p(u_char *x)
{
	u_long q;

	q = ((x[0]<<16) & 0x00ff0000) + ((x[1]<<8) & 0x0000ff00) + (x[2] & 0x000000ff);
	return (u_char *)q;
}

int
wdsprobe(struct isa_device *dev)
{
	/*scsi_debug = PRINTROUTINES | TRACEOPENS | TRACEINTERRUPTS |
		SHOWREQUESTS | SHOWSCATGATH | SHOWINQUIRY | SHOWCOMMANDS;*/

	if (dev->id_parent)
		return 1;

	if(wdsunit > NWDS)
		return 0;

	dev->id_unit = wdsunit;
	wds[wdsunit].addr = dev->id_iobase;

	if(wds_init(dev) != 0)
		return 0;
	wdsunit++;
	return 8;
}

void
wds_minphys(struct buf *bp)
{
	int base = (int)bp->b_data & (PAGESIZ-1);

	if (base + bp->b_bcount > PAGESIZ)
		bp->b_bcount = PAGESIZ - base;
	minphys(bp);
}

struct wds_req *
wdsr_alloc(int unit)
{
	struct wds_req *r;
	int x;
	int i;

	r = NULL;
	x = splbio();
	for(i=0; i<MAXSIMUL; i++)
		if(wds[unit].wdsr[i].busy == 0) {
			r = &wds[unit].wdsr[i];
			r->busy = 1;
			break;
		}
	if(r == NULL) {
		splx(x);
		return NULL;
	}

	r->ombn = -1;
	for(i=0; i<WDS_NOMB; i++)
		if(wds[unit].ombs[i].stat==0) {
			wds[unit].ombs[i].stat = 1;
			r->ombn = i;
			break;
		}
	if(r->ombn == -1 ) {
		r->busy = 0;
		splx(x);
		return NULL;
	}
	splx(x);
	return r;
}

int
wds_scsi_cmd(struct scsi_xfer *sxp)
{
	struct wds_req *r;
	int unit = sxp->adapter;
	int base;
	u_char c, *p;
	int i;

	base = wds[unit].addr;

	/*printf("scsi_cmd\n");*/

	if( sxp->flags & SCSI_RESET) {
		printf("reset!\n");
		return COMPLETE;
	}

	r = wdsr_alloc(unit);
	if(r==NULL) {
		printf("no request slot available!\n");
		sxp->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}
	r->done = 0;
	r->sxp = sxp;

	printf("wds%d: target %d/%d req %8x flags %08x len %d: ", unit,
		sxp->targ, sxp->lu, r, sxp->flags, sxp->cmdlen);
	for(i=0, p=(u_char *)sxp->cmd; i<sxp->cmdlen; i++)
		printf("%02x ", p[i]);
	printf("\n");
	printf("       data %08x datalen %08x\n", sxp->data, sxp->datalen);

	if(sxp->flags & SCSI_DATA_UIO) {
		printf("UIO!\n");
		sxp->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

	p2x(&wds[unit].ombs[r->ombn].addr[0], KVTOPHYS(&r->cmd));
	printf("%08x/%08x mbox@%08x: %02x %02x %02x %02x\n",
		&r->cmd, KVTOPHYS(&r->cmd), &wds[unit].ombs[0],
		wds[unit].ombs[r->ombn].stat, wds[unit].ombs[r->ombn].addr[0],
		wds[unit].ombs[r->ombn].addr[1], wds[unit].ombs[r->ombn].addr[2]);

	bzero(&r->cmd, sizeof r->cmd);
	r->cmd.cmd = WDSX_SCSICMD;
	r->cmd.targ = (sxp->targ << 5) | sxp->lu;
	bcopy(sxp->cmd, &r->cmd.scb, sxp->cmdlen<12 ? sxp->cmdlen : 12);
	p2x(&r->cmd.len[0], sxp->datalen);
	p2x(&r->cmd.data[0], sxp->datalen ? KVTOPHYS(sxp->data) : 0);
	r->cmd.write = (sxp->flags&SCSI_DATA_IN)? 0x80 : 0x00;
	p2x(&r->cmd.next[0], KVTOPHYS(&r->sense));

	bzero(&r->sense, sizeof r->sense);
	r->sense.cmd = r->cmd.cmd;
	r->sense.targ = r->cmd.targ;
	r->sense.scb.opcode = REQUEST_SENSE;
	p2x(&r->sense.data[0], KVTOPHYS(&sxp->sense));
	p2x(&r->sense.len[0], sizeof sxp->sense);
	r->sense.write = 0x80;

	/*printf("wdscmd: ");
	for(i=0, p=(u_char *)&r->cmd; i<sizeof r->cmd; i++)
		printf("%02x ", p[i]);
	printf("\n");*/

	if(sxp->flags & SCSI_NOMASK) {
		outb(base+WDS_HCR, WDSH_DRQEN);
		r->polled = 1;
	} else
		r->polled = 0;

	c = WDSC_MSTART(r->ombn);
	flushcache();
	if( wds_cmd(base, &c, sizeof c) != 0) {
		printf("wds%d: unable to start outgoing mbox\n", unit);
		r->busy = 0;
		/* XXX need to free mailbox */
		return TRY_AGAIN_LATER;
	}

	delay(10000);
	/*printf("%08x/%08x mbox: %02x %02x %02x %02x\n", &r->cmd, KVTOPHYS(&r->cmd),
		wds[unit].ombs[r->ombn].stat, wds[unit].ombs[r->ombn].addr[0],
		wds[unit].ombs[r->ombn].addr[1], wds[unit].ombs[r->ombn].addr[2]);*/

	if(sxp->flags & SCSI_NOMASK) {
repoll:		printf("wds%d: polling.", unit);
		i = 0;
		while( (inb(base+WDS_STAT) & WDS_IRQ) == 0) {
			printf(".");
			delay(10000);
			if(++i == 10) {
				printf("failed %02x\n", inb(base+WDS_IRQSTAT));
				/*r->busy = 0;*/
				sxp->error = XS_TIMEOUT;
				return HAD_ERROR;
			}
		}
		flushcache();
		printf("got one!\n");
		wdsintr(unit);
		if(r->done) {
			r->sxp->flags |= ITSDONE;
			if(r->sxp->when_done)
				(*r->sxp->when_done)(r->sxp->done_arg,
					r->sxp->done_arg2);
			r->busy = 0;
			return r->ret;
		}
		goto repoll;
	}

	outb(base+WDS_HCR, WDSH_IRQEN|WDSH_DRQEN);
	printf("wds%d: successfully queued\n", unit);
	return SUCCESSFULLY_QUEUED;
}

long
wds_adapter_info(int unit)
{
	return 1;
}

int
wdsintr(int unit)
{
	struct wds_cmd *pc, *vc;
	struct wds_mb *in;
	u_char stat;
	u_char c;

	/*printf("stat=%02x\n", inb(wds[unit].addr + WDS_STAT));*/
	delay(1000);
	c = inb(wds[unit].addr + WDS_IRQSTAT);
	printf("wdsintr: %02x\n", c);
	if( (c&WDSI_MASK) == WDSI_MSVC) {
		delay(1000);
		c = c & ~WDSI_MASK;
		flushcache();
		in = &wds[unit].imbs[c];

		printf("incoming mailbox %02x@%08x: ", c, in);
		printf("%02x %02x %02x %02x\n",
			in->stat, in->addr[0], in->addr[1], in->addr[2]);
		pc = (struct wds_cmd *)x2p(&in->addr[0]);
		vc = (struct wds_cmd *)PHYSTOKV(pc);
		stat = in->stat;
		printf("p=%08x v=%08x stat %02x\n", pc, vc, stat);
		wds_done(unit, vc, stat);
		in->stat = 0;

		outb(wds[unit].addr + WDS_IRQACK, 0xff);
	}
	return 1;
}

int
wds_done(int unit, struct wds_cmd *c, u_char stat)
{
	struct wds_req *r;
	int i;

	r = (struct wds_req *)NULL;
	for(i=0; i<MAXSIMUL; i++)
		if( c == &wds[unit].wdsr[i].cmd ) {
			/*printf("found at req slot %d\n", i);*/
			r = &wds[unit].wdsr[i];
			break;
		}
	if(r == (struct wds_req *)NULL) {
		printf("failed to find request!\n");
		return 1;
	}

	printf("wds%d: cmd %8x stat %2x/%2x %2x/%2x\n", unit, c,
		r->cmd.stat, r->cmd.venderr, r->sense.stat, r->sense.venderr);

	r->done = 1;
	/* XXX need to free mailbox */
	r->ret = HAD_ERROR;
	switch(r->cmd.stat) {
	case ICMB_OK:
		/*XXX r->sxp->sense.valid = 0;
		r->sxp->error = 0;*/
		r->ret = COMPLETE;
		break;
	case ICMB_OKERR:
 		printf("scsi err %02x\n", c->venderr);
		/*XXX r->sxp->sense.error_code = c->venderr;
		r->sxp->sense.valid = 1;*/
		r->ret = COMPLETE;
		break;
	case ICMB_ETIME:
		r->sxp->error = XS_TIMEOUT;
		r->ret = HAD_ERROR;
		break;
	case ICMB_ERESET:
	case ICMB_ETARCMD:
	case ICMB_ERESEL:
	case ICMB_ESEL:
	case ICMB_EABORT:
	case ICMB_ESRESET:
	case ICMB_EHRESET:
		r->sxp->error = XS_DRIVER_STUFFUP;
		r->ret = HAD_ERROR;
		break;
	}
	if(r->polled==0) {
		r->sxp->flags |= ITSDONE;
		if(r->sxp->when_done)
			(*r->sxp->when_done)(r->sxp->done_arg, r->sxp->done_arg2);
		r->busy = 0;
	}
	return 0;
}

int
wds_getvers(int unit)
{
	struct wds_req *r;
	int base;
	u_char c, *p;
	int i;

	base = wds[unit].addr;

	/*printf("scsi_cmd\n");*/

	r = wdsr_alloc(unit);
	if(r==NULL) {
		printf("wds%d: no request slot available!\n", unit);
		return -1;
	}
	r->done = 0;
	r->sxp = NULL;

	printf("wds%d: getvers req %8x\n", unit, r);

	p2x(&wds[unit].ombs[r->ombn].addr[0], KVTOPHYS(&r->cmd));
	printf("%08x/%08x mbox@%08x: %02x %02x %02x %02x\n",
		&r->cmd, KVTOPHYS(&r->cmd), &wds[unit].ombs[0],
		wds[unit].ombs[r->ombn].stat, wds[unit].ombs[r->ombn].addr[0],
		wds[unit].ombs[r->ombn].addr[1], wds[unit].ombs[r->ombn].addr[2]);

	bzero(&r->cmd, sizeof r->cmd);
	r->cmd.cmd = WDSX_GETFIRMREV;
	r->cmd.write = 0x80;

	printf("wdscmd: ");
	for(i=0, p=(u_char *)&r->cmd; i<sizeof r->cmd; i++)
		printf("%02x ", p[i]);
	printf("\n");

	outb(base+WDS_HCR, WDSH_DRQEN);
	r->polled = 1;

	c = WDSC_MSTART(r->ombn);
	flushcache();
	if( wds_cmd(base, &c, sizeof c) != 0) {
		printf("wds%d: unable to start outgoing mbox\n", unit);
		r->busy = 0;
		/* XXX need to free mailbox */
		return -1;
	}

	delay(10000);
	/*printf("%08x/%08x mbox: %02x %02x %02x %02x\n", &r->cmd, KVTOPHYS(&r->cmd),
		wds[unit].ombs[r->ombn].stat, wds[unit].ombs[r->ombn].addr[0],
		wds[unit].ombs[r->ombn].addr[1], wds[unit].ombs[r->ombn].addr[2]);*/

	while(1) {
		printf("wds%d: polling.", unit);
		i = 0;
		while( (inb(base+WDS_STAT) & WDS_IRQ) == 0) {
			printf(".");
			delay(10000);
			if(++i == 10) {
				printf("failed %02x\n", inb(base+WDS_IRQSTAT));
				/*r->busy = 0;*/
				return -1;
			}
		}
		flushcache();
		printf("got one!\n");
		wdsintr(unit);
		if(r->done) {
			printf("wds%d: version %02x %02x\n", unit,
				r->cmd.targ, r->cmd.scb.opcode);
			r->busy = 0;
			return 0;
		}
	}
}

int
wdsattach(struct isa_device *dev)
{
	int masunit;
	static int firstswitch[NWDS];
	static u_long versprobe		/* max 32 controllers */
	int r;

	if (!dev->id_parent)
		return 1;
	masunit = dev->id_parent->id_unit;

	if( !(versprobe & (1<<masunit))) {
		versprobe |= (1<<masunit);
		if(wds_getvers(masunit)==-1)
			printf("wds%d: getvers failed\n", masunit);
	}

	if (!firstswitch[masunit]) {
		firstswitch[masunit] = 1;
		wds_switch[masunit].name = "wds";
		wds_switch[masunit].scsi_cmd = wds_scsi_cmd;
		wds_switch[masunit].scsi_minphys = wdsminphys;
		wds_switch[masunit].open_target_lu = 0;
		wds_switch[masunit].close_target_lu = 0;
		wds_switch[masunit].adapter_info = wds_adapter_info;
		for (r = 0; r < 8; r++) {
			wds_switch[masunit].empty[r] = 0;
			wds_switch[masunit].used[r] = 0;
			wds_switch[masunit].printed[r] = 0;
		}
	}
	r = scsi_attach(masunit, &wds_switch[masunit], &dev->id_physid,
	    &dev->id_unit, dev->id_flags);
	return r;
}

int
wds_init(struct isa_device *dev)
{
	struct wds_setup init;
	int base;
	u_char *p, c;
	int unit, i;

	unit = dev->id_unit;
	base = wds[unit].addr;

	/*
	 * Sending a command causes the CMDRDY bit to clear.
 	 */
	c = inb(base+WDS_STAT);
	for(i=0; i<4; i++)
		if( (inb(base+WDS_STAT) & WDS_RDY) != 0) {
			goto ready;
		delay(10);
	}
	return 1;

ready:
	outb(base+WDS_CMD, WDSC_NOOP);
	if( inb(base+WDS_STAT) & WDS_RDY)
		return 1;

	/*
	 * the controller exists. reset and init.
	 */
	outb(base+WDS_HCR, WDSH_SCSIRESET|WDSH_ASCRESET);
	delay(3);
	outb(base+WDS_HCR, WDSH_DRQEN);
	delay(20000);

#if 1
	outb(0xd6, 0xc3);
	outb(0xd4, 0x03);
#else
	isa_dmacascade(dev->id_drq);
#endif

	if( (inb(base+WDS_STAT) & (WDS_RDY)) != WDS_RDY) {
		printf("wds%d: waiting for controller to become ready", unit);
		for(i=0; i<6; i++) {
			if( (inb(base+WDS_STAT) & (WDS_RDY)) == WDS_RDY)
				break;
			printf(".");
			delay(10000);
		}
		if( (inb(base+WDS_STAT) & (WDS_RDY)) != WDS_RDY) {
			printf("failed\n");
			return 1;
		}
	}

	bzero(&init, sizeof init);
	init.cmd = WDSC_INIT;
	init.scsi_id = 0;
	init.buson_t = 24;
	init.busoff_t = 48;
	p2x(&init.mbaddr[0], KVTOPHYS(&wds[unit].ombs[0]));
	init.xx = 0;
	init.nomb = WDS_NOMB;
	init.nimb = WDS_NIMB;

	/*p = (u_char *)&init;
	printf("wds%d: %08x %08x init: ", unit,
		&wds[unit].ombs[0], KVTOPHYS(&wds[unit].ombs[0]));
	for(i=0; i<sizeof init; i++)
		printf("%02x ", p[i]);
	printf("\n");*/

	wds_wait(base+WDS_STAT, WDS_RDY, WDS_RDY);
	flushcache();
	if( wds_cmd(base, (u_char *)&init, sizeof init) != 0) {
		printf("wds%d: wds_cmd failed\n", unit);
		return 1;
	}
	wds_wait(base+WDS_STAT, WDS_INIT, WDS_INIT);

	wds_wait(base+WDS_STAT, WDS_RDY, WDS_RDY);
	c = WDSC_DISUNSOL;
	if( wds_cmd(base, &c, sizeof c) != 0) {
		printf("wds%d: wds_cmd failed\n", unit);
		return 1;
	}

	return 0;
}

int
wds_cmd(int base, u_char *p, int l)
{
	int i;
	u_char c;

	i = 0;
	while(i < l) {
		while( ((c=inb(base+WDS_STAT)) & WDS_RDY) == 0)
			;

		outb(base+WDS_CMD, *p);

		while( ((c=inb(base+WDS_STAT)) & WDS_RDY) == 0)
			;

		if(c & WDS_REJ)
			return 1;
		p++;
		i++;
	}
	while( ((c=inb(base+WDS_STAT)) & WDS_RDY) == 0)
		;
	if(c & WDS_REJ)
		return 1;
	/*printf("wds_cmd: %02x\n", inb(base+WDS_STAT));*/
	return 0;
}

void
wds_wait(int reg, int mask, int val)
{
	while( (inb(reg) & mask) != val)
		;
}

#endif
