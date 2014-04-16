/*	$OpenBSD: pcex.c,v 1.1 2014/04/16 12:01:33 aoyama Exp $	*/

/*
 * Copyright (c) 2014 Kenji Aoyama.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * PC-9801 extension board slot direct access driver for LUNA-88K{,2}
 */

#include <sys/param.h>
#include <sys/systm.h>	/* tsleep()/wakeup() */
#include <sys/device.h>
#include <sys/ioctl.h>

#include <machine/asm_macro.h>	/* ff1() */
#include <machine/autoconf.h>
#include <machine/board.h>	/* PC_BASE */
#include <machine/conf.h>
#include <machine/intr.h>
#include <machine/pcex.h>

#include <uvm/uvm_extern.h>

#include <luna88k/luna88k/isr.h>

extern int hz;

#if 0
#define PCEX_DEBUG
#endif

#define PCEXMEM_BASE	PC_BASE
#define PCEXIO_BASE	(PC_BASE + 0x1000000)
#define CBUS_ISR	(PC_BASE + 0x1100000)

/*
 * C-bus Interrupt Status Register
 */
volatile u_int8_t *cisr = (u_int8_t *)CBUS_ISR;

const u_int8_t cisr_int_bits[] = {
	0x40,	/* INT 0 */
	0x20,	/* INT 1 */
	0x10,	/* INT 2 */
	0x08,	/* INT 3 */
	0x04,	/* INT 4 */
	0x02,	/* INT 5 */
	0x01	/* INT 6 */
/*	0x80	   NMI(?), not supported in this driver now */
};

/* autoconf stuff */
int pcex_match(struct device *, void *, void *);
void pcex_attach(struct device *, struct device *, void *);

struct pcex_softc {
	struct device sc_dev;
	u_int8_t int_bits;
};

const struct cfattach pcex_ca = {
	sizeof(struct pcex_softc), pcex_match, pcex_attach
};

struct cfdriver pcex_cd = {
	NULL, "pcex", DV_DULL
};

/* prototypes */
int pcex_intr(void *);
int pcex_set_int(struct pcex_softc *, u_int);
int pcex_reset_int(struct pcex_softc *, u_int);
int pcex_wait_int(struct pcex_softc *, u_int);

int
pcex_match(struct device *parent, void *cf, void *aux)
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, pcex_cd.cd_name))
		return 0;
#if 0
	if (badaddr((vaddr_t)ma->ma_addr, 4))
		return 0;
#endif
	return 1;
}

void
pcex_attach(struct device *parent, struct device *self, void *args)
{
	struct pcex_softc *sc = (struct pcex_softc *)self;
	struct mainbus_attach_args *ma = args;
	u_int8_t i;

	sc->int_bits = 0x00;

	/* make sure of clearing interrupt flags for INT0-INT6 */
	for (i = 0; i < 7; i++)
		*cisr = i;

	isrlink_autovec(pcex_intr, (void *)self, ma->ma_ilvl,
		ISRPRI_TTY, self->dv_xname);

	printf("\n");
}

int
pcexopen(dev_t dev, int flag, int mode, struct proc *p)
{
	switch (minor(dev)) {
	case 0:	/* memory area */
	case 1:	/* I/O port area */
		return 0;
	default:
		return ENXIO;
	}
}

int
pcexclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

paddr_t
pcexmmap(dev_t dev, off_t offset, int prot)
{
	paddr_t cookie = -1;

	switch (minor(dev)) {
	case 0:	/* memory area */
		if (offset >= 0 && offset < 0x1000000)
			cookie = (paddr_t)(PCEXMEM_BASE + offset);
		break;
	case 1:	/* I/O port area */
		if (offset >= 0 && offset < 0x10000)
			cookie = (paddr_t)(PCEXIO_BASE + offset);
		break;
	default:
		break;
	}

	return cookie;
}

int
pcexioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct pcex_softc *sc = NULL;
	u_int level;

	if (pcex_cd.cd_ndevs != 0)
		sc = pcex_cd.cd_devs[0];
	if (sc == NULL)
		return ENXIO;

	level = *(u_int *)data;

	switch(cmd) {
	case PCEXSETLEVEL:
		return pcex_set_int(sc, level);

	case PCEXRESETLEVEL:
		return pcex_reset_int(sc, level);

	case PCEXWAITINT:
		return pcex_wait_int(sc, level);

	default:
		return ENOTTY;
	}
}

int
pcex_set_int(struct pcex_softc *sc, u_int level)
{
	if (level > 6)
		return EINVAL;

	sc->int_bits |= cisr_int_bits[level];

	return 0;
}

int
pcex_reset_int(struct pcex_softc *sc, u_int level)
{
	if (level > 6)
		return EINVAL;

	sc->int_bits &= ~cisr_int_bits[level];

	return 0;
}

int
pcex_wait_int(struct pcex_softc *sc, u_int level)
{
	int ret;

	if (level > 6)
		return EINVAL;

	ret = tsleep((void *)sc, PWAIT | PCATCH, "pcex", hz /* XXX: 1 sec. */);
#ifdef PCEX_DEBUG
	if (ret == EWOULDBLOCK)
		printf("%s: timeout in tsleep\n", __func__);
#endif
	return ret;
}

/*
 * Note about interrupt on PC-9801 extension board slot
 *
 * PC-9801 extension board slot bus (so-called 'C-bus' in Japan) use 8 own
 * interrupt levels, INT0-INT6, and NMI.  On LUNA-88K{,2}, they all trigger
 * level 4 interrupt, so we need to check the dedicated interrupt status
 * register to know which C-bus interrupt is occurred.
 *
 * The interrupt status register for C-bus is located at (u_int8_t *)CBUS_ISR.
 * Each bit of the register becomes 0 when corresponding C-bus interrupt has
 * occurred, otherwise 1.
 *
 * bit 7 = NMI(?)
 * bit 6 = INT0
 * bit 5 = INT1
 *  :
 * bit 0 = INT6
 *
 * To clear the C-bus interrupt flag, write the corresponding 'bit' number
 * (as u_int_8) to the register.  For example, if you want to clear INT1,
 * you should write '5' like:
 *   *(u_int8_t *)CBUS_ISR = 5;
 */

/*
 * Interrupt handler
 */
int
pcex_intr(void *arg)
{
	struct pcex_softc *sc = (struct pcex_softc *)arg;
	u_int8_t int_status;
	int n;

	/*
	 * LUNA-88K{,2}'s interrupt level 4 is shared with other devices,
	 * such as le(4), for example.  So we check:
	 * - the value of our C-bus interrupt status register, and
	 * - if the INT level is what we are looking for.
	 */
	int_status = *cisr & sc->int_bits;
	if (int_status == sc->int_bits) return -1;	/* Not for me */

#ifdef PCEX_DEBUG
	printf("%s: called, *cisr=0x%02x, int_bits = 0x%02x\n",
		__func__, *cisr, sc->int_bits);
#endif

	/* Just wakeup(9) for now */
	wakeup((void *)sc);

	/* Make the bit pattern that we should clear interrupt flag */
	int_status = int_status ^ sc->int_bits;

	/* Clear each interrupt flag */
	while ((n = ff1(int_status)) != 32) {
		*cisr = (u_int8_t)n;
		int_status &= ~(1 << n); 
	}

	return 1;
}
