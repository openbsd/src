/*	$NetBSD: ch.c,v 1.14 1995/01/16 21:31:38 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * Originally written by grefen@?????
 * Based on scsi drivers by Julian Elischer (julian@tfs.com)
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/chio.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_changer.h>
#include <scsi/scsiconf.h>

#define	CHRETRIES	2

#define CHMODE(z)	(minor(z) & 0x0f)
#define CHUNIT(z)	(minor(z) >> 4)

struct ch_softc {
	struct device sc_dev;

	struct scsi_link *sc_link;	/* all the inter level info */
	u_int16_t chmo;			/* Offset of first CHM */
	u_int16_t chms;			/* No. of CHM */
	u_int16_t slots;		/* No. of Storage Elements */
	u_int16_t sloto;		/* Offset of first SE */
	u_int16_t imexs;		/* No. of Import/Export Slots */
	u_int16_t imexo;		/* Offset of first IM/EX */
	u_int16_t drives;		/* No. of CTS */
	u_int16_t driveo;		/* Offset of first CTS */
	u_int16_t rot;			/* CHM can rotate */
	u_long  op_matrix;		/* possible operations */
	u_int16_t lsterr;		/* details of lasterror */
	u_char  stor;			/* posible Storage locations */
};

int chmatch __P((struct device *, void *, void *));
void chattach __P((struct device *, struct device *, void *));

struct cfdriver chcd = {
	NULL, "ch", chmatch, chattach, DV_DULL, sizeof(struct ch_softc)
};

/*
 * This driver is so simple it uses all the default services
 */
struct scsi_device ch_switch = {
	NULL,
	NULL,
	NULL,
	NULL,
};

struct scsi_inquiry_pattern ch_patterns[] = {
	{T_CHANGER, T_REMOV,
	 "",         "",                 ""},
};

int
chmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)ch_patterns, sizeof(ch_patterns)/sizeof(ch_patterns[0]),
	    sizeof(ch_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
void 
chattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ch_softc *ch = (void *)self;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("chattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	ch->sc_link = sc_link;
	sc_link->device = &ch_switch;
	sc_link->device_softc = ch;
	sc_link->openings = 1;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	if (ch_mode_sense(ch, SCSI_AUTOCONF) != 0)
		printf(": offline\n");
	else
		printf(": %d slot(s), %d drive(s), %d arm(s), %d i/e-slot(s)\n",
		    ch->slots, ch->drives, ch->chms, ch->imexs);
}

/*
 *    open the device.
 */
int 
chopen(dev)
	dev_t dev;
{
	int error = 0;
	int unit, mode;
	struct ch_softc *ch;
	struct scsi_link *sc_link;

	unit = CHUNIT(dev);
	if (unit >= chcd.cd_ndevs)
		return ENXIO;
	ch = chcd.cd_devs[unit];
	if (!ch)
		return ENXIO;

	mode = CHMODE(dev);
	sc_link = ch->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("chopen: dev=0x%x (unit %d (of %d))\n", dev, unit, chcd.cd_ndevs));

	/*
	 * Only allow one at a time
	 */
	if (sc_link->flags & SDEV_OPEN) {
		printf("%s: already open\n", ch->sc_dev.dv_xname);
		return EBUSY;
	}

	/*
	 * Catch any unit attention errors.
	 */
	if (error = scsi_test_unit_ready(sc_link, SCSI_IGNORE_MEDIA_CHANGE))
		goto bad;

	sc_link->flags |= SDEV_OPEN;	/* unit attn are now errors */

	/*
	 * Make sure data is loaded
	 */
	if (error = ch_mode_sense(ch, 0)) {
		printf("%s: offline\n", ch->sc_dev.dv_xname);
		goto bad;
	}

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	return 0;

bad:
	sc_link->flags &= ~SDEV_OPEN;
	return error;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int 
chclose(dev)
	dev_t dev;
{
	struct ch_softc *ch = chcd.cd_devs[CHUNIT(dev)];

	SC_DEBUG(ch->sc_link, SDEV_DB1, ("closing\n"));
	ch->sc_link->flags &= ~SDEV_OPEN;

	return 0;
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
int 
chioctl(dev, cmd, arg, mode, p)
	dev_t dev;
	u_long cmd;
	caddr_t arg;
	int mode;
	struct proc *p;
{
	struct ch_softc *ch = chcd.cd_devs[CHUNIT(dev)];
	struct scsi_link *sc_link = ch->sc_link;
	int number;
	int flags;

	/*
	 * Find the device that the user is talking about
	 */
	flags = 0;		/* give error messages, act on errors etc. */

	switch (cmd) {
	case CHIOOP: {
		struct chop *chop = (struct chop *) arg;
		SC_DEBUG(sc_link, SDEV_DB2, ("[chtape_chop: %x]\n",
		    chop->ch_op));

		switch (chop->ch_op) {
		case CHGETPARAM:
			chop->u.getparam.chmo = ch->chmo;
			chop->u.getparam.chms = ch->chms;
			chop->u.getparam.sloto = ch->sloto;
			chop->u.getparam.slots = ch->slots;
			chop->u.getparam.imexo = ch->imexo;
			chop->u.getparam.imexs = ch->imexs;
			chop->u.getparam.driveo = ch->driveo;
			chop->u.getparam.drives = ch->drives;
			chop->u.getparam.rot = ch->rot;
			chop->result = 0;
			return 0;
			break;
		case CHPOSITION:
			return ch_position(ch, &chop->result,
			    chop->u.position.chm, chop->u.position.to, flags);
		case CHMOVE:
			return ch_move(ch, &chop->result, chop->u.position.chm,
			    chop->u.move.from, chop->u.move.to, flags);
		case CHGETELEM:
			return ch_getelem(ch, &chop->result,
			    chop->u.get_elem_stat.type,
			    chop->u.get_elem_stat.from,
			    &chop->u.get_elem_stat.elem_data, flags);
		default:
			return EINVAL;
		}
	}
	default:
		return scsi_do_ioctl(sc_link, dev, cmd, arg, mode, p);
	}
#ifdef DIAGNOSTIC
	panic("chioctl: impossible");
#endif
}

int 
ch_getelem(ch, stat, type, from, data, flags)
	struct ch_softc *ch;
	short *stat;
	int type, from;
	char *data;
	int flags;
{
	struct scsi_read_element_status scsi_cmd;
	char elbuf[32];
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = READ_ELEMENT_STATUS;
	scsi_cmd.byte2 = type;
	scsi_cmd.starting_element_addr[0] = (from >> 8) & 0xff;
	scsi_cmd.starting_element_addr[1] = from & 0xff;
	scsi_cmd.number_of_elements[1] = 1;
	scsi_cmd.allocation_length[2] = 32;

	error = scsi_scsi_cmd(ch->sc_link, (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd), (u_char *) elbuf, 32, CHRETRIES, 100000, NULL,
	    SCSI_DATA_IN | flags);
	if (error)
		*stat = ch->lsterr;
	else
		*stat = 0;
	bcopy(elbuf + 16, data, 16);
	return error;
}

int 
ch_move(ch, stat, chm, from, to, flags)
	struct ch_softc *ch;
	short *stat;
	int chm, from, to, flags;
{
	struct scsi_move_medium scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MOVE_MEDIUM;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (from >> 8) & 0xff;
	scsi_cmd.source_address[1] = from & 0xff;
	scsi_cmd.destination_address[0] = (to >> 8) & 0xff;
	scsi_cmd.destination_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	error = scsi_scsi_cmd(ch->sc_link, (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd), NULL, 0, CHRETRIES, 100000, NULL, flags);
	if (error)
		*stat = ch->lsterr;
	else
		*stat = 0;
	return error;
}

int 
ch_position(ch, stat, chm, to, flags)
	struct ch_softc *ch;
	short *stat;
	int chm, to, flags;
{
	struct scsi_position_to_element scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = POSITION_TO_ELEMENT;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (to >> 8) & 0xff;
	scsi_cmd.source_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	error = scsi_scsi_cmd(ch->sc_link, (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd), NULL, 0, CHRETRIES, 100000, NULL, flags);
	if (error)
		*stat = ch->lsterr;
	else
		*stat = 0;
	return error;
}

/*
 * Get the scsi driver to send a full inquiry to the
 * device and use the results to fill out the global 
 * parameter structure.
 */
int 
ch_mode_sense(ch, flags)
	struct ch_softc *ch;
	int flags;
{
	struct scsi_mode_sense scsi_cmd;
	u_char scsi_sense[128];	/* Can't use scsi_mode_sense_data because of
				 * missing block descriptor.
				 */
	u_char *b;
	int i, l;
	int error;
	struct scsi_link *sc_link = ch->sc_link;

	/*
	 * First check if we have it all loaded
	 */
	if (sc_link->flags & SDEV_MEDIA_LOADED)
		return 0;

	/*
	 * First do a mode sense 
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.byte2 = SMS_DBD;
	scsi_cmd.page = 0x3f;	/* All Pages */
	scsi_cmd.length = sizeof(scsi_sense);

	/*
	 * Read in the pages
	 */
	if (error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &scsi_cmd,
	    sizeof(scsi_cmd), (u_char *) &scsi_sense, sizeof(scsi_sense),
	    CHRETRIES, 5000, NULL, flags | SCSI_DATA_IN)) {
		printf("%s: could not mode sense\n", ch->sc_dev.dv_xname);
		return error;
	}

	sc_link->flags |= SDEV_MEDIA_LOADED;
	l = scsi_sense[0] - 3;
	b = &scsi_sense[4];

	/*
	 * To avoid alignment problems
	 */
/* XXXX - FIX THIS FOR MSB */
#define p2copy(valp)	 (valp[1] | (valp[0]<<8)); valp+=2
#define p4copy(valp)	 (valp[3] | (valp[2]<<8) | (valp[1]<<16) | (valp[0]<<24)); valp+=4
#if 0
	printf("\nmode_sense %d\n", l);
	for (i = 0; i < l + 4; i++)
		printf("%x%c", scsi_sense[i], i % 8 == 7 ? '\n' : ':');
	printf("\n");
#endif
	for (i = 0; i < l;) {
		u_char pc = (*b++) & 0x3f;
		u_char pl = *b++;
		u_char *bb = b;
		switch (pc) {
		case 0x1d:
			ch->chmo = p2copy(bb);
			ch->chms = p2copy(bb);
			ch->sloto = p2copy(bb);
			ch->slots = p2copy(bb);
			ch->imexo = p2copy(bb);
			ch->imexs = p2copy(bb);
			ch->driveo = p2copy(bb);
			ch->drives = p2copy(bb);
			break;
		case 0x1e:
			ch->rot = *b & 0x1;
			break;
		case 0x1f:
			ch->stor = *b & 0xf;
			bb += 2;
			ch->stor = p4copy(bb);
			break;
		default:
			break;
		}
		b += pl;
		i += pl + 2;
	}
	SC_DEBUG(sc_link, SDEV_DB2,
	    (" cht(%d-%d)slot(%d-%d)imex(%d-%d)cts(%d-%d) %s rotate\n",
	    ch->chmo, ch->chms, ch->sloto, ch->slots, ch->imexo, ch->imexs,
	    ch->driveo, ch->drives, ch->rot ? "can" : "can't"));
	return 0;
}
