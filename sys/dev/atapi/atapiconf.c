/*	$OpenBSD: atapiconf.c,v 1.22 1998/06/09 13:29:58 provos Exp $	*/

/*
 * Copyright (c) 1996 Manuel Bouyer.  All rights reserved.
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
 *  This product includes software developed by Manuel Bouyer.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>  

#include <dev/atapi/atapilink.h>
#include <dev/atapi/atapi.h>

#define SILENT_PRINTF(flags,string) if (!(flags & A_SILENT)) printf string

#ifdef ATAPI_DEBUG_CMD
#define ATAPI_DEBUG_CMD_PRINT(args)	printf args
#else
#define ATAPI_DEBUG_CMD_PRINT(args)
#endif

#ifdef ATAPI_DEBUG_FCTN
#define ATAPI_DEBUG_FCTN_PRINT(args)	printf args
#else
#define ATAPI_DEBUG_FCTN_PRINT(args)
#endif

struct atapibus_softc {
	struct	device sc_dev;
	struct	bus_link *b_link;
};

LIST_HEAD(pkt_free_list, atapi_command_packet) pkt_free_list;

int atapi_error __P((struct atapi_command_packet *));
void atapi_sense __P((struct atapi_command_packet *, u_int8_t, u_int8_t));
void at_print_addr __P((struct at_dev_link *, u_int8_t));

int atapibusmatch __P((struct device *, void *, void *));
void atapibusattach __P((struct device *, struct device *, void *));
void atapi_fixquirk __P((struct at_dev_link *));
int atapiprint __P((void *, const char *));

struct cfattach atapibus_ca = {
	sizeof(struct atapibus_softc), atapibusmatch, atapibusattach
};

struct cfdriver atapibus_cd = {
	NULL, "atapibus", DV_DULL
};


/*
 * ATAPI quirk table support.
 */

struct atapi_quirk_inquiry_pattern {
	u_int8_t type;
	u_int8_t rem;
	char *product;
	char *revision;

	u_int8_t quirks;
};

struct atapi_quirk_inquiry_pattern atapi_quirk_inquiry_patterns[] = {
	/* GoldStar 8X */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "GCD-R580B", "1.00", AQUIRK_LITTLETOC},
	/* MATSHITA CR-574 */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "MATSHITA CR-574", "1.06", AQUIRK_NOCAPACITY},
	/* NEC Multispin 2Vi */
	{ATAPI_DEVICE_TYPE_DAD, ATAPI_REMOVABLE,
	 "NEC                 CD-ROM DRIVE:260", "3.04", AQUIRK_CDROM},
	/* NEC 273 */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "NEC                 CD-ROM DRIVE:273", "4.21", AQUIRK_NOTUR},
	/* NEC 4CD changer CDR-C251 */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "NEC                 CD-ROM DRIVE:251", "4.14", AQUIRK_NOCAPACITY},
	/* Sanyo 4x */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "SANYO CRD-254P", "1.02", AQUIRK_NOCAPACITY},
	/* Sanyo 4x */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "SANYO CRD-S54P", "1.08", AQUIRK_NOCAPACITY},
	/* Sanyo 6x */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "SANYO CRD-256P", "1.02", AQUIRK_NOCAPACITY},
	/* Another Sanyo 4x */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "CD-ROM  CDR-S1", "1.70",AQUIRK_NOCAPACITY},
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "CD-ROM  CDR-N16", "1.25",AQUIRK_NOCAPACITY},
	/* Acer Notelight 370 */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "UJDCD8730", "1.14", AQUIRK_NODOORLOCK},
	/* ALPS CD changer */
	{ATAPI_DEVICE_TYPE_CD, ATAPI_REMOVABLE,
	 "ALPS ELECTRIC CO.,LTD. DC544C", "SW03D", AQUIRK_NOTUR},
	{0, 0, NULL, NULL, 0}
};

int
atapibusmatch(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct bus_link *ab_link = aux;

	if (ab_link == NULL)
		return 0;
	if (ab_link->type != BUS)
		return 0;
	return 1;
}

void
atapi_fixquirk(ad_link)
	struct at_dev_link *ad_link;
{
	struct atapi_identify *id = &ad_link->id;
	struct atapi_quirk_inquiry_pattern *quirk;

	/*
	 * Shuffle string byte order.
	 * Mitsumi and NEC drives don't need this.
	 */
	if (((id->model[0] == 'N' && id->model[1] == 'E') ||
	    (id->model[0] == 'F' && id->model[1] == 'X')) == 0)
		bswap(id->model, sizeof(id->model));
	bswap(id->serial_number, sizeof(id->serial_number));
	bswap(id->firmware_revision, sizeof(id->firmware_revision));

	/*
	 * Clean up the model name, serial and
	 * revision numbers.
	 */
	btrim(id->model, sizeof(id->model));
	btrim(id->serial_number, sizeof(id->serial_number));
	btrim(id->firmware_revision, sizeof(id->firmware_revision));

#define quirk_null(_q)	((_q->type == 0) && (_q->rem == 0) && \
			 (_q->product == NULL) && (_q->revision == NULL))

	for (quirk = atapi_quirk_inquiry_patterns;
	     !quirk_null(quirk); quirk++) {
		if ((id->config.device_type
		    & ATAPI_DEVICE_TYPE_MASK) != quirk->type)
			continue;
		if ((id->config.cmd_drq_rem & quirk->rem) == 0)
			continue;
		if (strcmp(id->model, quirk->product))
			continue;
		if (strcmp(id->firmware_revision, quirk->revision))
			continue;

		break;
	}

	if (!quirk_null(quirk)) {
		/* Found a quirk entry for this drive. */
		ad_link->quirks = quirk->quirks;
	}
}

int
atapiprint(aux, bus)
	void *aux;
	const char *bus;
{
	struct at_dev_link *ad_link = aux;
	struct atapi_identify *id = &ad_link->id;
	char *dtype, *fixrem;

	/*
	 * Figure out basic device type.
	 */
	switch (id->config.device_type & ATAPI_DEVICE_TYPE_MASK) {
	case ATAPI_DEVICE_TYPE_DAD:
		dtype = "direct";
		break;

	case ATAPI_DEVICE_TYPE_CD:
		dtype = "cdrom";
		break;

	case ATAPI_DEVICE_TYPE_OMD:
		dtype = "optical";
		break;

	default:
		dtype = "unknown";
		break;
	}

	fixrem = (id->config.cmd_drq_rem & ATAPI_REMOVABLE) ?
	    "removable" : "fixed";

	if (bus != NULL)
		printf("%s", bus);

	if (id->serial_number[0]) {
		printf(" drive %d: <%s, %s, %s> ATAPI %d/%s %s",
	    	    ad_link->drive, id->model, id->serial_number,
	    	    id->firmware_revision,
	    	    (id->config.device_type & ATAPI_DEVICE_TYPE_MASK),
		    dtype, fixrem);
	} else {
		printf(" drive %d: <%s, %s> ATAPI %d/%s %s",
	    	    ad_link->drive, id->model, id->firmware_revision,
	    	    (id->config.device_type & ATAPI_DEVICE_TYPE_MASK),
		    dtype, fixrem);
	}

	return UNCONF;
}


void
atapibusattach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct atapibus_softc *ab = (struct atapibus_softc *)self;
	struct bus_link *ab_link_proto = aux;
	struct atapi_identify ids;
	struct atapi_identify *id = &ids;
	struct at_dev_link *ad_link;
	int drive;

	printf("\n");

	ab_link_proto->atapibus_softc = (caddr_t)ab;
	ab->b_link = ab_link_proto;

	for (drive = 0; drive < 2 ; drive++) {
		if (wdc_atapi_get_params(ab_link_proto, drive, id)) {
#ifdef ATAPI_DEBUG_PROBE
			printf("%s drive %d: cmdsz 0x%x drqtype 0x%x\n",
			    self->dv_xname, drive,
			    id->config.cmd_drq_rem & ATAPI_PACKET_SIZE_MASK,
			    id->config.cmd_drq_rem & ATAPI_DRQ_MASK);
#endif

			/*
			 * Allocate a device link and try and attach
			 * a driver to this device.  If we fail, free
			 * the link.
			 */
			ad_link = malloc(sizeof(*ad_link), M_DEVBUF, M_NOWAIT);
			if (ad_link == NULL) {
				printf("%s: can't allocate link for drive %d\n",
				    self->dv_xname, drive);
				continue;
			}

			/* Fill in link. */
			ad_link->drive = drive;
			if (id->config.cmd_drq_rem & ATAPI_PACKET_SIZE_16)
				ad_link->flags |= ACAP_LEN;
			ad_link->flags |=
			    (id->config.cmd_drq_rem & ATAPI_DRQ_MASK) << 3;
			ad_link->bus = ab_link_proto;
			bcopy(id, &ad_link->id, sizeof(*id));

			/* Fix strings and look through the quirk table. */
			atapi_fixquirk(ad_link);

			/* Try to find a match. */
			if (config_found(self, ad_link, atapiprint) == NULL)
				free(ad_link, M_DEVBUF);
		}
	}
}

int
atapi_exec_cmd(ad_link, cmd, cmd_size, databuf, datalen, rw, flags)
	struct at_dev_link *ad_link;
	void *cmd;
	int cmd_size;
	void *databuf;
	int datalen;
	int rw;
	int flags;
{
	struct atapi_command_packet *pkt;
	struct bus_link *b_link = ad_link->bus;
	int status, s;

	/* Allocate packet. */
	pkt = atapi_get_pkt(ad_link, flags);
	if (pkt == NULL)
		return -1;

	/* Fill it out. */
	bcopy(cmd, &pkt->cmd_store, cmd_size);
	pkt->command = &pkt->cmd_store;
	pkt->command_size = (ad_link->flags & ACAP_LEN) ? 16 : 12;
	pkt->databuf = databuf;
	pkt->data_size = datalen;
	pkt->flags = rw | (flags & 0xff) | (ad_link->flags & 0x0300);
	pkt->drive = ad_link->drive;

	/* Send it to drive. */
	wdc_atapi_send_command_packet(b_link, pkt);
	if ((flags & (A_POLLED | A_NOSLEEP)) == 0) {
		ATAPI_DEBUG_CMD_PRINT(("atapi_exec_cmd: sleeping\n"));

		s = splbio();
		while ((pkt->status & ITSDONE) == 0)
			tsleep(pkt, PRIBIO + 1,"atapicmd", 0);
		splx(s);

		ATAPI_DEBUG_CMD_PRINT(("atapi_exec_cmd: done sleeping\n"));

		status = pkt->status & STATUS_MASK;
		atapi_free_pkt(pkt);
	} else {
		if ((flags & A_POLLED) != 0) {
			if ((pkt->status & ERROR) && (pkt->error)) {
				atapi_error(pkt);
				SILENT_PRINTF(flags,("\n"));
			}
		}
		status = pkt->status & STATUS_MASK;
		if ((flags & A_POLLED) != 0)
			atapi_free_pkt(pkt);
	}

	if ((pkt->status & ERROR) && (pkt->error))
	     status |= pkt->error << 8;

	return status;
}

int
atapi_exec_io(ad_link, cmd, cmd_size, bp, flags)
	struct at_dev_link *ad_link;
	void *cmd;
	int cmd_size;
	struct buf *bp;
	int flags;
{
	struct atapi_command_packet *pkt;
	struct bus_link *b_link = ad_link->bus;

	/* Allocate a packet. */
	pkt = atapi_get_pkt(ad_link, flags);
	if (pkt == NULL) {
		printf("atapi_exec_io: no pkt\n");
		return ERROR;
	}

	/* Fill it in. */
	bcopy(cmd, &pkt->cmd_store, cmd_size);
	pkt->command = &pkt->cmd_store;
	pkt->command_size = (ad_link->flags & ACAP_LEN) ? 16 : 12;
	pkt->bp = bp;
	pkt->databuf = bp->b_data;
	pkt->data_size = bp->b_bcount;
	pkt->flags = (bp->b_flags & (B_READ|B_WRITE)) | (flags & 0xff) |
	    (ad_link->flags & 0x0300);
	pkt->drive = ad_link->drive;

	wdc_atapi_send_command_packet(b_link, pkt);
	return (pkt->status & STATUS_MASK);
}

void
atapi_done(acp)
	struct atapi_command_packet *acp;
{
	struct at_dev_link *ad_link = acp->ad_link;
	struct buf *bp = acp->bp;
	int error = 0;

	ATAPI_DEBUG_CMD_PRINT(("atapi_done\n"));

	if ((acp->status & ERROR) && (acp->error)) {
		atapi_error(acp);
		if (acp->status & RETRY) {
			if (acp->retries <ATAPI_NRETRIES) {
				acp->retries++;
				acp->status = 0;
				acp->error = 0;
				SILENT_PRINTF(acp->flags & 0xff,
				    (", retry #%d\n", acp->retries));
				wdc_atapi_send_command_packet(ad_link->bus,
				    acp);
				return;
			} else
				acp->status = ERROR;
		}
		SILENT_PRINTF(acp->flags & 0xff,("\n"));
	}
	acp->status |= ITSDONE;

	if (ad_link->done) {
		ATAPI_DEBUG_CMD_PRINT(("calling private done\n"));
		error = (*ad_link->done)(acp);
		if (error == EJUSTRETURN)
			return;
	}
	if (acp->bp == NULL) {
		ATAPI_DEBUG_CMD_PRINT(("atapidone: wakeup acp\n"));
		wakeup(acp);
		return;
	}

	ATAPI_DEBUG_CMD_PRINT(("atapi_done: status %d\n", acp->status));

	switch (acp->status & 0x0f) {
	case MEDIA_CHANGE:
		if (ad_link->flags & ADEV_REMOVABLE)
			ad_link->flags &= ~ADEV_MEDIA_LOADED;

		error = EIO;
		break;

	case NO_ERROR:
		error = 0;
		break;

	case ERROR:
	case END_OF_MEDIA:
	default:
		error = EIO;
		break;
	}

	switch (acp->status & 0xf0) {
	case NOT_READY:
	case UNIT_ATTENTION:
		if (ad_link->flags & ADEV_REMOVABLE)
			ad_link->flags &= ~ADEV_MEDIA_LOADED;

		error = EIO;
		break;
	}

	if (error) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
	} else {
		bp->b_error = 0;
		bp->b_resid = acp->data_size;
	}
	biodone(bp);
	atapi_free_pkt(acp);
}

struct atapi_command_packet *
atapi_get_pkt(ad_link, flags)
	struct at_dev_link *ad_link;
	int flags;
{
	struct atapi_command_packet *pkt;
	int s;

	s = splbio();
	while (ad_link->openings <= 0) {
		if (flags & A_NOSLEEP) {
			splx(s);
			return 0;
		}

		ATAPI_DEBUG_CMD_PRINT(("atapi_get_pkt: sleeping\n"));

		ad_link->flags |= ADEV_WAITING;
		(void)tsleep(ad_link, PRIBIO, "getpkt", 0);
	}

	ad_link->openings--;

	if ((pkt = pkt_free_list.lh_first) != 0) {
		LIST_REMOVE(pkt, free_list);
		splx(s);
	} else {
		splx(s);
		pkt = malloc(sizeof(struct atapi_command_packet), M_DEVBUF,
		    ((flags & A_NOSLEEP) != 0 ? M_NOWAIT : M_WAITOK));
		if (pkt == NULL) {
			printf("atapi_get_pkt: cannot allocate pkt\n");
			ad_link->openings++;
			return 0;
		}
	}

	bzero(pkt, sizeof(struct atapi_command_packet));
	pkt->ad_link = ad_link;
	return pkt;
}

void
atapi_free_pkt(pkt)
	struct atapi_command_packet *pkt;
{
	struct at_dev_link *ad_link = pkt->ad_link;
	int s;

	s = splbio();
	LIST_INSERT_HEAD(&pkt_free_list, pkt, free_list);

	ad_link->openings++;

	if ((ad_link->flags & ADEV_WAITING) != 0) {
		ad_link->flags &= ~ADEV_WAITING;
		wakeup(ad_link);
	} else {
		if (ad_link->start) {
			ATAPI_DEBUG_CMD_PRINT(("atapi_free_pkt: calling private start\n"));
			(*ad_link->start)((void *)ad_link->device_softc);
		}
	}
	splx(s);
}

int
atapi_test_unit_ready(ad_link, flags)
	struct at_dev_link *ad_link;
	int flags;
{	
	int ret;
	struct atapi_test_unit_ready cmd;

	ATAPI_DEBUG_FCTN_PRINT(("atapi_test_unit_ready: "));

	/* Device doesn't support TUR! */
	if (ad_link->quirks & AQUIRK_NOTUR)
		ret = 0;
	else {
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = ATAPI_TEST_UNIT_READY;
		ret = atapi_exec_cmd(ad_link, &cmd, sizeof(cmd), 0, 0, 0,
		    flags);
	}
	ATAPI_DEBUG_FCTN_PRINT(("atapi_test_unit_ready: ret %d\n", ret));
	return ret;
}

int
atapi_start_stop(ad_link, how, flags)
	struct at_dev_link  *ad_link;
	int how;
	int flags;
{
	struct atapi_start_stop_unit cmd;
	int ret;
		
	ATAPI_DEBUG_FCTN_PRINT(("atapi_start_stop: "));

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = ATAPI_START_STOP_UNIT;
	cmd.how = how;

	ret = atapi_exec_cmd(ad_link, &cmd, sizeof(cmd), 0,0,0,flags);

	ATAPI_DEBUG_FCTN_PRINT(("ret %d\n", ret));

	return ret;
}

int
atapi_prevent(ad_link, how)
	struct at_dev_link *ad_link;
	int how;
{
	struct atapi_prevent_allow_medium_removal cmd;
	int ret;

	ATAPI_DEBUG_FCTN_PRINT(("atapi_prevent: "));

	if (ad_link->quirks & AQUIRK_NODOORLOCK) 
	        ret = 0;
	else { 
	        bzero(&cmd, sizeof(cmd));
		cmd.opcode = ATAPI_PREVENT_ALLOW_MEDIUM_REMOVAL;
		cmd.how = how & 0xff;

		ret = atapi_exec_cmd(ad_link, &cmd, sizeof(cmd), 0,0,0,0);
	}
	ATAPI_DEBUG_FCTN_PRINT(("ret %d\n", ret));

	return ret;
}

int 
atapi_error(acp)
	struct atapi_command_packet* acp;
{
	int flags, error, ret = -1;
	struct at_dev_link *ad_link = acp->ad_link;

	flags = acp->flags & 0xff;
	error = acp->error;

	at_print_addr(ad_link, acp->flags & 0xff);

	if (error & ATAPI_MCR) {
		SILENT_PRINTF(flags,("media change requested "));
		acp->status = MEDIA_CHANGE;
	}

	if (error & ATAPI_ABRT) {      
		SILENT_PRINTF(flags,("command aborted "));
		acp->status = ERROR; 
	}

	if (error & ATAPI_EOM) {
		SILENT_PRINTF(flags,("end of media "));
		acp->status = END_OF_MEDIA;
	}

	if (error & ATAPI_ILI) {
		SILENT_PRINTF(flags,("illegal length indication "));
		acp->status = ERROR;
	}

	if ((error & 0x0f) == 0)
		ret = 0;

	atapi_sense(acp, error >> 4, flags);

	if (((flags & A_SILENT) == 0) && (acp->status != NO_ERROR)) {
		int i;
		printf(", command:");
		for (i = 0; i < acp->command_size; i++)
			printf(" %2x", ((u_int8_t *)acp->command)[i]);
	}

	return ret;
}

void
atapi_sense(acp, sense_key, flags)
	struct atapi_command_packet *acp;
	u_int8_t sense_key;
	u_int8_t flags;
{
	struct at_dev_link *ad_link = acp->ad_link;

	switch (sense_key) {
	case ATAPI_SK_NO_SENSE:
		break;

	case ATAPI_SK_REC_ERROR:
		SILENT_PRINTF(flags,("recovered error"));
		acp->status = 0;
		break;

	case ATAPI_SK_NOT_READY:
		SILENT_PRINTF(flags,("not ready"));
		acp->status = NOT_READY;
		break;

	case ATAPI_SK_MEDIUM_ERROR:
		SILENT_PRINTF(flags,("medium error"));
		acp->status = ERROR;
		break;

	case ATAPI_SK_HARDWARE_ERROR:
		SILENT_PRINTF(flags,("hardware error"));
		acp->status = ERROR;
		break;

	case ATAPI_SK_ILLEGAL_REQUEST:
		SILENT_PRINTF(flags,("illegal request"));
		acp->status = ERROR;
		break;

	case ATAPI_SK_UNIT_ATTENTION:
		SILENT_PRINTF(flags,("unit attention"));
		acp->status = UNIT_ATTENTION;
		if (ad_link->flags & ADEV_REMOVABLE)
			ad_link->flags &= ~ADEV_MEDIA_LOADED;
		break;

	case ATAPI_SK_DATA_PROTECT:
		SILENT_PRINTF(flags,("data protect"));
		acp->status = ERROR;
		break;

	case ATAPI_SK_ABORTED_COMMAND:
		SILENT_PRINTF(flags,("aborted command"));
		acp->status = RETRY;
		break;

	case ATAPI_SK_MISCOMPARE:
		SILENT_PRINTF(flags,("miscompare"));
		acp->status = ERROR;
		break;

	default:
		SILENT_PRINTF(flags,("unexpected sense key %02x", sense_key));
		acp->status = ERROR;
	}
}

void
at_print_addr(ad_link, flags)
	struct at_dev_link *ad_link; 
	u_int8_t flags;
{

	if (flags & A_SILENT)
		return;

	printf("%s(%s:%d): ", ad_link->device_softc ?
	    ((struct device *)ad_link->device_softc)->dv_xname : "probe",
	    ((struct device *)ad_link->bus->wdc_softc)->dv_xname,
	    ad_link->drive);
}
