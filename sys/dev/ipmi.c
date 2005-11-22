/*	$OpenBSD: ipmi.c,v 1.19 2005/11/22 04:28:14 marco Exp $	*/

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
#include <sys/kthread.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ipmivar.h>

#include <uvm/uvm_extern.h>

struct ipmi_sensor {
	u_int8_t	*i_sdr;
	int		i_num;
	int		stype;
	int		etype;
	struct		sensor i_sensor;
	SLIST_ENTRY(ipmi_sensor) list;
};

int	ipmi_nintr;
int	ipmi_dbg = 0;
int	ipmi_poll = 1;

#define SENSOR_REFRESH_RATE (10 * hz)

#define SMBIOS_TYPE_IPMI	0x26

#define DEVNAME(s)  ((s)->sc_dev.dv_xname)

/*
 * Format of SMBIOS IPMI Flags
 *
 * bit0: interrupt trigger mode (1=level, 0=edge)
 * bit1: interrupt polarity (1=active high, 0=active low)
 * bit2: reserved
 * bit3: address LSB (1=odd,0=even)
 * bit4: interrupt (1=specified, 0=not specified)
 * bit5: reserved
 * bit6/7: register spacing (1,4,2,err)
 */
#define SMIPMI_FLAG_IRQLVL		(1L << 0)
#define SMIPMI_FLAG_IRQEN		(1L << 3)
#define SMIPMI_FLAG_ODDOFFSET		(1L << 4)
#define SMIPMI_FLAG_IFSPACING(x)	(((x)>>6)&0x3)
#define	 IPMI_IOSPACING_BYTE		 0
#define	 IPMI_IOSPACING_WORD		 2
#define	 IPMI_IOSPACING_DWORD		 1

#define IPMI_BTMSG_LEN			0
#define IPMI_BTMSG_NFLN			1
#define IPMI_BTMSG_SEQ			2
#define IPMI_BTMSG_CMD			3
#define IPMI_BTMSG_CCODE		4
#define IPMI_BTMSG_DATASND		4
#define IPMI_BTMSG_DATARCV		5

#define IPMI_MSG_NFLN			0
#define IPMI_MSG_CMD			1
#define IPMI_MSG_CCODE			2
#define IPMI_MSG_DATASND		2
#define IPMI_MSG_DATARCV		3

#define IPMI_SENSOR_TYPE_TEMP		0x0101
#define IPMI_SENSOR_TYPE_VOLT		0x0102
#define IPMI_SENSOR_TYPE_FAN		0x0104
#define IPMI_SENSOR_TYPE_INTRUSION	0x6F05
#define IPMI_SENSOR_TYPE_PWRSUPPLY	0x6F08

#define IPMI_NAME_UNICODE		0x00
#define IPMI_NAME_BCDPLUS		0x01
#define IPMI_NAME_ASCII6BIT		0x02
#define IPMI_NAME_ASCII8BIT		0x03

#define IPMI_ENTITY_PWRSUPPLY		0x0A

#define IPMI_INVALID_SENSOR		(1L << 5)

#define IPMI_SDR_TYPEFULL		1
#define IPMI_SDR_TYPECOMPACT		2

#define byteof(x) ((x) >> 3)
#define bitof(x)  (1L << ((x) & 0x7))
#define TB(b,m)	  (data[2+byteof(b)] & bitof(b))

#define dbg_printf(lvl, fmt...) \
	if (ipmi_dbg >= lvl) \
		printf(fmt);
#define dbg_dump(lvl, msg, len, buf) \
	if (len && ipmi_dbg >= lvl) \
		dumpb(msg, len, (const u_int8_t *)(buf));

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
int	ipmi_match(struct device *, void *, void *);
void	ipmi_attach(struct device *, struct device *, void *);

long	ipow(long, int);
long	ipmi_convert(u_int8_t, sdrtype1 *, long);
void	ipmi_sensor_name(char *, int, u_int8_t, u_int8_t *);

/* BMC Helper Functions */
u_int8_t bmc_read(struct ipmi_softc *, int);
void	bmc_write(struct ipmi_softc *, int, u_int8_t);
int	bmc_io_wait(struct ipmi_softc *, int, u_int8_t, u_int8_t, long,
	    const char *);

void	*bt_buildmsg(struct ipmi_softc *, int, int, int, const void *, int *);
void	*cmn_buildmsg(struct ipmi_softc *, int, int, int, const void *, int *);

int	getbits(u_int8_t *, int, int);
int	ipmi_sensor_type(int, int, int);

void	ipmi_refresh_sensors(struct ipmi_softc *sc);
int	ipmi_map_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia);
void	ipmi_unmap_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia);

struct smbios_mem_map {
	vaddr_t		baseva;
	u_int8_t	*va;
	size_t		vsize;
	paddr_t		pa;
};

void	*smbios_map(paddr_t, size_t, struct smbios_mem_map *);
void	smbios_unmap(struct smbios_mem_map *);
void	*scan_sig(long, long, int, int, const void *);
int	scan_smbios(u_int8_t, void (*)(void *, void *), void *);

int	ipmi_test_threshold(u_int8_t, u_int8_t, u_int8_t, u_int8_t);
int	ipmi_sensor_status(struct ipmi_softc *, struct ipmi_sensor *,
    u_int8_t *);

int	 add_child_sensors(struct ipmi_softc *, u_int8_t *, int, int, int,
    int, int, int, const char *);

struct ipmi_if kcs_if = {
	"KCS",
	IPMI_IF_KCS_NREGS,
	cmn_buildmsg,
	kcs_sendmsg,
	kcs_recvmsg,
	kcs_reset,
	kcs_probe,
};

struct ipmi_if smic_if = {
	"SMIC",
	IPMI_IF_SMIC_NREGS,
	cmn_buildmsg,
	smic_sendmsg,
	smic_recvmsg,
	smic_reset,
	smic_probe,
};

struct ipmi_if bt_if = {
	"BT",
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
u_int8_t
bmc_read(struct ipmi_softc *sc, int offset)
{
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh,
	    offset * sc->sc_if_iospacing));
}

void
bmc_write(struct ipmi_softc *sc, int offset, u_int8_t val)
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
		if ((v & mask) == value)
			return v;
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
#define	  BT_CLR_WR_PTR			(1L << 0)
#define	  BT_CLR_RD_PTR			(1L << 1)
#define	  BT_HOST2BMC_ATN		(1L << 2)
#define	  BT_BMC2HOST_ATN		(1L << 3)
#define	  BT_EVT_ATN			(1L << 4)
#define	  BT_HOST_BUSY			(1L << 6)
#define	  BT_BMC_BUSY			(1L << 7)

#define	  BT_READY	(BT_HOST_BUSY|BT_HOST2BMC_ATN|BT_BMC2HOST_ATN)

#define _BT_DATAIN_REG			1
#define _BT_DATAOUT_REG			1

#define _BT_INTMASK_REG			2
#define	 BT_IM_HIRQ_PEND		(1L << 1)
#define	 BT_IM_SCI_EN			(1L << 2)
#define	 BT_IM_SMI_EN			(1L << 3)
#define	 BT_IM_NMI2SMI			(1L << 4)

int
bt_sendmsg(struct ipmi_softc *sc, int len, const u_int8_t *data)
{
	int i;

	if (bmc_io_wait(sc, _BT_CTRL_REG, BT_READY, 0, 0xFFFFF, "btsend") < 0)
		return -1;

	bmc_write(sc, _BT_CTRL_REG, BT_CLR_WR_PTR);
	for (i = 0; i < len; i++)
		bmc_write(sc, _BT_DATAOUT_REG, data[i]);

	bmc_write(sc, _BT_CTRL_REG, BT_HOST2BMC_ATN);

	if (bmc_io_wait(sc, _BT_CTRL_REG, BT_BMC2HOST_ATN, BT_BMC2HOST_ATN,
		0xFFFFF, "btswait") < 0)
		return -1;

	return (0);
}

int
bt_recvmsg(struct ipmi_softc *sc, int maxlen, int *rxlen, u_int8_t *data)
{
	u_int8_t len, v, i;

	/* BT Result data: 0: len   1:nfln   2:seq   3:cmd   4:ccode
	 * 5:data... */
	bmc_write(sc, _BT_CTRL_REG, BT_HOST_BUSY|BT_CLR_RD_PTR);
	len = bmc_read(sc, _BT_DATAIN_REG);
	for (i = IPMI_BTMSG_NFLN; i <= len; i++) {
		/* Ignore sequence number */
		v = bmc_read(sc, _BT_DATAIN_REG);
		if (i != IPMI_BTMSG_SEQ)
			*(data++) = v;
	}
	bmc_write(sc, _BT_CTRL_REG, BT_BMC2HOST_ATN | BT_HOST_BUSY);
	*rxlen = len - 1;

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
	u_int8_t rv;

	rv = bmc_read(sc, _BT_CTRL_REG);
	rv &= BT_HOST_BUSY;
	rv |= BT_CLR_WR_PTR|BT_CLR_RD_PTR|BT_BMC2HOST_ATN|BT_HOST2BMC_ATN;
	bmc_write(sc, _BT_CTRL_REG, rv);

	rv = bmc_read(sc, _BT_INTMASK_REG);
	rv &= BT_IM_SCI_EN|BT_IM_SMI_EN|BT_IM_NMI2SMI;
	rv |= BT_IM_HIRQ_PEND;
	bmc_write(sc, _BT_INTMASK_REG, rv);

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
	return (0);
}

/*
 * SMIC interface
 */
#define _SMIC_DATAIN_REG		0
#define _SMIC_DATAOUT_REG		0

#define _SMIC_CTRL_REG			1
#define	  SMS_CC_GET_STATUS		 0x40
#define	  SMS_CC_START_TRANSFER		 0x41
#define	  SMS_CC_NEXT_TRANSFER		 0x42
#define	  SMS_CC_END_TRANSFER		 0x43
#define	  SMS_CC_START_RECEIVE		 0x44
#define	  SMS_CC_NEXT_RECEIVE		 0x45
#define	  SMS_CC_END_RECEIVE		 0x46
#define	  SMS_CC_TRANSFER_ABORT		 0x47

#define	  SMS_SC_READY			 0xc0
#define	  SMS_SC_WRITE_START		 0xc1
#define	  SMS_SC_WRITE_NEXT		 0xc2
#define	  SMS_SC_WRITE_END		 0xc3
#define	  SMS_SC_READ_START		 0xc4
#define	  SMS_SC_READ_NEXT		 0xc5
#define	  SMS_SC_READ_END		 0xc6

#define _SMIC_FLAG_REG			2
#define	  SMIC_BUSY			(1L << 0)
#define	  SMIC_SMS_ATN			(1L << 2)
#define	  SMIC_EVT_ATN			(1L << 3)
#define	  SMIC_SMI			(1L << 4)
#define	  SMIC_TX_DATA_RDY		(1L << 6)
#define	  SMIC_RX_DATA_RDY		(1L << 7)

int	smic_wait(struct ipmi_softc *, u_int8_t, u_int8_t, const char *);
int	smic_write_cmd_data(struct ipmi_softc *, u_int8_t, const u_int8_t *);
int	smic_read_data(struct ipmi_softc *, u_int8_t *);

int
smic_wait(struct ipmi_softc *sc, u_int8_t mask, u_int8_t val, const char *lbl)
{
	int v;

	/* Wait for expected flag bits */
	v = bmc_io_wait(sc, _SMIC_FLAG_REG, mask, val, 0xFFFFF, "smicwait");
	if (v < 0)
		return (-1);

	/* Return current status */
	v = bmc_read(sc, _SMIC_CTRL_REG);
	dbg_printf(99, "smic_wait = %.2x\n", v);
	return (v);
}

int
smic_write_cmd_data(struct ipmi_softc *sc, u_int8_t cmd, const u_int8_t *data)
{
	int	sts, v;

	dbg_printf(50, "smic_wcd: %.2x %.2x\n", cmd, data ? *data : -1);
	sts = smic_wait(sc, SMIC_TX_DATA_RDY | SMIC_BUSY, SMIC_TX_DATA_RDY,
	    "smic_write_cmd_data ready");
	if (sts < 0)
		return (sts);

	bmc_write(sc, _SMIC_CTRL_REG, cmd);
	if (data)
		bmc_write(sc, _SMIC_DATAOUT_REG, *data);

	/* Toggle BUSY bit, then wait for busy bit to clear */
	v = bmc_read(sc, _SMIC_FLAG_REG);
	bmc_write(sc, _SMIC_FLAG_REG, v | SMIC_BUSY);

	return (smic_wait(sc, SMIC_BUSY, 0, "smic_write_cmd_data busy"));
}

int
smic_read_data(struct ipmi_softc *sc, u_int8_t *data)
{
	int sts;

	sts = smic_wait(sc, SMIC_RX_DATA_RDY | SMIC_BUSY, SMIC_RX_DATA_RDY,
	    "smic_read_data");
	if (sts >= 0) {
		*data = bmc_read(sc, _SMIC_DATAIN_REG);
		dbg_printf(50, "smic_readdata: %.2x\n", *data);
	}
	return (sts);
}

#define ErrStat(a,b) if (a) printf(b);

int
smic_sendmsg(struct ipmi_softc *sc, int len, const u_int8_t *data)
{
	int sts, idx;

	sts = smic_write_cmd_data(sc, SMS_CC_START_TRANSFER, &data[0]);
	ErrStat(sts != SMS_SC_WRITE_START, "wstart");
	for (idx = 1; idx < len - 1; idx++) {
		sts = smic_write_cmd_data(sc, SMS_CC_NEXT_TRANSFER,
		    &data[idx]);
		ErrStat(sts != SMS_SC_WRITE_NEXT, "write");
	}
	sts = smic_write_cmd_data(sc, SMS_CC_END_TRANSFER, &data[idx]);
	if (sts != SMS_SC_WRITE_END) {
		dbg_printf(50, "smic_sendmsg %d/%d = %.2x\n", idx, len, sts);
		return (-1);
	}

	return (0);
}

int
smic_recvmsg(struct ipmi_softc *sc, int maxlen, int *len, u_int8_t *data)
{
	int sts, idx;

	*len = 0;
	sts = smic_wait(sc, SMIC_RX_DATA_RDY, SMIC_RX_DATA_RDY, "smic_recvmsg");
	if (sts < 0)
		return (-1);

	sts = smic_write_cmd_data(sc, SMS_CC_START_RECEIVE, NULL);
	ErrStat(sts != SMS_SC_READ_START, "rstart");
	for (idx = 0;; ) {
		sts = smic_read_data(sc, &data[idx++]);
		if (sts != SMS_SC_READ_START && sts != SMS_SC_READ_NEXT)
			break;
		smic_write_cmd_data(sc, SMS_CC_NEXT_RECEIVE, NULL);
	}
	ErrStat(sts != SMS_SC_READ_END, "rend");

	*len = idx;

	sts = smic_write_cmd_data(sc, SMS_CC_END_RECEIVE, NULL);
	if (sts != SMS_SC_READY) {
		dbg_printf(50, "smic_recvmsg %d/%d = %.2x\n", idx, maxlen, sts);
		return (-1);
	}

	return (0);
}

int
smic_reset(struct ipmi_softc *sc)
{
	return (-1);
}

int
smic_probe(struct ipmi_softc *sc)
{
	/* Flag register should not be 0xFF on a good system */
	if (bmc_read(sc, _SMIC_FLAG_REG) == 0xFF)
		return (-1);

	return (0);
}

/*
 * KCS interface
 */
#define _KCS_DATAIN_REGISTER		0
#define _KCS_DATAOUT_REGISTER		0
#define	  KCS_READ_NEXT			0x68

#define _KCS_COMMAND_REGISTER		1
#define	  KCS_GET_STATUS		0x60
#define	  KCS_WRITE_START		0x61
#define	  KCS_WRITE_END			0x62

#define _KCS_STATUS_REGISTER		1
#define	  KCS_OBF			(1L << 0)
#define	  KCS_IBF			(1L << 1)
#define	  KCS_SMS_ATN			(1L << 2)
#define	  KCS_CD			(1L << 3)
#define	  KCS_OEM1			(1L << 4)
#define	  KCS_OEM2			(1L << 5)
#define	  KCS_STATE_MASK		0xc0
#define	    KCS_IDLE_STATE		0x00
#define	    KCS_READ_STATE		0x40
#define	    KCS_WRITE_STATE		0x80
#define	    KCS_ERROR_STATE		0xC0

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

	/* Check if output buffer full, read dummy byte	 */
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
		dbg_printf(1, "kcs sendmsg = %d/%d <%.2x>\n", idx, len, sts);
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
		dbg_printf(1, "kcs read = %d/%d <%.2x>\n", idx, maxlen, sts);

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
	sizeof(struct ipmi_softc), ipmi_match, ipmi_attach
};

struct cfdriver ipmi_cd = {
	NULL, "ipmi", DV_DULL
};

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

void *
smbios_map(paddr_t pa, size_t len, struct smbios_mem_map *handle)
{
	paddr_t pgstart = trunc_page(pa);
	paddr_t pgend	= round_page(pa + len);
	vaddr_t va = uvm_km_valloc(kernel_map, pgend-pgstart);

	if (va == 0)
		return NULL;

	handle->pa = pa;
	handle->baseva = va;
	handle->va = (u_int8_t *)(va + (pa & PGOFSET));
	handle->vsize = pgend - pgstart;

	do {
		pmap_kenter_pa(va, pgstart, VM_PROT_READ);
		va += NBPG;
		pgstart += NBPG;
	} while (pgstart < pgend);

	return handle->va;
}

void
smbios_unmap(struct smbios_mem_map *handle)
{
	pmap_kremove(handle->baseva, handle->vsize);
	uvm_km_free(kernel_map, handle->baseva, handle->vsize);
}

/* Scan SMBIOS for table type */
int
scan_smbios(u_int8_t mtype, void (*smcb) (void *base, void *arg), void *arg)
{
	smbiosanchor_t		*romhdr;
	smhdr_t			*smhdr;
	u_int8_t		*offset;
	int			nmatch, num;
	struct smbios_mem_map	smm;

	/* Scan for SMBIOS Table Signature */
	romhdr = (smbiosanchor_t *) scan_sig(0xF0000, 0xFFFFF, 16, 4, "_SM_");
	if (romhdr == NULL)
		return (-1);

	dbg_printf(99, "SMBIOS Version %d.%d at 0x%lx, %d entries\n",
	    romhdr->smr_smbios_majver, romhdr->smr_smbios_minver,
	    romhdr->smr_table_address, romhdr->smr_count);

	/* Map SMBIOS Table start address */
	nmatch = 0;
	offset = smbios_map(romhdr->smr_table_address,
	    romhdr->smr_count * romhdr->smr_maxsize, &smm);
	if (offset == NULL)
		return (-1);

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
	smbios_unmap(&smm);

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
	ia->iaa_if_irq = (pipmi->smipmi_base_flags & SMIPMI_FLAG_IRQEN) ?
	    pipmi->smipmi_irq : -1;
	ia->iaa_if_irqlvl = (pipmi->smipmi_base_flags & SMIPMI_FLAG_IRQLVL) ?
	    IST_LEVEL : IST_EDGE;

	switch (SMIPMI_FLAG_IFSPACING(pipmi->smipmi_base_flags)) {
	case IPMI_IOSPACING_BYTE:
		ia->iaa_if_iospacing = 1;
		break;

	case IPMI_IOSPACING_DWORD:
		ia->iaa_if_iospacing = 4;
		break;

	case IPMI_IOSPACING_WORD:
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
	if (pipmi->smipmi_base_flags & SMIPMI_FLAG_ODDOFFSET)
		ia->iaa_if_iobase++;
}

/*
 * bt_buildmsg builds an IPMI message from a nfLun, cmd, and data
 * This is used by BT protocol
 *
 * Returns a buffer to an allocated message, txlen contains length
 *   of allocated message
 */
void *
bt_buildmsg(struct ipmi_softc *sc, int nfLun, int cmd, int len,
    const void *data, int *txlen)
{
	u_int8_t *buf;

	/* Block transfer needs 4 extra bytes: length/netfn/seq/cmd + data */
	*txlen = len + 4;
	buf = malloc(*txlen, M_DEVBUF, M_WAITOK);
	if (buf == NULL)
		return (NULL);

	buf[IPMI_BTMSG_LEN] = len + 3;
	buf[IPMI_BTMSG_NFLN] = nfLun;
	buf[IPMI_BTMSG_SEQ] = sc->sc_btseq++;
	buf[IPMI_BTMSG_CMD] = cmd;
	if (len && data)
		memcpy(buf + IPMI_BTMSG_DATASND, data, len);

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
		memcpy(buf + IPMI_MSG_DATASND, data, len);

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
		if (txlen)
			memcpy(imbreq->data, data, txlen);
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

	*rxlen = rawlen - IPMI_MSG_DATARCV;
	if (*rxlen > 0 && data)
		memcpy(data, buf + IPMI_MSG_DATARCV, *rxlen);

	if ((rc = buf[IPMI_MSG_CCODE]) != 0)
		dbg_printf(1, "ipmi_recvmsg: nfln=%.2x cmd=%.2x err=%.2x\n",
		    buf[IPMI_MSG_NFLN], buf[IPMI_MSG_CMD], buf[IPMI_MSG_CCODE]);
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
	if (nxtRecordId)
		*nxtRecordId = *(uint16_t *) cmd;
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
		if (bytes[bitpos >> 3] & mask)
			v |= 1;
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
	switch (typelen >> 6) {
	case IPMI_NAME_UNICODE:
		//unicode
		break;

	case IPMI_NAME_BCDPLUS:
		/* Characters are encoded in 4-bit BCDPLUS */
		if (len < slen * 2 + 1)
			slen = (len >> 1) - 1;
		for (i = 0; i < slen; i++) {
			*(name++) = bcdplus[bits[i] >> 4];
			*(name++) = bcdplus[bits[i] & 0xF];
		}
		break;

	case IPMI_NAME_ASCII6BIT:
		/* Characters are encoded in 6-bit ASCII
		 *   0x00 - 0x3F maps to 0x20 - 0x5F */
		/* XXX: need to calculate max len: slen = 3/4 * len */
		if (len < slen + 1)
			slen = len - 1;
		for (i = 0; i < slen * 8; i += 6)
			*(name++) = getbits(bits, i, 6) + ' ';
		break;

	case IPMI_NAME_ASCII8BIT:
		/* Characters are 8-bit ascii */
		if (len < slen + 1)
			slen = len - 1;
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

	/* Calculate sensor reading:
	 *  y = L((M * v + (B * 10^K1)) * 10^(K2+adj)
	 *
	 * This commutes out to:
	 *  y = L(M*v * 10^(K2+adj) + B * 10^(K1+K2+adj)); */
	val = ipow(M * v, K2 + adj) + ipow(B, K1 + K2 + adj);

	/* Linearization function: y = f(x) 0 : y = x 1 : y = ln(x) 2 : y =
	 * log10(x) 3 : y = log2(x) 4 : y = e^x 5 : y = 10^x 6 : y = 2^x 7 : y
	 * = 1/x 8 : y = x^2 9 : y = x^3 10 : y = square root(x) 11 : y = cube
	 * root(x) */
	return (val);
}

int
ipmi_test_threshold(u_int8_t v, u_int8_t valid, u_int8_t hi, u_int8_t lo)
{
	dbg_printf(10, "thresh: %.2x %.2x %.2x %d\n", v, lo, hi,valid);
	return ((valid & 1 && lo != 0x00 && v <= lo) ||
	    (valid & 8 && hi != 0xFF && v >= hi));
}

int
ipmi_sensor_status(struct ipmi_softc *sc, struct ipmi_sensor *psensor,
    u_int8_t *reading)
{
	u_int8_t		data[32];
	sdrtype1	*s1 = (sdrtype1 *)psensor->i_sdr;
	int		rxlen, etype;

	psensor->i_sensor.status = SENSOR_S_OK;

	/* Get reading of sensor */
	switch (psensor->i_sensor.type) {
	case SENSOR_TEMP:
		psensor->i_sensor.value = ipmi_convert(reading[0], s1, 6);
		psensor->i_sensor.value += 273150000;
		break;

	case SENSOR_VOLTS_DC:
		psensor->i_sensor.value = ipmi_convert(reading[0], s1, 6);
		break;

	case SENSOR_FANRPM:
		psensor->i_sensor.value = ipmi_convert(reading[0], s1, 0);
		break;
	default:
		break;
	}

	/* Return Sensor Status */
	etype = (psensor->etype << 8) + psensor->stype;
	switch (etype) {
	case IPMI_SENSOR_TYPE_TEMP:
	case IPMI_SENSOR_TYPE_VOLT:
	case IPMI_SENSOR_TYPE_FAN:
		data[0] = psensor->i_num;
		ipmi_sendcmd(sc, s1->owner_id, s1->owner_lun,
		    SE_NETFN, SE_GET_SENSOR_THRESHOLD, 1, data);
		ipmi_recvcmd(sc, sizeof(data), &rxlen, data);

		dbg_printf(10, "recvdata: %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
		    data[0], data[1], data[2], data[3], data[4], data[5],
		    data[6]);

		if (ipmi_test_threshold(*reading, data[0] >> 2 ,
		    data[6], data[3]))
			return (SENSOR_S_CRIT);

		if (ipmi_test_threshold(*reading, data[0] >> 1,
		    data[5], data[2]))
			return (SENSOR_S_CRIT);

		if (ipmi_test_threshold(*reading, data[0] ,
		    data[4], data[1]))
			return (SENSOR_S_WARN);

		break;

	case IPMI_SENSOR_TYPE_INTRUSION:
		psensor->i_sensor.value = (reading[2] & 1) ? 1 : 0;
		if (reading[2] & 0x1)
			return (SENSOR_S_CRIT);
		break;

	case IPMI_SENSOR_TYPE_PWRSUPPLY:
		/* Reading: 1 = present+powered, 0 = otherwise */
		psensor->i_sensor.value = (reading[2] & 1) ? 1 : 0;
		if (reading[2] & 0x10) {
			/* XXX: Need sysctl type for Power Supply types
			 *   ok: power supply installed && powered
			 * warn: power supply installed && !powered
			 * crit: power supply !installed
			 */
			return (SENSOR_S_CRIT);
		}
		if (reading[2] & 0x08) {
			/* Power supply AC lost */
			return (SENSOR_S_WARN);
		}
		break;
	}

	return (SENSOR_S_OK);
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

	dbg_printf(1, "values=%.2x %.2x %.2x %.2x %s\n",
	    data[0],data[1],data[2],data[3], psensor->i_sensor.desc);
	psensor->i_sensor.flags &= ~SENSOR_FINVALID;
	if (data[1] & IPMI_INVALID_SENSOR) {
		/* Check if sensor is valid */
		psensor->i_sensor.flags |= SENSOR_FINVALID;
	}
	psensor->i_sensor.status = ipmi_sensor_status(sc, psensor, data);

	return (0);
}

int
ipmi_sensor_type(int type, int ext_type, int entity)
{
	switch (ext_type << 8L | type) {
	case IPMI_SENSOR_TYPE_TEMP:
		return (SENSOR_TEMP);

	case IPMI_SENSOR_TYPE_VOLT:
		return (SENSOR_VOLTS_DC);

	case IPMI_SENSOR_TYPE_FAN:
		return (SENSOR_FANRPM);

	case IPMI_SENSOR_TYPE_PWRSUPPLY:
		if (entity == IPMI_ENTITY_PWRSUPPLY)
			return (SENSOR_INDICATOR);
		break;

	case IPMI_SENSOR_TYPE_INTRUSION:
		return (SENSOR_INDICATOR);
	}

	return (-1);
}

/* Add Sensor to BSD Sysctl interface */
int
add_sdr_sensor(struct ipmi_softc *sc, u_int8_t *psdr)
{
	int			rc;
	sdrtype1		*s1 = (sdrtype1 *) psdr;
	sdrtype2		*s2 = (sdrtype2 *) psdr;
	char			name[64];

	switch (s1->sdrhdr.record_type) {
	case IPMI_SDR_TYPEFULL:
		ipmi_sensor_name(name, sizeof(name), s1->typelen, s1->name);
		rc = add_child_sensors(sc, psdr, 1, s1->sensor_num,
		    s1->sensor_type, s1->event_code, 0, s1->entity_id, name);
		break;

	case IPMI_SDR_TYPECOMPACT:
		ipmi_sensor_name(name, sizeof(name), s2->typelen, s2->name);
		rc = add_child_sensors(sc, psdr, s2->share1 & 0xF,
		    s2->sensor_num, s2->sensor_type, s2->event_code,
		    s2->share2 & 0x7F, s2->entity_id, name);
		break;

	default:
		return (0);
	}

	return rc;
}

int
add_child_sensors(struct ipmi_softc *sc, u_int8_t *psdr, int count,
    int sensor_num, int sensor_type, int ext_type, int sensor_base,
    int entity, const char *name)
{
	int typ, idx;
	struct ipmi_sensor *psensor;
	sdrtype1 *s1 = (sdrtype1 *) psdr;

	typ = ipmi_sensor_type(sensor_type, ext_type, entity);
	if (typ == -1) {
		dbg_printf(1, "Unknown sensor type:%.2x et:%.2x sn:%.2x "
		    "name:%s\n", sensor_type, ext_type, sensor_num, name);
		return 0;
	}
	for (idx = 0; idx < count; idx++) {
		psensor = malloc(sizeof(struct ipmi_sensor), M_DEVBUF,
		    M_WAITOK);

		/* XXX get rid of this */
		if (psensor == NULL)
			break;

		memset(psensor, 0, sizeof(struct ipmi_sensor));

		/* Initialize BSD Sensor info */
		psensor->i_sdr = psdr;
		psensor->i_num = sensor_num + idx;
		psensor->stype = sensor_type;
		psensor->etype = ext_type;
		psensor->i_sensor.status = SENSOR_S_OK;
		psensor->i_sensor.type = typ;
		strlcpy(psensor->i_sensor.device, DEVNAME(sc),
		    sizeof(psensor->i_sensor.device));
		if (count > 1)
			snprintf(psensor->i_sensor.desc,
			    sizeof(psensor->i_sensor.desc),
			    "%s - %d", name, sensor_base + idx);
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
			dbg_printf(1, "	 reading: %lld [%s]\n",
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

	if (!ipmi_poll)
		return;

	SLIST_FOREACH(psensor, &ipmi_sensor_list, list)
		if (read_sensor(sc, psensor))
			printf("error reading: %s\n", psensor->i_sensor.desc);
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
	if (bus_space_map(sc->sc_iot, ia->iaa_if_iobase,
	    sc->sc_if->nregs * sc->sc_if_iospacing,
	    0, &sc->sc_ioh)) {
		printf("ipmi: bus_space_map(%x %x %x 0 %x) failed\n",
		    sc->sc_iot, ia->iaa_if_iobase,
		    sc->sc_if->nregs * sc->sc_if_iospacing, &sc->sc_ioh);
		return (-1);
	}
#if 0
	if (iaa->if_if_irq != -1)
		sc->ih = isa_intr_establish(-1, iaa->if_if_irq,
		    iaa->if_irqlvl, IPL_BIO, ipmi_intr, sc, DEVNAME(sc));
#endif
	return (0);
}

void
ipmi_unmap_regs(struct ipmi_softc *sc, struct ipmi_attach_args *ia)
{
	bus_space_unmap(sc->sc_iot, sc->sc_ioh,
	    sc->sc_if->nregs * sc->sc_if_iospacing);
}

void
ipmi_poll_thread(void *arg)
{
	struct ipmi_thread *thread = arg;
	struct ipmi_softc  *sc = thread->sc;

	while (thread->running) {
		ipmi_refresh_sensors(sc);
		tsleep(thread, PWAIT, "timeout", SENSOR_REFRESH_RATE);
	}
	free(thread, M_DEVBUF);

	kthread_exit(0);
}

void
ipmi_create_thread(void *arg)
{
	struct ipmi_softc *sc = arg;

	if (kthread_create(ipmi_poll_thread, sc->sc_thread, NULL,
	    DEVNAME(sc)) != 0) {
		printf("%s: unable to create polling thread, ipmi disabled\n",
		    DEVNAME(sc));
		return;
	}
}

int
ipmi_probe(void *aux)
{
	struct ipmi_attach_args	*ia = aux;

	if (scan_smbios(SMBIOS_TYPE_IPMI, smbios_ipmi_probe, ia) == 0) {
		dmd_ipmi_t *pipmi;

		/* XXX hack to find Dell PowerEdge 8450 */
		pipmi = (dmd_ipmi_t *)scan_sig(0xC0000L, 0xFFFFFL, 16, 4,
		    "IPMI");
		if (pipmi == NULL) {
			/* no IPMI found */
			return (0);
		}

		/* we have an IPMI signature, fill in attach arg structure */
		ia->iaa_if_type = pipmi->dmd_if_type;
		ia->iaa_if_rev = pipmi->dmd_if_rev;

		return (1);
	}

	return (1);
}

int
ipmi_match(struct device *parent, void *match, void *aux)
{
	struct ipmi_softc	sc;
	struct ipmi_attach_args	*ia = aux;
	struct cfdata		*cf = match;

	if (strcmp(ia->iaa_name, cf->cf_driver->cd_name))
		return (0);

	/* Map registers */
	if (ipmi_map_regs(&sc, ia) == 0) {
		sc.sc_if->probe(&sc);
		ipmi_unmap_regs(&sc, ia);

		return (1);
	}

	return (0);
}

void
ipmi_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipmi_softc	*sc = (void *) self;
	struct ipmi_attach_args	*ia = aux;
	u_int8_t		cmd[32];
	int			len;
	u_int16_t		rec;

	sc->sc_thread = malloc(sizeof(struct ipmi_thread), M_DEVBUF, M_NOWAIT);
	if (sc->sc_thread == NULL) {
		printf("%s: unable to allocate thread\n",
		    DEVNAME(sc));
		return;
	}
	sc->sc_thread->sc = sc;
	sc->sc_thread->running = 1;

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

	/* Setup threads */
	kthread_create_deferred(ipmi_create_thread, sc);

	printf(": version %d.%d interface %s %sbase 0x%x/%x spacing %d",
	    ia->iaa_if_rev >> 4, ia->iaa_if_rev & 0xF, sc->sc_if->name,
	    ia->iaa_if_iotype == 'i' ? "io" : "mem", ia->iaa_if_iobase,
	    ia->iaa_if_iospacing * sc->sc_if->nregs, ia->iaa_if_iospacing);
	if (ia->iaa_if_irq != -1)
		printf(" irq %d", ia->iaa_if_irq);
	printf("\n");
}
