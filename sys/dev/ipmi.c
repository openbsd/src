/* $OpenBSD: ipmi.c,v 1.2 2005/10/05 02:02:11 deraadt Exp $ */

/*
 * Copyright (c) 2005 Jordan Hargrave
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/timeout.h>
#include <sys/sensors.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ipmivar.h>

struct ipmi_sensor {
	u_int8_t	*i_sdr;
	int		i_num;
	struct		sensor i_sensor;
	SLIST_ENTRY(ipmi_sensor) list;
};

int	ipmi_nintr;
int	ipmi_dbg = 0;

#define SENSOR_REFRESH_RATE (10 * hz)

#define SMBIOS_TYPE_IPMI	0x26

#define BIT(x) (1L << (x))

#define IPMI_BTMSG_LEN		0
#define IPMI_BTMSG_NFLN		1
#define IPMI_BTMSG_SEQ		2
#define IPMI_BTMSG_CMD		3
#define IPMI_BTMSG_CCODE	4
#define IPMI_BTMSG_DATAIN	4
#define IPMI_BTMSG_DATAOUT	5

#define IPMI_MSG_NFLN		0
#define IPMI_MSG_CMD		1
#define IPMI_MSG_CCODE		2
#define IPMI_MSG_DATAIN		2
#define IPMI_MSG_DATAOUT	3

#define byteof(x) ((x) >> 3)
#define bitof(x)  (1L << ((x) & 0x7))
#define TB(b,m)   (data[2+byteof(b)] & bitof(b))

#define dbg_printf(lvl,fmt...)    if (ipmi_dbg >= lvl) { printf(fmt); }
#define dbg_dump(lvl,msg,len,buf) if (len && ipmi_dbg >= lvl) { dumpb(msg,len,(const u_int8_t *)(buf)); }

SLIST_HEAD(ipmi_sensors_head, ipmi_sensor);
struct ipmi_sensors_head ipmi_sensor_list =
    SLIST_HEAD_INITIALIZER(&ipmi_sensor_list);

struct timeout ipmi_timeout;

void	smbios_ipmi_probe(void *, void *);
void	dumpb(const char *, int, const u_int8_t *);

int	read_sensor(struct ipmi_softc *, struct ipmi_sensor *);
int	add_sdr_sensor(struct ipmi_softc *, u_int8_t *);
int	get_sdr_partial(struct ipmi_softc *, u_int16_t, u_int16_t,
	    u_int8_t, u_int8_t, void *, u_int16_t *);
int	get_sdr(struct ipmi_softc *, u_int16_t, u_int16_t *);

int	ipmi_sendcmd(struct ipmi_softc *, int, int, int, int, int, const void*);
int	ipmi_recvcmd(struct ipmi_softc *, int, int *, void *);

int	ipmi_intr(void *);
int	ipmi_probe(struct device *, void *, void *);
void	ipmi_attach(struct device *, struct device *, void *);

long	ipow(long, int);
long	ipmi_convert(u_int8_t, sdrtype1 *, long);
void	ipmi_sensor_name(char *, int, u_int8_t, u_int8_t *);

/* BMC Helper Functions */
uint8_t	bmc_read(struct ipmi_softc *, int);
void	bmc_write(struct ipmi_softc *, int, uint8_t);
int	bmc_io_wait(struct ipmi_softc *, int, u_int8_t, u_int8_t, long,
	    const char *);

void	*bt_buildmsg(struct ipmi_softc *, int, int, int, const void *, int *);
void	*cmn_buildmsg(struct ipmi_softc *, int, int, int, const void *, int *);

int	getbits(u_int8_t *, int, int);
int	valid_sensor(int, int);

void    ipmi_refresh(void *arg);
void    ipmi_refresh_sensors(struct ipmi_softc *sc);
int	ipmi_map_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia);
void    ipmi_unmap_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia);

struct ipmi_if kcs_if = {
	"kcs",
	IPMI_IF_KCS_NREGS,
	cmn_buildmsg,
	kcs_sendmsg,
	kcs_recvmsg,
	kcs_reset,
	kcs_probe,
};

struct ipmi_if smic_if = {
	"smic",
	IPMI_IF_SMIC_NREGS,
	cmn_buildmsg,
	smic_sendmsg,
	smic_recvmsg,
	smic_reset,
	smic_probe,
};

struct ipmi_if bt_if = {
	"bt",
	IPMI_IF_BT_NREGS,
	bt_buildmsg,
	bt_sendmsg,
	bt_recvmsg,
	bt_reset,
	bt_probe,
};

struct ipmi_if *ipmi_get_if(int);
struct ipmi_if *
ipmi_get_if(int iftype)
{
	switch (iftype) {
	case IPMI_IF_KCS:
		return (&kcs_if);

	case IPMI_IF_SMIC:
		return (&smic_if);

	case IPMI_IF_BT:
		return (&bt_if);
	}

	return (NULL);
}

/*
 * BMC Helper Functions
 */
uint8_t
bmc_read(struct ipmi_softc *sc, int offset)
{
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    offset * sc->sc_if_iospacing));
}

void
bmc_write(struct ipmi_softc *sc, int offset, uint8_t val)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    offset * sc->sc_if_iospacing, val);
}

int
bmc_io_wait(struct ipmi_softc *sc, int offset, u_int8_t mask,
    u_int8_t value, long count, const char *lbl)
{
	volatile u_int8_t v;

	/* Spin loop (ugly) */
	while (count--) {
		v = bmc_read(sc, offset);
		if ((v & mask) == value) {
			return v;
		}
	}
	printf("bmc_io_wait fails : v=%.2x m=%.2x b=%.2x %s\n",
	    v, mask, value, lbl);
	return (-1);
}

#define NETFN_LUN(nf,ln) (((nf) << 2) | ((ln) & 0x3))

/*
 * BT interface
 */
#define _BT_CTRL_REG			0
#define BT_CLR_WR_PTR			BIT(0)
#define BT_CLR_RD_PTR			BIT(1)
#define BT_HOST2BMC_ATN			BIT(2)
#define BT_BMC2HOST_ATN			BIT(3)
#define BT_EVT_ATN			BIT(4)
#define BT_HOST_BUSY			BIT(6)
#define BT_BMC_BUSY			BIT(7)

#define _BT_DATAIN_REG			1
#define _BT_DATAOUT_REG			1
#define _BT_INTMASK_REG			2

int
bt_sendmsg(struct ipmi_softc *sc, int len, const u_int8_t *data)
{
	int i;

	bmc_write(sc, _BT_CTRL_REG, BT_CLR_WR_PTR);
	for (i = 0; i < len; i++)
		bmc_write(sc, _BT_DATAOUT_REG, data[i]);

	bmc_write(sc, _BT_CTRL_REG, BT_HOST2BMC_ATN);
	return (0);
}

int
bt_recvmsg(struct ipmi_softc *sc, int maxlen, int *rxlen, u_int8_t *data)
{
	uint8_t len, v, i;

	/* BT Result data: 0: len   1:nfln   2:seq   3:cmd   4:ccode
	 * 5:data... */
	len = bmc_read(sc, _BT_DATAIN_REG);
	for (i = IPMI_BTMSG_NFLN; i <= len; i++) {
		/* Ignore sequence number */
		v = bmc_read(sc, _BT_DATAIN_REG);
		if (i != IPMI_BTMSG_SEQ)
			*(data++) = v;
	}
	bmc_write(sc, _BT_CTRL_REG, BT_BMC2HOST_ATN | BT_HOST_BUSY);
	*rxlen = len - 1;

#if 0
	data[IPMI_MSG_NFLN] = data[IPMI_BTMSG_
#endif
	    return (0);
}

int
bt_reset(struct ipmi_softc *sc)
{
	return (-1);
}

int
bt_probe(struct ipmi_softc *sc)
{
	uint8_t v;

	v = bmc_read(sc, _BT_CTRL_REG);
#if 0
	printf("bt_probe: %2x\n", v);
	printf(" WR    : %2x\n", v & BT_CLR_WR_PTR);
	printf(" RD    : %2x\n", v & BT_CLR_RD_PTR);
	printf(" H2B   : %2x\n", v & BT_HOST2BMC_ATN);
	printf(" B2H   : %2x\n", v & BT_BMC2HOST_ATN);
	printf(" EVT   : %2x\n", v & BT_EVT_ATN);
	printf(" HBSY  : %2x\n", v & BT_HOST_BUSY);
	printf(" BBSY  : %2x\n", v & BT_BMC_BUSY);
#endif
	return (-1);
}

/*
 * SMIC interface
 */
#define _SMIC_DATAIN_REG		0
#define _SMIC_DATAOUT_REG		0

#define _SMIC_CTRL_REG			1

#define _SMIC_FLAG_REG			2
#define  SMIC_BUSY			BIT(0)
#define  SMIC_SMS_ATN			BIT(2)
#define  SMIC_EVT_ATN			BIT(3)
#define  SMIC_SMI			BIT(4)
#define  SMIC_TX_DATA_RDY		BIT(6)
#define  SMIC_RX_DATA_RDY		BIT(7)

#if 0
int
smic_wait(struct ipmi_softc *sc, u_int8_t mask, u_int8_t val, const char *lbl)
{
	return (inb(SMIC_CNTL_REGISTER(sc)));
}

int
smic_write_cmd_data(struct ipmi_softc *sc, u_int8_t cmd, const u_int8_t *data)
{
	int	sts;

	sts = smic_wait(sc, SMIC_TX_DATA_RDY | SMIC_BUSY, SMIC_TX_DATA_RDY,
	    "smic_write_cmd_data ready");
	if (sts != 0)
		return (sts);

	bmc_write(sc, _SMIC_CTRL_REG, cmd);
	if (data)
		bmc_write(sc, _SMIC_DATAOUT_REG, *data);

	v = bmc_read(sc, _SMIC_FLAG_REG);
	bmc_write(sc, _SMIC_FLAG_REG, v | SMIC_BUSY);

	return (smic_wait(sc, SMIC_BUSY, 0, "smic_write_cmd_data busy"));
}

int
smic_read_data(struct ipmi_softc *sc, u_int8_t *data)
{
	sts = smic_wait(sc, SMIC_RX_DATA_RDY | SMIC_BUSY, SMIC_RX_DATA_RDY,
	    "smic_read_data");
	if (sts != SMIC_STATE_READ)
		return (sts);

	sts = bmc_read(sc, _SMIC_CNTL_REG);
	*data = bmc_read(sc, _SMIC_DATAIN_REG);
	return (sts);
}
#endif

int
smic_sendmsg(struct ipmi_softc *sc, int len, const u_int8_t *data)
{
#if 0
	int sts, idx;

	sts = smic_write_cmd_data(sc, SMS_CC_START_TRANSFER, &data[0]);
	for (idx = 0; idx < len - 1; idx++)
		sts = smic_write_cmd_data(sc, SMS_CC_NEXT_TRANSFER, &data[idx]);

	sts = smic_write_cmd(sc, SMS_CC_END_TRANSFER, &data[idx]);
#endif
	return (-1);
}

int
smic_recvmsg(struct ipmi_softc *sc, int maxlen, int *len, u_int8_t *data)
{
#if 0
	int sts, idx;

	sts = smic_write_cmd_data(sc, SMS_CC_START_RECEIVE, NULL);
	for (idx = 0;; idx++) {
		smic_read_data(sc, &data[idx]);
		smic_write_cmd_data(sc, SMS_CC_NEXT_RECEIVE, NULL);
	}
	smic_write_cmd_data(sc, SMS_CC_END_RECEIVE, NULL);
#endif
	return (-1);
}

int
smic_reset(struct ipmi_softc *sc)
{
	return (-1);
}

int
smic_probe(struct ipmi_softc *sc)
{
	return (-1);
}

/*
 * KCS interface
 */
#define _KCS_DATAIN_REGISTER		0
#define _KCS_DATAOUT_REGISTER		0
#define   KCS_READ_NEXT			0x68

#define _KCS_COMMAND_REGISTER		1
#define   KCS_GET_STATUS		0x60
#define   KCS_WRITE_START		0x61
#define   KCS_WRITE_END		0x62

#define _KCS_STATUS_REGISTER		1
#define   KCS_OBF			BIT(0)
#define   KCS_IBF			BIT(1)
#define   KCS_SMS_ATN			BIT(2)
#define   KCS_CD			BIT(3)
#define   KCS_OEM1			BIT(4)
#define   KCS_OEM2			BIT(5)
#define   KCS_STATE_MASK		0xc0

#define KCS_IDLE_STATE		0x00
#define KCS_READ_STATE		0x40
#define KCS_WRITE_STATE		0x80
#define KCS_ERROR_STATE		0xC0

int	kcs_wait(struct ipmi_softc *, u_int8_t, u_int8_t, const char *);
int	kcs_write_cmd(struct ipmi_softc *, u_int8_t);
int	kcs_write_data(struct ipmi_softc *, u_int8_t);
int	kcs_read_data(struct ipmi_softc *, u_int8_t *);

int
kcs_wait(struct ipmi_softc *sc, u_int8_t mask, u_int8_t value, const char *lbl)
{
	int v;

	v = bmc_io_wait(sc, _KCS_STATUS_REGISTER, mask, value, 0xFFFFF, lbl);
	if (v < 0)
		return (v);

	/* Check if output buffer full, read dummy byte  */
	if (value == 0 && (v & KCS_OBF))
		bmc_read(sc, _KCS_DATAIN_REGISTER);

	/* Check for error state */
	if ((v & KCS_STATE_MASK) == KCS_ERROR_STATE) {
		bmc_write(sc, _KCS_COMMAND_REGISTER, KCS_GET_STATUS);
		while (bmc_read(sc, _KCS_STATUS_REGISTER) & KCS_IBF);
		printf(" error code: %x\n", bmc_read(sc, _KCS_DATAIN_REGISTER));
	}

	return (v & KCS_STATE_MASK);
}

int
kcs_write_cmd(struct ipmi_softc *sc, u_int8_t cmd)
{
	/* ASSERT: IBF and OBF are clear */
	dbg_printf(99, "kcswritecmd: %.2x\n", cmd);
	bmc_write(sc, _KCS_COMMAND_REGISTER, cmd);

	return (kcs_wait(sc, KCS_IBF, 0, "write_cmd"));
}

int
kcs_write_data(struct ipmi_softc *sc, u_int8_t data)
{
	/* ASSERT: IBF and OBF are clear */
	dbg_printf(99, "kcswritedata: %.2x\n", data);
	bmc_write(sc, _KCS_DATAOUT_REGISTER, data);

	return (kcs_wait(sc, KCS_IBF, 0, "write_data"));
}

int
kcs_read_data(struct ipmi_softc *sc, u_int8_t * data)
{
	int sts;

	sts = kcs_wait(sc, KCS_IBF | KCS_OBF, KCS_OBF, "read_data");
	if (sts != KCS_READ_STATE)
		return (sts);

	/* ASSERT: OBF is set read data, request next byte */
	*data = bmc_read(sc, _KCS_DATAIN_REGISTER);
	bmc_write(sc, _KCS_DATAOUT_REGISTER, KCS_READ_NEXT);

	dbg_printf(99, "kcsreaddata: %.2x\n", *data);

	return (sts);
}

/* Exported KCS functions */
int
kcs_sendmsg(struct ipmi_softc *sc, int len, const u_int8_t * data)
{
	int idx, sts;

	/* ASSERT: IBF is clear */
	dbg_dump(50, "kcs sendmsg", len, data);
	sts = kcs_write_cmd(sc, KCS_WRITE_START);
	for (idx = 0; idx < len; idx++) {
		if (idx == len - 1)
			sts = kcs_write_cmd(sc, KCS_WRITE_END);

		if (sts != KCS_WRITE_STATE)
			break;

		sts = kcs_write_data(sc, data[idx]);
	}
	if (sts != KCS_READ_STATE) {
		printf("kcs sendmsg = %d/%d <%.2x>\n", idx, len, sts);
		dumpb("kcs_sendmsg", len, data);
	}

	return (sts != KCS_READ_STATE);
}

int
kcs_recvmsg(struct ipmi_softc *sc, int maxlen, int *rxlen, u_int8_t * data)
{
	int idx, sts;

	for (idx = 0; idx < maxlen; idx++) {
		sts = kcs_read_data(sc, &data[idx]);
		if (sts != KCS_READ_STATE)
			break;
	}
	sts = kcs_wait(sc, KCS_IBF, 0, "recv");
	*rxlen = idx;
	if (sts != KCS_IDLE_STATE)
		printf("kcs read = %d/%d <%.2x>\n", idx, maxlen, sts);

	dbg_dump(50, "kcs recvmsg", idx, data);

	return (sts != KCS_IDLE_STATE);
}

int
kcs_reset(struct ipmi_softc *sc)
{
	return (-1);
}

int
kcs_probe(struct ipmi_softc *sc)
{
	u_int8_t v;

	v = bmc_read(sc, _KCS_STATUS_REGISTER);
#if 0
	printf("kcs_probe: %2x\n", v);
	printf(" STS: %2x\n", v & KCS_STATE_MASK);
	printf(" ATN: %2x\n", v & KCS_SMS_ATN);
	printf(" C/D: %2x\n", v & KCS_CD);
	printf(" IBF: %2x\n", v & KCS_IBF);
	printf(" OBF: %2x\n", v & KCS_OBF);
#endif
	return (0);
}

/*
 * IPMI code
 */
#define READ_SMS_BUFFER		0x37
#define WRITE_I2C		0x50

#define GET_MESSAGE_CMD		0x33
#define SEND_MESSAGE_CMD	0x34

#define IPMB_CHANNEL_NUMBER	0

#define PUBLIC_BUS		0

#define MIN_I2C_PACKET_SIZE	3
#define MIN_IMB_PACKET_SIZE	7	/* one byte for cksum */

#define MIN_BTBMC_REQ_SIZE	4
#define MIN_BTBMC_RSP_SIZE	5
#define MIN_BMC_REQ_SIZE	2
#define MIN_BMC_RSP_SIZE	3

#define BMC_SA			0x20	/* BMC/ESM3 */
#define FPC_SA			0x22	/* front panel */
#define BP_SA			0xC0	/* Primary Backplane */
#define BP2_SA			0xC2	/* Secondary Backplane */
#define PBP_SA			0xC4	/* Peripheral Backplane */
#define DRAC_SA			0x28	/* DRAC-III */
#define DRAC3_SA		0x30	/* DRAC-III */
#define BMC_LUN			0
#define SMS_LUN			2

typedef struct {
	u_int8_t	rsSa;
	u_int8_t	rsLun;
	u_int8_t	netFn;
	u_int8_t	cmd;
	u_int8_t	data_len;
	u_int8_t	*data;
} ipmi_request_;

typedef struct {
	u_int8_t	cCode;
	u_int8_t	data_len;
	u_int8_t	*data;
} ipmi_response_t;

typedef struct {
	u_int8_t	bmc_nfLn;
	u_int8_t	bmc_cmd;
	u_int8_t	bmc_data_len;
	u_int8_t	bmc_data[1];
} ipmi_bmc_request_t;

typedef struct {
	u_int8_t	bmc_nfLn;
	u_int8_t	bmc_cmd;
	u_int8_t	bmc_cCode;
	u_int8_t	bmc_data_len;
	u_int8_t	bmc_data[1];
} ipmi_bmc_response_t;

struct cfattach ipmi_ca = {
	sizeof(struct ipmi_softc), ipmi_probe, ipmi_attach
};

struct cfdriver ipmi_cd = {
	NULL, "ipmi", DV_DULL
};

void	*scan_sig(long, long, int, int, const void *);
int	scan_smbios(u_int8_t, void (*)(void *, void *), void *);

/* Scan memory for signature */
void *
scan_sig(long start, long end, int skip, int len, const void *data)
{
	void *va;

	while (start < end) {
		va = ISA_HOLE_VADDR(start);
		if (memcmp(va, data, len) == 0)
			return (va);

		start += skip;
	}

	return (NULL);
}

/* Scan SMBIOS for table type */
int
scan_smbios(u_int8_t mtype, void (*smcb) (void *base, void *arg), void *arg) {
	smbiosanchor_t	*romhdr;
	smhdr_t		*smhdr;
	u_int8_t	*offset;
	int		nmatch, num;

	/* Scan for SMBIOS Table Signature */
	romhdr = (smbiosanchor_t *) scan_sig(0xF0000, 0xFFFFF, 16, 4, "_SM_");
	if (romhdr == NULL)
		return (-1);

#if 0
	printf("Found SMBIOS Version %d.%d at 0x%lx, %d entries\n",
	    romhdr->smr_smbios_majver,
	    romhdr->smr_smbios_minver,
	    romhdr->smr_table_address,
	    romhdr->smr_count);
#endif
	/* XXX: Need to handle correctly if SMBIOS in high memory Get offset
	 * of SMBIOS Table entries */
	nmatch = 0;
	offset = ISA_HOLE_VADDR(romhdr->smr_table_address);
	for (num = 0; num < romhdr->smr_count; num++) {
		smhdr = (smhdr_t *) offset;
		if (smhdr->smh_type == SMBIOS_TYPE_END ||
		    smhdr->smh_length == 0)
			break;

		/* found a match here */
		if (smhdr->smh_type == mtype) {
			smcb(&smhdr[1], arg);
			nmatch++;
		}
		/* Search for end of string table, marked by '\0\0' */
		offset += smhdr->smh_length;
		while (offset[0] || offset[1])
			offset++;

		offset += 2;
	}

	return (nmatch);
}

void
dumpb(const char *lbl, int len, const u_int8_t *data)
{
	int idx;

	printf("%s: ", lbl);
	for (idx = 0; idx < len; idx++)
		printf("%.2x ", data[idx]);

	printf("\n");
}

void
smbios_ipmi_probe(void *ptr, void *arg)
{
	struct ipmi_attach_args	*ia = arg;
	smbios_ipmi_t		*pipmi = (smbios_ipmi_t *)ptr;

	ia->iaa_if_type = pipmi->smipmi_if_type;
	ia->iaa_if_rev = pipmi->smipmi_if_rev;
	ia->iaa_if_irq = (pipmi->smipmi_base_flags & BIT(3)) ?
	    pipmi->smipmi_irq : -1;
	ia->iaa_if_irqlvl = (pipmi->smipmi_base_flags & BIT(1)) ?
	    IST_LEVEL : IST_EDGE;

	switch (pipmi->smipmi_base_flags >> 6) {
	case 0:
		ia->iaa_if_iospacing = 1;
		break;

	case 1:
		ia->iaa_if_iospacing = 4;
		break;

	case 2:
		ia->iaa_if_iospacing = 2;
		break;

	default:
		ia->iaa_if_iospacing = 1;
		printf("ipmi: unknown register spacing\n");
	}

	/* Calculate base address (PCI BAR format) */
	if (pipmi->smipmi_base_address & 0x1) {
		ia->iaa_if_iotype = 'i';
		ia->iaa_if_iobase = pipmi->smipmi_base_address & ~0x1;
	} else {
		ia->iaa_if_iotype = 'm';
		ia->iaa_if_iobase = pipmi->smipmi_base_address & ~0xF;
	}
	if (pipmi->smipmi_base_flags & BIT(4)) {
		ia->iaa_if_iobase++;
	}
}

/*
 * bt_buildmsg builds an IPMI message from a nfLun, cmd, and data
 * This is used by BT protocol
 *
 * Returns a buffer to an allocated message, txlen contains length
 *   of allocated message
 */
void *
bt_buildmsg(struct ipmi_softc *sc, int netlun, int cmd, int len,
    const void *data, int *txlen)
{
	u_int8_t *buf;

	/* Block transfer needs 4 extra bytes: length/netfn/seq/cmd + data */
	*txlen = len + 4;
	buf = malloc(*txlen, M_DEVBUF, M_WAITOK);
	if (buf == NULL)
		return (NULL);

	buf[IPMI_BTMSG_LEN] = len + 3;
	buf[IPMI_BTMSG_NFLN] = netlun;
	buf[IPMI_BTMSG_SEQ] = sc->sc_btseq++;
	buf[IPMI_BTMSG_CMD] = cmd;
	if (len && data)
		memcpy(buf + IPMI_BTMSG_DATAIN, data, len);

	return (buf);
}

/*
 * cmn_buildmsg builds an IPMI message from a nfLun, cmd, and data
 * This is used by both SMIC and KCS protocols
 *
 * Returns a buffer to an allocated message, txlen contains length
 *   of allocated message
 */
void *
cmn_buildmsg(struct ipmi_softc *sc, int nfLun, int cmd, int len,
    const void *data, int *txlen)
{
	u_int8_t *buf;

	/* Common needs two extra bytes: nfLun/cmd + data */
	*txlen = len + 2;
	buf = malloc(*txlen, M_DEVBUF, M_WAITOK);
	if (buf == NULL)
		return (NULL);

	buf[IPMI_MSG_NFLN] = nfLun;
	buf[IPMI_MSG_CMD] = cmd;
	if (len && data)
		memcpy(buf + IPMI_MSG_DATAIN, data, len);

	return (buf);
}

/* Send an IPMI command */
int
ipmi_sendcmd(struct ipmi_softc *sc, int rssa, int rslun, int netfn, int cmd,
    int txlen, const void *data)
{
	u_int8_t	*buf;
	int		rc;

	dbg_printf(10, "ipmi_sendcmd: rssa=%.2x nfln=%.2x cmd=%.2x len=%.2x\n",
	    rssa, NETFN_LUN(netfn, rslun), cmd, txlen);
	dbg_dump(10, " send", txlen, data);
	if (rssa != BMC_SA) {
#if 0
		buf = sc->sc_if->buildmsg(sc, NETFN_LUN(APP_NETFN, BMC_LUN),
		    APP_SEND_MESSAGE, 7 + txlen, NULL, &txlen);
		pI2C->bus = (sc->if_ver == 0x09) ?
		    PUBLIC_BUS :
		    IPMB_CHANNEL_NUMBER;

		imbreq->rsSa = rssa;
		imbreq->nfLn = NETFN_LUN(netfn, rslun);
		imbreq->cSum1 = -(imbreq->rsSa + imbreq->nfLn);
		imbreq->rqSa = BMC_SA;
		imbreq->seqLn = NETFN_LUN(sc->imb_seq++, SMS_LUN);
		imbreq->cmd = cmd;
		if (txlen) {
			memcpy(imbreq->data, data, txlen);
		}
		/* Set message checksum */
		imbreq->data[txlen] = cksum8(&imbreq->rqSa, txlen + 3);
#endif
		return (-1);
	} else
		buf = sc->sc_if->buildmsg(sc, NETFN_LUN(netfn, rslun), cmd,
		    txlen, data, &txlen);

	if (buf == NULL) {
		printf("sendcmd malloc fails\n");
		return (-1);
	}
	rc = sc->sc_if->sendmsg(sc, txlen, buf);
	free(buf, M_DEVBUF);

	return (rc);
}

int
ipmi_recvcmd(struct ipmi_softc *sc, int maxlen, int *rxlen, void *data)
{
	u_int8_t	*buf, rc = 0;
	int		rawlen;

	/* Need three extra bytes: netfn/cmd/ccode + data */
	buf = malloc(maxlen + 3, M_DEVBUF, M_WAITOK);
	if (buf == NULL) {
		printf("ipmi_recvcmd: malloc fails\n");
		return -1;
	}
	/* Receive message from interface, copy out result data */
	sc->sc_if->recvmsg(sc, maxlen + 3, &rawlen, buf);

	*rxlen = rawlen - IPMI_MSG_DATAOUT;
	if (*rxlen > 0 && data)
		memcpy(data, buf + IPMI_MSG_DATAOUT, *rxlen);

	if ((rc = buf[IPMI_MSG_CCODE]) != 0) {
		dbg_printf(1, "ipmi_recvmsg: nfln=%.2x cmd=%.2x err=%.2x\n",
		    buf[IPMI_MSG_NFLN], buf[IPMI_MSG_CMD], buf[IPMI_MSG_CCODE]);
	}
	dbg_printf(10, "ipmi_recvcmd: nfln=%.2x cmd=%.2x err=%.2x len=%.2x\n",
	    buf[IPMI_MSG_NFLN], buf[IPMI_MSG_CMD], buf[IPMI_MSG_CCODE],
	    *rxlen);
	dbg_dump(10, " recv", *rxlen, data);

	free(buf, M_DEVBUF);

	return (rc);
}

/* Read a partial SDR entry */
int
get_sdr_partial(struct ipmi_softc *sc, u_int16_t recordId, u_int16_t reserveId,
    u_int8_t offset, u_int8_t length, void *buffer, u_int16_t *nxtRecordId)
{
	u_int8_t	cmd[8 + length];
	int		len;

	((u_int16_t *) cmd)[0] = reserveId;
	((u_int16_t *) cmd)[1] = recordId;
	cmd[4] = offset;
	cmd[5] = length;
	if (ipmi_sendcmd(sc, BMC_SA, 0, STORAGE_NETFN, STORAGE_GET_SDR, 6,
	    cmd)) {
		printf("sendcmd fails\n");
		return (-1);
	}
	if (ipmi_recvcmd(sc, 8 + length, &len, cmd)) {
		printf("getSdrPartial: recvcmd fails\n");
		return (-1);
	}
	if (nxtRecordId) {
		*nxtRecordId = *(uint16_t *) cmd;
	}
	memcpy(buffer, cmd + 2, len - 2);

	return (0);
}

int maxsdrlen = 0x10;

/* Read an entire SDR; pass to add sensor */
int
get_sdr(struct ipmi_softc *sc, u_int16_t recid, u_int16_t *nxtrec)
{
	u_int16_t	resid;
	int		len, sdrlen, offset;
	u_int8_t	*psdr;
	sdrhdr_t	shdr;

	/* Reserve SDR */
	if (ipmi_sendcmd(sc, BMC_SA, 0, STORAGE_NETFN, STORAGE_RESERVE_SDR,
	    0, NULL)) {
		printf("reserve send fails\n");
		return (-1);
	}
	if (ipmi_recvcmd(sc, sizeof(resid), &len, &resid)) {
		printf("reserve recv fails\n");
		return (-1);
	}
	/* Get SDR Header */
	if (get_sdr_partial(sc, recid, resid, 0, sizeof(sdrhdr_t), &shdr,
	    nxtrec)) {
		printf("get header fails\n");
		return (-1);
	}
	/* Allocate space for entire SDR Length of SDR in header does not
	 * include header length */
	sdrlen = sizeof(shdr) + shdr.record_length;
	psdr = malloc(sdrlen, M_DEVBUF, M_WAITOK);
	if (psdr == NULL)
		return -1;

	memcpy(psdr, &shdr, sizeof(shdr));

	/* Read SDR Data maxsdrlen bytes at a time */
	for (offset = sizeof(shdr); offset < sdrlen; offset += maxsdrlen) {
		len = sdrlen - offset;
		if (len > maxsdrlen)
			len = maxsdrlen;

		if (get_sdr_partial(sc, recid, resid, offset, len,
		    psdr + offset, NULL)) {
			printf("get chunk : %d,%d fails\n", offset, len);
			return (-1);
		}
	}

	/* Add SDR to sensor list, if not wanted, free buffer */
	if (add_sdr_sensor(sc, psdr) == 0)
		free(psdr, M_DEVBUF);

	return (0);
}

int
getbits(u_int8_t *bytes, int bitpos, int bitlen)
{
	int	v;
	int	mask;

	bitpos += bitlen - 1;
	for (v = 0; bitlen--;) {
		v <<= 1;
		mask = 1L << (bitpos & 7);
		if (bytes[bitpos >> 3] & mask) {
			v |= 1;
		}
		bitpos--;
	}

	return (v);
}

/* Decode IPMI sensor name */
void
ipmi_sensor_name(char *name, int len, u_int8_t typelen, u_int8_t *bits)
{
	int	i, slen;
	char	bcdplus[] = "0123456789 -.:,_";

	slen = typelen & 0x1F;
	switch (typelen & 0xC0) {
	case 0x00:
		//unicode
		break;

	case 0x40:
		//bcdplus
		/* Characters are encoded in 4-bit BCDPLUS */
		for (i = 0; i < slen; i++) {
			*(name++) = bcdplus[bits[i] >> 4];
			*(name++) = bcdplus[bits[i] & 0xF];
		}
		break;

	case 0x80:
		//6 - bit ascii
		/* Characters are encoded in 6-bit ASCII 0x00 - 0x3F maps to
		 * 0x20 - 0x5F */
		for (i = 0; i < slen * 8; i += 6) {
			*(name++) = getbits(bits, i, 6) + ' ';
		}
		break;

	case 0xC0:
		//8 - bit ascii
		while (slen--)
			*(name++) = *(bits++);
		break;
	}
	*name = 0;
}

/* Calculate val * 10^exp */
long
ipow(long val, int exp)
{
	while (exp > 0) {
		val *= 10;
		exp--;
	}

	while (exp < 0) {
		val /= 10;
		exp++;
	}

	return (val);
}

/* Convert IPMI reading from sensor factors */
long
ipmi_convert(u_int8_t v, sdrtype1 *s1, long adj)
{
	short	M, B;
	char	K1, K2;
	long	val;

	/* M is 10-bit value: check if negative */
	M = (((short)(s1->m_tolerance & 0xC0)) << 2) + s1->m;
	if (M & 0x200)
		M |= 0xFC00;

	/* B is 10-bit value; check if negative */
	B = (((short)(s1->b_accuracy & 0xC0)) << 2) + s1->b;
	if (B & 0x200)
		B |= 0xFC00;

	/* K1/K2 are 4-bit values; check if negative */
	K1 = s1->rbexp & 0xF;
	if (K1 & 0x8)
		K1 |= 0xF0;

	K2 = s1->rbexp >> 4;
	if (K2 & 0x8)
		K2 |= 0xF0;

	/* Calculate sensor reading: y = L((M * v + (B * 10^K1)) *
	 * 10^(K2+adj));
	 *
	 * This commutes out to: y = L(M*v * 10^(K2+adj) + B * 10^(K1+K2+adj)); */
	val = ipow(M * v, K2 + adj) + ipow(B, K1 + K2 + adj);

	/* Linearization function: y = f(x) 0 : y = x 1 : y = ln(x) 2 : y =
	 * log10(x) 3 : y = log2(x) 4 : y = e^x 5 : y = 10^x 6 : y = 2^x 7 : y
	 * = 1/x 8 : y = x^2 9 : y = x^3 10 : y = square root(x) 11 : y = cube
	 * root(x) */
	return (val);
}

int
read_sensor(struct ipmi_softc *sc, struct ipmi_sensor *psensor)
{
	sdrtype1	*s1 = (sdrtype1 *) psensor->i_sdr;
	u_int8_t	data[8];
	int		rxlen;

	memset(data, 0, sizeof(data));
	data[0] = psensor->i_num;
	if (ipmi_sendcmd(sc, s1->owner_id, s1->owner_lun, SE_NETFN,
	    SE_GET_SENSOR_READING, 1, data))
		return (-1);

	if (ipmi_recvcmd(sc, sizeof(data), &rxlen, data))
		return (-1);

	psensor->i_sensor.flags &= ~SENSOR_FINVALID;
	if (data[1] & BIT(5)) {
		/* Check if sensor is valid */
		psensor->i_sensor.flags |= SENSOR_FINVALID;
	}
	psensor->i_sensor.status = SENSOR_S_OK;
	if (s1->sdrhdr.record_type == 2) {
		/* Direct reading */
		psensor->i_sensor.value = data[0];
		return (0);
	}

	dbg_printf(1, "sensor state: %.02x %.02x %.02x %s\n",
	    rxlen, data[2], data[3], psensor->i_sensor.desc);
	/* ..XX.XX. X......X */
	if (data[2] & 0x36)
		psensor->i_sensor.status = SENSOR_S_CRIT;
	else
		if (data[2] & 0x81)
			psensor->i_sensor.status = SENSOR_S_WARN;

	switch (psensor->i_sensor.type) {
	case SENSOR_TEMP:
		psensor->i_sensor.value = ipmi_convert(data[0], s1, 6);
		psensor->i_sensor.value += 273150000;
		break;

	case SENSOR_VOLTS_DC:
		psensor->i_sensor.value = ipmi_convert(data[0], s1, 6);
		break;

	default:
		psensor->i_sensor.value = ipmi_convert(data[0], s1, 0);
	}

	return (0);
}

int
valid_sensor(int type, int btype)
{
	switch (type << 8L | btype) {
	case 0x0101:
		return (SENSOR_TEMP);

	case 0x0201:
		return (SENSOR_VOLTS_DC);

	case 0x0401:
		return (SENSOR_FANRPM);

	case 0x056F:
		return (SENSOR_INDICATOR);
	}

	return (-1);
}

/* Add Sensor to BSD Sysctl interface */
int
add_sdr_sensor(struct ipmi_softc *sc, u_int8_t *psdr)
{
	struct ipmi_sensor	*psensor;
	int			typ, base, icnt, snum, idx;
	sdrtype1		*s1 = (sdrtype1 *) psdr;
	sdrtype2		*s2 = (sdrtype2 *) psdr;
	char			name[64];

	switch (s1->sdrhdr.record_type) {
	case 1:
		ipmi_sensor_name(name, sizeof(name), s1->typelen, s1->name);
		icnt = 1;
		snum = s1->sensor_num;
		typ = valid_sensor(s1->sensor_type, s1->event_code);
		if (typ == -1) {
			dbg_printf(1, "Unknown sensor type1: st:%.02x "
			    "et:%.02x sn:%.02x %s\n",
			    s1->sensor_type, s1->event_code, snum, name);
			return (0);
		}
		break;

	case 2:
		ipmi_sensor_name(name, sizeof(name), s2->typelen, s2->name);
		base = s2->share2 & 0x7F;
		icnt = s2->share1 & 0x0F;
		snum = s2->sensor_num;
		typ = valid_sensor(s2->sensor_type, s2->event_code);
		if (typ == -1) {
			dbg_printf(1, "Unknown sensor type2: st:%.02x "
			    "et:%.02x sn:%.02x %s\n",
			    s2->sensor_type, s2->event_code, snum, name);

			return (0);
		}
		break;

	default:
		return (0);
	}

	for (idx = 0; idx < icnt; idx++) {

		psensor = malloc(sizeof(struct ipmi_sensor), M_DEVBUF,
		    M_WAITOK);

		/* XXX get rid of this */
		if (psensor == NULL)
			break;

		memset(psensor, 0, sizeof(struct ipmi_sensor));

		/* Initialize BSD Sensor info */
		psensor->i_sdr = psdr;
		psensor->i_num = snum + idx;
		psensor->i_sensor.status = SENSOR_S_OK;
		psensor->i_sensor.type = typ;
		strlcpy(psensor->i_sensor.device, sc->sc_dev.dv_xname,
		    sizeof(psensor->i_sensor.device));
		if (icnt > 1)
			snprintf(psensor->i_sensor.desc,
			    sizeof(psensor->i_sensor.desc),
			    "%s - %d", name, base + idx);
		else
			strlcpy(psensor->i_sensor.desc, name,
			    sizeof(psensor->i_sensor.desc));

		dbg_printf(1, "add sensor:%.4x %.2x:%d ent:%.2x:%.2x %s\n",
		    s1->sdrhdr.record_id, s1->sensor_type,
		    typ, s1->entity_id, s1->entity_instance,
		    psensor->i_sensor.desc);
		if (read_sensor(sc, psensor) == 0) {
			SLIST_INSERT_HEAD(&ipmi_sensor_list, psensor, list);
			SENSOR_ADD(&psensor->i_sensor);
			dbg_printf(1, "  reading: %lld [%s]\n",
			    psensor->i_sensor.value,
			    psensor->i_sensor.desc);
		}
	}

	return (1);
}

/* Interrupt handler */
int
ipmi_intr(void *arg)
{
	struct ipmi_softc	*sc = (struct ipmi_softc *)arg;
	int			v;

	v = bmc_read(sc, _KCS_STATUS_REGISTER);
	if (v & KCS_OBF)
		++ipmi_nintr;

	return (0);
}

/* Handle IPMI Timer - reread sensor values */
void
ipmi_refresh_sensors(struct ipmi_softc *sc)
{
	struct	ipmi_sensor *psensor = NULL;

	SLIST_FOREACH(psensor, &ipmi_sensor_list, list) {
		if (read_sensor(sc, psensor))
			printf("error reading: %s\n", psensor->i_sensor.desc);

	}
}

void
ipmi_refresh(void *arg)
{
	struct	ipmi_softc *sc = (struct ipmi_softc *)arg;

	ipmi_refresh_sensors(sc);
	timeout_add(&ipmi_timeout, SENSOR_REFRESH_RATE);
}

int
ipmi_map_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia)
{
	sc->sc_if = ipmi_get_if(ia->iaa_if_type);
	if (sc->sc_if == NULL)
		return (-1);

	if (ia->iaa_if_iotype == 'i')
		sc->sc_iot = ia->iaa_iot;
	else
		sc->sc_iot = ia->iaa_memt;

	sc->sc_if_rev = ia->iaa_if_rev;
	sc->sc_if_iospacing = ia->iaa_if_iospacing;
	bus_space_map(sc->sc_iot, ia->iaa_if_iobase,
	    sc->sc_if->nregs * sc->sc_if_iospacing,
	    0, &sc->sc_ioh);
#if 0
	if (iaa->if_if_irq != -1) {
		sc->ih = isa_intr_establish(-1, iaa->if_if_irq,
		    iaa->if_irqlvl, IPL_BIO,
		    ipmi_intr, sc
		    sc->sc_dev.dv_xname);
	}
#endif
	return (0);
}

void
ipmi_unmap_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia)
{
	bus_space_unmap(sc->sc_iot, sc->sc_ioh,
	    sc->sc_if->nregs * sc->sc_if_iospacing);
}

int
ipmi_probe(struct device *parent, void *match, void *aux)
{
	struct ipmi_softc	sc;
	struct ipmi_attach_args	*ia = aux;
	struct cfdata		*cf = match;
	int			rc;

	if (strcmp(ia->iaa_name, cf->cf_driver->cd_name))
		return (0);

	if (scan_smbios(SMBIOS_TYPE_IPMI, smbios_ipmi_probe, ia) == 0) {
		dmd_ipmi_t *pipmi;

		pipmi = (dmd_ipmi_t *)scan_sig(0xC0000L, 0xFFFFFL, 16, 4,
		    "IPMI");
		if (pipmi == NULL)
			return (0);

		ia->iaa_if_type = pipmi->dmd_if_type;
		ia->iaa_if_rev = pipmi->dmd_if_rev;
	}
	/* Map registers */
	if (ipmi_map_regs(&sc, ia) != 0)
		return (0);

	rc = sc.sc_if->probe(&sc);
	ipmi_unmap_regs(&sc, ia);

	return (!rc);
}

void
ipmi_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipmi_softc	*sc = (void *) self;
	struct ipmi_attach_args	*ia = aux;
	u_int8_t		cmd[32];
	int			len;
	u_int16_t		rec;

	/* Map registers */
	ipmi_map_regs(sc, ia);

	/* Identify BMC device */
	ipmi_sendcmd(sc, BMC_SA, 0, APP_NETFN, APP_GET_DEVICE_ID, 0, NULL);
	ipmi_recvcmd(sc, sizeof(cmd), &len, cmd);
	dbg_dump(1, "bmc data", len, cmd);

	/* Scan SDRs, add sensors */
	for (rec = 0; rec != 0xFFFF;)
		if (get_sdr(sc, rec, &rec))
			break;

	/* Setup timeout */
	timeout_set(&ipmi_timeout, ipmi_refresh, sc);
	timeout_add(&ipmi_timeout, SENSOR_REFRESH_RATE);

	printf(": version %d.%d interface %s %cbase 0x%x/%x spacing %d irq %d\n",
	    ia->iaa_if_rev >> 4, ia->iaa_if_rev & 0xF,
	    sc->sc_if->name, ia->iaa_if_iotype, ia->iaa_if_iobase,
	    ia->iaa_if_iospacing * sc->sc_if->nregs, ia->iaa_if_iospacing,
	    ia->iaa_if_irq);
}
