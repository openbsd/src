/*	$OpenBSD: ad1848.c,v 1.20 2000/03/02 23:02:48 mickey Exp $	*/
/*	$NetBSD: ad1848.c,v 1.45 1998/01/30 02:02:38 augustss Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Copyright by Hannu Savolainen 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Portions of this code are from the VOXware support for the ad1848
 * by Hannu Savolainen <hannu@voxware.pp.fi>
 * 
 * Portions also supplied from the SoundBlaster driver for NetBSD.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <vm/vm.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/cs4231var.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (ad1848debug) printf x
int	ad1848debug = 0;
#else
#define DPRINTF(x)
#endif

/*
 * Initial values for the indirect registers of CS4248/AD1848.
 */
static int ad1848_init_values[] = {
			/* Left Input Control */
    GAIN_12|INPUT_MIC_GAIN_ENABLE,
			/* Right Input Control */
    GAIN_12|INPUT_MIC_GAIN_ENABLE,
    ATTEN_12,		/* Left Aux #1 Input Control */
    ATTEN_12,		/* Right Aux #1 Input Control */
    ATTEN_12,		/* Left Aux #2 Input Control */
    ATTEN_12,		/* Right Aux #2 Input Control */
    /* bits 5-0 are attenuation select */
    ATTEN_12,		/* Left DAC output Control */
    ATTEN_12,		/* Right DAC output Control */
			/* Clock and Data Format */
    CLOCK_XTAL1|FMT_PCM8,
			/* Interface Config */
    SINGLE_DMA|AUTO_CAL_ENABLE,
    INTERRUPT_ENABLE,	/* Pin control */
    0x00,		/* Test and Init */
    MODE2,		/* Misc control */
    ATTEN_0<<2,		/* Digital Mix Control */
    0,			/* Upper base Count */
    0,			/* Lower base Count */

    /* These are for CS4231 &c. only (additional registers): */
    0,			/* Alt feature 1 */
    0,			/* Alt feature 2 */
    ATTEN_12,		/* Left line in */
    ATTEN_12,		/* Right line in */
    0,			/* Timer low */
    0,			/* Timer high */
    0,			/* unused */
    0,			/* unused */
    0,			/* IRQ status */
    0,			/* unused */
			/* Mono input (a.k.a speaker) (mic) Control */
    MONO_INPUT_MUTE|ATTEN_6,		/* mute speaker by default */
    0,			/* unused */
    0,			/* record format */
    0,			/* Crystal Clock Select */
    0,			/* upper record count */
    0			/* lower record count */
};

void	ad1848_reset __P((struct ad1848_softc *));
int	ad1848_set_speed __P((struct ad1848_softc *, u_long *));
void	ad1848_mute_monitor __P((void *, int));

static int ad_read __P((struct ad1848_softc *, int));
static void ad_write __P((struct ad1848_softc *, int, int));
static void ad_set_MCE __P((struct ad1848_softc *, int));
static void wait_for_calibration __P((struct ad1848_softc *));

#define ADREAD(sc, addr) bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_iooffs+(addr))
#define ADWRITE(sc, addr, data) bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_iooffs+(addr), (data))

static int
ad_read(sc, reg)
    struct ad1848_softc *sc;
    int reg;
{
    int x, s;

    s = splaudio();
    ADWRITE(sc, AD1848_IADDR, (reg & 0xff) | sc->MCE_bit);
    x = ADREAD(sc, AD1848_IDATA);
    splx(s);
    /*  printf("(%02x<-%02x) ", reg|sc->MCE_bit, x); */

    return x;
}

static void
ad_write(sc, reg, data)
    struct ad1848_softc *sc;
    int reg;
    int data;
{
    int s = splaudio();
    ADWRITE(sc, AD1848_IADDR, (reg & 0xff) | sc->MCE_bit);
    ADWRITE(sc, AD1848_IDATA, data & 0xff);
    splx(s);
    /* printf("(%02x->%02x) ", reg|sc->MCE_bit, data); */
}

static void
ad_set_MCE(sc, state)
    struct ad1848_softc *sc;
    int state;
{
    if (state)
	sc->MCE_bit = MODE_CHANGE_ENABLE;
    else
	sc->MCE_bit = 0;

    ADWRITE(sc, AD1848_IADDR, sc->MCE_bit);
}

static void
wait_for_calibration(sc)
    struct ad1848_softc *sc;
{
    int timeout;

    DPRINTF(("ad1848: Auto calibration started.\n"));
    /*
     * Wait until the auto calibration process has finished.
     *
     * 1) Wait until the chip becomes ready (reads don't return 0x80).
     * 2) Wait until the ACI bit of I11 gets on and then off.
     */
    timeout = 100000;
    while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
	timeout--;

    if (ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
	DPRINTF(("ad1848: Auto calibration timed out(1).\n"));

    ADWRITE(sc, AD1848_IADDR, SP_TEST_AND_INIT);
    timeout = 100000;
    while (timeout > 0 && ADREAD(sc, AD1848_IADDR) != SP_TEST_AND_INIT)
	timeout--;

    if (ADREAD(sc, AD1848_IADDR) == SP_TEST_AND_INIT)
	DPRINTF(("ad1848: Auto calibration timed out(1.5).\n"));

    if (!(ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG)) {
	timeout = 100000;
	while (timeout > 0 && !(ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG))
	    timeout--;

	if (!(ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG))
	    DPRINTF(("ad1848: Auto calibration timed out(2).\n"));
    }

    timeout = 100000;
    while (timeout > 0 && ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG)
	timeout--;
    if (ad_read(sc, SP_TEST_AND_INIT) & AUTO_CAL_IN_PROG)
        DPRINTF(("ad1848: Auto calibration timed out(3).\n"));
}

#ifdef AUDIO_DEBUG
void ad1848_dump_regs __P((struct ad1848_softc *));

void
ad1848_dump_regs(sc)
    struct ad1848_softc *sc;
{
    int i;
    u_char r;
    
    printf("ad1848 status=%02x", ADREAD(sc, AD1848_STATUS));
    printf(" regs: ");
    for (i = 0; i < 16; i++) {
	r = ad_read(sc, i);
	printf("%02x ", r);
    }
    if (sc->mode == 2) {
	    for (i = 16; i < 32; i++) {
		    r = ad_read(sc, i);
		    printf("%02x ", r);
	    }
    }
    printf("\n");
}
#endif

/*
 * Map and probe for the ad1848 chip
 */
int
ad1848_mapprobe(sc, iobase)
    struct ad1848_softc *sc;
    int iobase;
{
    if (!AD1848_BASE_VALID(iobase)) {
#ifdef AUDIO_DEBUG
	printf("ad1848: configured iobase %04x invalid\n", iobase);
#endif
	return 0;
    }

    sc->sc_iooffs = 0;
    /* Map the AD1848 ports */
    if (bus_space_map(sc->sc_iot, iobase, AD1848_NPORT, 0, &sc->sc_ioh))
	return 0;

    if (!ad1848_probe(sc)) {
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, AD1848_NPORT);
	return 0;
    } else
	return 1;
}

/*
 * Probe for the ad1848 chip
 */
int
ad1848_probe(sc)
    struct ad1848_softc *sc;
{
    u_char tmp, tmp1 = 0xff, tmp2 = 0xff;
#if 0
    int i;
#endif
    
    /* Is there an ad1848 chip ? */
    sc->MCE_bit = MODE_CHANGE_ENABLE;
    sc->mode = 1;	/* MODE 1 = original ad1848/ad1846/cs4248 */
    
    /*
     * Check that the I/O address is in use.
     *
     * The SP_IN_INIT bit of the base I/O port is known to be 0 after the
     * chip has performed its power-on initialization. Just assume
     * this has happened before the OS is starting.
     *
     * If the I/O address is unused, inb() typically returns 0xff.
     */
    tmp = ADREAD(sc, AD1848_IADDR);
    if (tmp & SP_IN_INIT) { /* Not a AD1848 */
#if 0
	DPRINTF(("ad_detect_A %x\n", tmp));
#endif
	goto bad;
    }

    /*
     * Test if it's possible to change contents of the indirect registers.
     * Registers 0 and 1 are ADC volume registers. The bit 0x10 is read only
     * so try to avoid using it.
     */
    ad_write(sc, 0, 0xaa);
    ad_write(sc, 1, 0x45);	/* 0x55 with bit 0x10 clear */

    if ((tmp1 = ad_read(sc, 0)) != 0xaa ||
	(tmp2 = ad_read(sc, 1)) != 0x45) {
	DPRINTF(("ad_detect_B (%x/%x)\n", tmp1, tmp2));
	goto bad;
    }

    ad_write(sc, 0, 0x45);
    ad_write(sc, 1, 0xaa);

    if ((tmp1 = ad_read(sc, 0)) != 0x45 ||
	(tmp2 = ad_read(sc, 1)) != 0xaa) {
	DPRINTF(("ad_detect_C (%x/%x)\n", tmp1, tmp2));
	goto bad;
    }

    /*
     * The indirect register I12 has some read only bits. Lets
     * try to change them.
     */
    tmp = ad_read(sc, SP_MISC_INFO);
    ad_write(sc, SP_MISC_INFO, (~tmp) & 0x0f);

    if ((tmp & 0x0f) != ((tmp1 = ad_read(sc, SP_MISC_INFO)) & 0x0f)) {
	DPRINTF(("ad_detect_D (%x)\n", tmp1));
	goto bad;
    }

    /*
     * MSB and 4 LSBs of the reg I12 tell the chip revision.
     *
     * A preliminary version of the AD1846 data sheet stated that it
     * used an ID field of 0x0B.  The current version, however,
     * states that the AD1846 uses ID 0x0A, just like the AD1848K.
     *
     * this switch statement will need updating as newer clones arrive....
     */
    switch (tmp1 & 0x8f) {
    case 0x09:
	sc->chip_name = "AD1848J";
	break;
    case 0x0A:
	sc->chip_name = "AD1848K";
	break;
#if 0	/* See above */
    case 0x0B:
	sc->chip_name = "AD1846";
	break;
#endif
    case 0x81:
	sc->chip_name = "CS4248revB"; /* or CS4231 rev B; see below */
	break;
    case 0x89:
	sc->chip_name = "CS4248";
	break;
    case 0x8A:
	sc->chip_name = "broken"; /* CS4231/AD1845; see below */
	break;
    default:
	sc->chip_name = "unknown";
	DPRINTF(("ad1848: unknown codec version %#02X\n", (tmp1 & 0x8f)));
    }	
    
#if 0
    /*
     * XXX I don't know why, but this probe fails on an otherwise well-working
     * AW35/pro card, so I'll just take it out for now. [niklas@openbsd.org]
     */

    /*
     * The original AD1848/CS4248 has just 16 indirect registers. This means
     * that I0 and I16 should return the same value (etc.).
     * Ensure that the Mode2 enable bit of I12 is 0. Otherwise this test fails
     * with CS4231, AD1845, etc.
     */
    ad_write(sc, SP_MISC_INFO, 0);	/* Mode2 = disabled */

    for (i = 0; i < 16; i++)
	if ((tmp1 = ad_read(sc, i)) != (tmp2 = ad_read(sc, i + 16))) {
	    if (i != SP_TEST_AND_INIT) {
	        DPRINTF(("ad_detect_F(%d/%x/%x)\n", i, tmp1, tmp2));
	        goto bad;
	    }
	}
#endif

    /*
     * Try to switch the chip to mode2 (CS4231) by setting the MODE2 bit
     * The bit 0x80 is always 1 in CS4248, CS4231, and AD1845.
     */
    ad_write(sc, SP_MISC_INFO, MODE2);	/* Set mode2, clear 0x80 */

    tmp1 = ad_read(sc, SP_MISC_INFO);
    if ((tmp1 & 0xc0) == (0x80 | MODE2)) {
	/*
	 *      CS4231 or AD1845 detected - is it?
	 *
	 *	Verify that setting I2 doesn't change I18.
	 */
	ad_write(sc, 18, 0x88); /* Set I18 to known value */

	ad_write(sc, 2, 0x45);
	if ((tmp2 = ad_read(sc, 18)) != 0x45) { /* No change -> CS4231? */
	    ad_write(sc, 2, 0xaa);
	    if ((tmp2 = ad_read(sc, 18)) == 0xaa) {     /* Rotten bits? */
		DPRINTF(("ad_detect_H(%x)\n", tmp2));
		goto bad;
	    }

	    /*
	     *  It's a CS4231, or another clone with 32 registers.
	     *  Let's find out which by checking I25.
	     */
	    if ((tmp1 & 0x8f) == 0x8a) {
		tmp1 = ad_read(sc, CS_VERSION_ID);
		switch (tmp1 & 0xe7) {
		case 0xA0:
		    sc->chip_name = "CS4231A";
		    break;
		case 0x80:
		    /*  XXX I25 no good, AD1845 same as CS4231 */
		    sc->chip_name = "CS4231 or AD1845";
		    break;
		case 0x82:
		case 0xa2:
		    sc->chip_name = "CS4232";
		    break;
		case 0x03:
		    sc->chip_name = "CS4236/CS4236B";
		    break;
		}
	    }
	    sc->mode = 2;
	}
    }

    /* Wait for 1848 to init */
    while(ADREAD(sc, AD1848_IADDR) & SP_IN_INIT)
        ;
	
    /* Wait for 1848 to autocal */
    ADWRITE(sc, AD1848_IADDR, SP_TEST_AND_INIT);
    while(ADREAD(sc, AD1848_IDATA) & AUTO_CAL_IN_PROG)
        ;

    return 1;
bad:
    return 0;
}

/* Unmap the I/O ports */
void
ad1848_unmap(sc)
    struct ad1848_softc *sc;
{
    bus_space_unmap(sc->sc_iot, sc->sc_ioh, AD1848_NPORT);
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
ad1848_attach(sc)
    struct ad1848_softc *sc;
{
    int i;
    struct ad1848_volume vol_mid = {220, 220};
    struct ad1848_volume vol_0   = {0, 0};
    struct audio_params pparams, rparams;
    int timeout;
    
    sc->sc_locked = 0;
    sc->sc_playrun = NOTRUNNING;
    sc->sc_recrun = NOTRUNNING;

    if (sc->sc_drq != -1) {
	if (isa_dmamap_create(sc->sc_isa, sc->sc_drq, MAX_ISADMA,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		printf("ad1848_attach: can't create map for drq %d\n",
		    sc->sc_drq);
		return;
	}
    }
    if (sc->sc_recdrq != -1 && sc->sc_recdrq != sc->sc_drq) {
	if (isa_dmamap_create(sc->sc_isa, sc->sc_recdrq, MAX_ISADMA,
	    BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		printf("ad1848_attach: can't creape map for drq %d\n",
		    sc->sc_recdrq);
		return;
	}
    }

    /* Initialize the ad1848... */
    for (i = 0; i < 0x10; i++) {
	ad_write(sc, i, ad1848_init_values[i]);
        timeout = 100000;
        while (timeout > 0 && ad_read(sc, AD1848_IADDR) & SP_IN_INIT)
	    timeout--;
    }
    /* ...and additional CS4231 stuff too */
    if (sc->mode == 2) {
	    ad_write(sc, SP_INTERFACE_CONFIG, 0); /* disable SINGLE_DMA */
	    for (i = 0x10; i < 0x20; i++)
		    if (ad1848_init_values[i] != 0) {
			    ad_write(sc, i, ad1848_init_values[i]);
			    timeout = 100000;
    			    while (timeout > 0 && 
				   ad_read(sc, AD1848_IADDR) & SP_IN_INIT)
				timeout--;
		    }
    }
    ad1848_reset(sc);

    pparams = audio_default;
    rparams = audio_default;
    (void) ad1848_set_params(sc, AUMODE_RECORD|AUMODE_PLAY, 0, &pparams, &rparams);

    /* Set default gains */
    (void) ad1848_set_rec_gain(sc, &vol_mid);
    (void) ad1848_set_channel_gain(sc, AD1848_DAC_CHANNEL, &vol_mid);
    (void) ad1848_set_channel_gain(sc, AD1848_MONITOR_CHANNEL, &vol_0);
    (void) ad1848_set_channel_gain(sc, AD1848_AUX1_CHANNEL, &vol_mid);	/* CD volume */
    if (sc->mode == 2) {
	(void) ad1848_set_channel_gain(sc, AD1848_AUX2_CHANNEL, &vol_mid); /* CD volume */
	(void) ad1848_set_channel_gain(sc, AD1848_LINE_CHANNEL, &vol_mid);
	(void) ad1848_set_channel_gain(sc, AD1848_MONO_CHANNEL, &vol_0);
	sc->mute[AD1848_MONO_CHANNEL] = MUTE_ALL;
    } else
	(void) ad1848_set_channel_gain(sc, AD1848_AUX2_CHANNEL, &vol_0);

    /* Set default port */
    (void) ad1848_set_rec_port(sc, MIC_IN_PORT);

    if (sc->chip_name)
	printf(": %s", sc->chip_name);
#undef WAITREADY
}

/*
 * Various routines to interface to higher level audio driver
 */
struct ad1848_mixerinfo {
  int  left_reg;
  int  right_reg;
  int  atten_bits;
  int  atten_mask;
} mixer_channel_info[] = 
{ { SP_LEFT_AUX2_CONTROL, SP_RIGHT_AUX2_CONTROL, AUX_INPUT_ATTEN_BITS,
    AUX_INPUT_ATTEN_MASK },
  { SP_LEFT_AUX1_CONTROL, SP_RIGHT_AUX1_CONTROL, AUX_INPUT_ATTEN_BITS,
    AUX_INPUT_ATTEN_MASK },
  { SP_LEFT_OUTPUT_CONTROL, SP_RIGHT_OUTPUT_CONTROL, 
    OUTPUT_ATTEN_BITS, OUTPUT_ATTEN_MASK }, 
  { CS_LEFT_LINE_CONTROL, CS_RIGHT_LINE_CONTROL, LINE_INPUT_ATTEN_BITS,
    LINE_INPUT_ATTEN_MASK },
  { CS_MONO_IO_CONTROL, 0, MONO_INPUT_ATTEN_BITS, MONO_INPUT_ATTEN_MASK },
  { SP_DIGITAL_MIX, 0, OUTPUT_ATTEN_BITS, MIX_ATTEN_MASK }
};

/*
 *  This function doesn't set the mute flags but does use them.
 *  The mute flags reflect the mutes that have been applied by the user.
 *  However, the driver occasionally wants to mute devices (e.g. when chaing
 *  sampling rate). These operations should not affect the mute flags.
 */

void 
ad1848_mute_channel(sc, device, mute) 
	struct ad1848_softc *sc;
	int device;
	int mute;
{
  u_char reg;

  reg = ad_read(sc, mixer_channel_info[device].left_reg);

  if (mute & MUTE_LEFT) {
    if (device == AD1848_MONITOR_CHANNEL)
        ad_write(sc, mixer_channel_info[device].left_reg, reg & 0xFE);
    else
        ad_write(sc, mixer_channel_info[device].left_reg, reg | 0x80);
  } else if (!(sc->mute[device] & MUTE_LEFT)) {
    if (device == AD1848_MONITOR_CHANNEL)
        ad_write(sc, mixer_channel_info[device].left_reg, reg | 0x01);
    else
        ad_write(sc, mixer_channel_info[device].left_reg, reg & ~0x80);
  }

  if (!mixer_channel_info[device].right_reg) {
    return;
  }

  reg = ad_read(sc, mixer_channel_info[device].right_reg);

  if (mute & MUTE_RIGHT)
    ad_write(sc, mixer_channel_info[device].right_reg, reg | 0x80);
  else if (!(sc->mute[device] & MUTE_RIGHT)) {
    ad_write(sc, mixer_channel_info[device].right_reg, reg & ~0x80);
  }
}


int
ad1848_set_channel_gain(sc, device, gp)
    struct ad1848_softc *sc;
    int device;
    struct ad1848_volume *gp;
{
    struct ad1848_mixerinfo *info = &mixer_channel_info[device];
    u_char reg;
    u_int atten;

    sc->gains[device] = *gp;

    atten = ((AUDIO_MAX_GAIN - gp->left) * info->atten_bits)/AUDIO_MAX_GAIN;

    reg = ad_read(sc, info->left_reg) & (info->atten_mask);
    if (device == AD1848_MONITOR_CHANNEL)
      reg |= ((atten & info->atten_bits) << 2);
    else
      reg |= ((atten & info->atten_bits));

    ad_write(sc, info->left_reg, reg);

    if (!info->right_reg)
      return (0);

    atten = ((AUDIO_MAX_GAIN - gp->right) * info->atten_bits)/AUDIO_MAX_GAIN;
    reg = ad_read(sc, info->right_reg);
    reg &= (info->atten_mask);
    ad_write(sc, info->right_reg, (atten& info->atten_bits)|reg);

    return(0);
}


int
ad1848_get_device_gain(sc, device, gp)
    struct ad1848_softc *sc;
    int device;
    struct ad1848_volume *gp;
{
    *gp = sc->gains[device];
    return(0);
}

int
ad1848_get_rec_gain(sc, gp)
    struct ad1848_softc *sc;
    struct ad1848_volume *gp;
{
    *gp = sc->rec_gain;
    return(0);
}

int
ad1848_set_rec_gain(sc, gp)
    struct ad1848_softc *sc;
    struct ad1848_volume *gp;
{
    u_char reg, gain;
    
    DPRINTF(("ad1848_set_rec_gain: %d:%d\n", gp->left, gp->right));

    sc->rec_gain = *gp;

    gain = (gp->left * GAIN_22_5)/AUDIO_MAX_GAIN;
    reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
    reg &= INPUT_GAIN_MASK;
    ad_write(sc, SP_LEFT_INPUT_CONTROL, (gain&0x0f)|reg);

    gain = (gp->right * GAIN_22_5)/AUDIO_MAX_GAIN;
    reg = ad_read(sc, SP_RIGHT_INPUT_CONTROL);
    reg &= INPUT_GAIN_MASK;
    ad_write(sc, SP_RIGHT_INPUT_CONTROL, (gain&0x0f)|reg);

    return(0);
}


void
ad1848_mute_monitor(addr, mute)
	void *addr;
	int mute;
{
	struct ad1848_softc *sc = addr;

	DPRINTF(("ad1848_mute_monitor: %smuting\n", mute ? "" : "un"));
	if (sc->mode == 2) {
	        ad1848_mute_channel(sc, AD1848_DAC_CHANNEL, mute ? MUTE_ALL : 0);
		ad1848_mute_channel(sc, AD1848_MONO_CHANNEL, mute ? MUTE_MONO : 0);
		ad1848_mute_channel(sc, AD1848_LINE_CHANNEL, mute ? MUTE_ALL : 0);
	}
		
	ad1848_mute_channel(sc, AD1848_AUX2_CHANNEL, mute ? MUTE_ALL : 0);
	ad1848_mute_channel(sc, AD1848_AUX1_CHANNEL, mute ? MUTE_ALL : 0);
}

int
ad1848_set_mic_gain(sc, gp)
    struct ad1848_softc *sc;
    struct ad1848_volume *gp;
{
    u_char reg;
    
    DPRINTF(("cs4231_set_mic_gain: %d\n", gp->left));

    if (gp->left > AUDIO_MAX_GAIN/2) {
	    sc->mic_gain_on = 1;
	    reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
	    ad_write(sc, SP_LEFT_INPUT_CONTROL, reg | INPUT_MIC_GAIN_ENABLE);
    } else {
	    sc->mic_gain_on = 0;
	    reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
	    ad_write(sc, SP_LEFT_INPUT_CONTROL, reg & ~INPUT_MIC_GAIN_ENABLE);
    }

    return(0);
}

int
ad1848_get_mic_gain(sc, gp)
    struct ad1848_softc *sc;
    struct ad1848_volume *gp;
{
	if (sc->mic_gain_on)
		gp->left = gp->right = AUDIO_MAX_GAIN;
	else
		gp->left = gp->right = AUDIO_MIN_GAIN;
	return(0);
}


static ad1848_devmap_t *ad1848_mixer_find_dev __P((ad1848_devmap_t *, int, mixer_ctrl_t *));

static ad1848_devmap_t *
ad1848_mixer_find_dev(map, cnt, cp)
  ad1848_devmap_t *map;
  int cnt;
  mixer_ctrl_t *cp;

{
  int idx;

  for (idx = 0; idx < cnt; idx++) {
    if (map[idx].id == cp->dev) {
      return (&map[idx]);
    }
  }
  return (NULL);
}

int     
ad1848_mixer_get_port(ac, map, cnt, cp)
  struct ad1848_softc *ac;
  struct ad1848_devmap *map;
  int cnt;
  mixer_ctrl_t *cp;
{
  ad1848_devmap_t *entry;
  struct ad1848_volume vol;
  int error = EINVAL;
  int dev;

  if (!(entry = ad1848_mixer_find_dev(map, cnt, cp)))
    return (ENXIO);

  dev = entry->dev;

  switch (entry->kind) {
  case AD1848_KIND_LVL:
    if (cp->type != AUDIO_MIXER_VALUE)
      break;

    if (dev < AD1848_AUX2_CHANNEL ||
	dev > AD1848_MONITOR_CHANNEL)
      break;

    if (cp->un.value.num_channels != 1 &&
        mixer_channel_info[dev].right_reg == 0) 
      break;

    error = ad1848_get_device_gain(ac, dev, &vol);
    if (!error)
      ad1848_from_vol(cp, &vol);

    break;

  case AD1848_KIND_MUTE:
    if (cp->type != AUDIO_MIXER_ENUM) break;

    cp->un.ord = ac->mute[dev] ? 1 : 0;
    error = 0;
    break;

  case AD1848_KIND_RECORDGAIN:
    if (cp->type != AUDIO_MIXER_VALUE) break;

    error = ad1848_get_rec_gain(ac, &vol);
    if (!error)
      ad1848_from_vol(cp, &vol);

    break;

  case AD1848_KIND_MICGAIN:
    if (cp->type != AUDIO_MIXER_VALUE) break;

    error = ad1848_get_mic_gain(ac, &vol);
    if (!error)
      ad1848_from_vol(cp, &vol);

    break;

  case AD1848_KIND_RECORDSOURCE:
    if (cp->type != AUDIO_MIXER_ENUM) break;
    cp->un.ord = ad1848_get_rec_port(ac);
    error = 0;
    break;
  default:
    printf ("Invalid kind\n");
    break;
  }

  return (error);
}

int     
ad1848_mixer_set_port(ac, map, cnt, cp)
  struct ad1848_softc *ac;
  struct ad1848_devmap *map;
  int cnt;
  mixer_ctrl_t *cp;
{
  ad1848_devmap_t *entry;
  struct ad1848_volume vol;
  int error = EINVAL;
  int dev;

  if (!(entry = ad1848_mixer_find_dev(map, cnt, cp)))
    return (ENXIO);

  dev = entry->dev;

  switch (entry->kind) {
  case AD1848_KIND_LVL:
    if (cp->type != AUDIO_MIXER_VALUE)
      break;

    if (dev < AD1848_AUX2_CHANNEL ||
	dev > AD1848_MONITOR_CHANNEL)
      break;

    if (cp->un.value.num_channels != 1 &&
        mixer_channel_info[dev].right_reg == 0) 
      break;
    
    ad1848_to_vol(cp, &vol);
    error = ad1848_set_channel_gain(ac, dev, &vol);
    break;

  case AD1848_KIND_MUTE:
    if (cp->type != AUDIO_MIXER_ENUM) break;
    
    ac->mute[dev] = (cp->un.ord ? MUTE_ALL : 0);
    ad1848_mute_channel(ac, dev, ac->mute[dev]);
    error = 0;
    break;

  case AD1848_KIND_RECORDGAIN:
    if (cp->type != AUDIO_MIXER_VALUE) break;

    ad1848_to_vol(cp, &vol);
    error = ad1848_set_rec_gain(ac, &vol);
    break;

  case AD1848_KIND_MICGAIN:
    if (cp->type != AUDIO_MIXER_VALUE) break;

    ad1848_to_vol(cp, &vol);
    error = ad1848_set_mic_gain(ac, &vol);
    break;

  case AD1848_KIND_RECORDSOURCE:
    if (cp->type != AUDIO_MIXER_ENUM) break;

    error = ad1848_set_rec_port(ac,  cp->un.ord);
    break;
  default:
    printf ("Invalid kind\n");
    break;
  }

  return (error);
}


int
ad1848_query_encoding(addr, fp)
    void *addr;
    struct audio_encoding *fp;
{
    struct ad1848_softc *sc = addr;

    switch (fp->index) {
    case 0:
	strcpy(fp->name, AudioEmulaw);
	fp->encoding = AUDIO_ENCODING_ULAW;
	fp->precision = 8;
	fp->flags = 0;
	break;
    case 1:
	strcpy(fp->name, AudioEalaw);
	fp->encoding = AUDIO_ENCODING_ALAW;
	fp->precision = 8;
	fp->flags = 0;
	break;
    case 2:
	strcpy(fp->name, AudioEslinear_le);
	fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
	fp->precision = 16;
	fp->flags = 0;
	break;
    case 3:
	strcpy(fp->name, AudioEulinear);
	fp->encoding = AUDIO_ENCODING_ULINEAR;
	fp->precision = 8;
	fp->flags = 0;
	break;

    case 4: /* only on CS4231 */
	strcpy(fp->name, AudioEslinear_be);
	fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
	fp->precision = 16;
	fp->flags = sc->mode == 1 ? AUDIO_ENCODINGFLAG_EMULATED : 0;
	break;

    /* emulate some modes */
    case 5:
	strcpy(fp->name, AudioEslinear);
	fp->encoding = AUDIO_ENCODING_SLINEAR;
	fp->precision = 8;
	fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
	break;
    case 6:
	strcpy(fp->name, AudioEulinear_le);
	fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
	fp->precision = 16;
	fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
	break;
    case 7:
	strcpy(fp->name, AudioEulinear_be);
	fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
	fp->precision = 16;
	fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
	break;

    case 8: /* only on CS4231 */
	if (sc->mode == 1)
	    return EINVAL;
	strcpy(fp->name, AudioEadpcm);
	fp->encoding = AUDIO_ENCODING_ADPCM;
	fp->precision = 8;
	fp->flags = 0;
	break;
    default:
	return EINVAL;
	/*NOTREACHED*/
    }
    return (0);
}

int
ad1848_set_params(addr, setmode, usemode, p, r)
    void *addr;
    int setmode, usemode;
    struct audio_params *p, *r;
{
    struct ad1848_softc *sc = addr;
    int error, bits, enc;
    void (*pswcode) __P((void *, u_char *buf, int cnt));
    void (*rswcode) __P((void *, u_char *buf, int cnt));

    DPRINTF(("ad1848_set_params: %d %d %d %ld\n", 
	     p->encoding, p->precision, p->channels, p->sample_rate));

    enc = p->encoding;
    pswcode = rswcode = 0;
    switch (enc) {
    case AUDIO_ENCODING_SLINEAR_LE:
	if (p->precision == 8) {
	    enc = AUDIO_ENCODING_ULINEAR_LE;
	    pswcode = rswcode = change_sign8;
	}
	break;
    case AUDIO_ENCODING_SLINEAR_BE:
	if (p->precision == 16 && sc->mode == 1) {
	    enc = AUDIO_ENCODING_SLINEAR_LE;
	    pswcode = rswcode = swap_bytes;
	}
	break;
    case AUDIO_ENCODING_ULINEAR_LE:
	if (p->precision == 16) {
	    enc = AUDIO_ENCODING_SLINEAR_LE;
	    pswcode = rswcode = change_sign16;
	}
	break;
    case AUDIO_ENCODING_ULINEAR_BE:
	if (p->precision == 16) {
	    enc = AUDIO_ENCODING_SLINEAR_LE;
	    pswcode = swap_bytes_change_sign16;
	    rswcode = change_sign16_swap_bytes;
	}
	break;
    }
    switch (enc) {
    case AUDIO_ENCODING_ULAW:
	bits = FMT_ULAW >> 5;
	break;
    case AUDIO_ENCODING_ALAW:
	bits = FMT_ALAW >> 5;
	break;
    case AUDIO_ENCODING_ADPCM:
	bits = FMT_ADPCM >> 5;
	break;
    case AUDIO_ENCODING_SLINEAR_LE:
	if (p->precision == 16)
	    bits = FMT_TWOS_COMP >> 5;
	else
	    return EINVAL;
	break;
    case AUDIO_ENCODING_SLINEAR_BE:
	if (p->precision == 16)
	    bits = FMT_TWOS_COMP_BE >> 5;
	else
	    return EINVAL;
	break;
    case AUDIO_ENCODING_ULINEAR_LE:
	if (p->precision == 8)
	    bits = FMT_PCM8 >> 5;
	else
	    return EINVAL;
	break;
    default:
	return EINVAL;
    }

    if (p->channels < 1 || p->channels > 2)
	return EINVAL;

    error = ad1848_set_speed(sc, &p->sample_rate);
    if (error)
	return error;

    p->sw_code = pswcode;
    r->sw_code = rswcode;

    sc->format_bits = bits;
    sc->channels = p->channels;
    sc->precision = p->precision;
    sc->need_commit = 1;

    DPRINTF(("ad1848_set_params succeeded, bits=%x\n", bits));
    return (0);
}

int
ad1848_set_rec_port(sc, port)
    struct ad1848_softc *sc;
    int port;
{
    u_char inp, reg;
    
    DPRINTF(("ad1848_set_rec_port: 0x%x\n", port));

    if (port == MIC_IN_PORT) {
	inp = MIC_INPUT;
    }
    else if (port == LINE_IN_PORT) {
	inp = LINE_INPUT;
    }
    else if (port == DAC_IN_PORT) {
	inp = MIXED_DAC_INPUT;
    }
    else if (sc->mode == 2 && port == AUX1_IN_PORT) {
	inp = AUX_INPUT;
    }
    else
	return(EINVAL);

    reg = ad_read(sc, SP_LEFT_INPUT_CONTROL);
    reg &= INPUT_SOURCE_MASK;
    ad_write(sc, SP_LEFT_INPUT_CONTROL, (inp|reg));

    reg = ad_read(sc, SP_RIGHT_INPUT_CONTROL);
    reg &= INPUT_SOURCE_MASK;
    ad_write(sc, SP_RIGHT_INPUT_CONTROL, (inp|reg));

    sc->rec_port = port;

    return(0);
}

int
ad1848_get_rec_port(sc)
    struct ad1848_softc *sc;
{
    return(sc->rec_port);
}

int
ad1848_round_blocksize(addr, blk)
    void *addr;
    int blk;
{
    struct ad1848_softc *sc = addr;

    sc->sc_lastcc = -1;

    /* Round to a multiple of the biggest sample size. */
    blk &= -4;

    return (blk);
}

int
ad1848_open(addr, flags)
    void *addr;
    int flags;
{
    struct ad1848_softc *sc = addr;

    DPRINTF(("ad1848_open: sc=%p\n", sc));

    sc->sc_intr = 0;
    sc->sc_lastcc = -1;
    sc->sc_locked = 0;

    /* Enable interrupts */
    DPRINTF(("ad1848_open: enable intrs\n"));
    ad_write(sc, SP_PIN_CONTROL, INTERRUPT_ENABLE|ad_read(sc, SP_PIN_CONTROL));

#ifdef AUDIO_DEBUG
    if (ad1848debug)
	ad1848_dump_regs(sc);
#endif

    return 0;
}

/*
 * Close function is called at splaudio().
 */
void
ad1848_close(addr)
    void *addr;
{
    struct ad1848_softc *sc = addr;
    u_char r;
    
    sc->sc_intr = 0;

    DPRINTF(("ad1848_close: stop DMA\n"));
    if (sc->sc_playrun != NOTRUNNING) {
	isa_dmaabort(sc->sc_isa, sc->sc_drq);
	sc->sc_playrun = NOTRUNNING;
    }
    if (sc->sc_recrun != NOTRUNNING) {
	isa_dmaabort(sc->sc_isa, sc->sc_recdrq);
	sc->sc_recrun = NOTRUNNING;
    }
    ad_write(sc, SP_LOWER_BASE_COUNT, (u_char)0);
    ad_write(sc, SP_UPPER_BASE_COUNT, (u_char)0);

    /* Disable interrupts */
    DPRINTF(("ad1848_close: disable intrs\n"));
    ad_write(sc, SP_PIN_CONTROL, 
	     ad_read(sc, SP_PIN_CONTROL) & ~INTERRUPT_ENABLE);

    DPRINTF(("ad1848_close: disable capture and playback\n"));
    r = ad_read(sc, SP_INTERFACE_CONFIG);
    r &= ~(CAPTURE_ENABLE|PLAYBACK_ENABLE);
    ad_write(sc, SP_INTERFACE_CONFIG, r);

#ifdef AUDIO_DEBUG
    if (ad1848debug)
	ad1848_dump_regs(sc);
#endif
}

/*
 * Lower-level routines
 */
int
ad1848_commit_settings(addr)
    void *addr;
{
    struct ad1848_softc *sc = addr;
    int timeout;
    u_char fs;
    int s;

    if (!sc->need_commit)
	return 0;

    s = splaudio();
    
    ad1848_mute_monitor(sc, 1);
    
    ad_set_MCE(sc, 1);		/* Enables changes to the format select reg */

    fs = sc->speed_bits | (sc->format_bits << 5);

    if (sc->channels == 2)
	fs |= FMT_STEREO;

    ad_write(sc, SP_CLOCK_DATA_FORMAT, fs);

    /*
     * If mode == 2 (CS4231), set I28 also. It's the capture format register.
     */
    if (sc->mode == 2) {
	/* Gravis Ultrasound MAX SDK sources says something about errata
	 * sheets, with the implication that these inb()s are necessary.
	 */
	(void)ADREAD(sc, AD1848_IDATA);
	(void)ADREAD(sc, AD1848_IDATA);
	/*
	 * Write to I8 starts resyncronization. Wait until it completes.
	 */
	timeout = 100000;
	while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
	    timeout--;

	ad_write(sc, CS_REC_FORMAT, fs);
	/* Gravis Ultrasound MAX SDK sources says something about errata
	 * sheets, with the implication that these inb()s are necessary.
	 */
	(void)ADREAD(sc, AD1848_IDATA);
	(void)ADREAD(sc, AD1848_IDATA);
	/* Now wait for resync for capture side of the house */
    }
    /*
     * Write to I8 starts resyncronization. Wait until it completes.
     */
    timeout = 100000;
    while (timeout > 0 && ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
	timeout--;

    if (ADREAD(sc, AD1848_IADDR) == SP_IN_INIT)
	printf("ad1848_commit: Auto calibration timed out\n");

    /*
     * Starts the calibration process and
     * enters playback mode after it.
     */
    ad_set_MCE(sc, 0);
    wait_for_calibration(sc);

    ad1848_mute_monitor(sc, 0);

    sc->sc_lastcc = -1;

    splx(s);
    
    sc->need_commit = 0;
    return 0;
}

void
ad1848_reset(sc)
    struct ad1848_softc *sc;
{
    u_char r;
    
    DPRINTF(("ad1848_reset\n"));
    
    /* Clear the PEN and CEN bits */
    r = ad_read(sc, SP_INTERFACE_CONFIG);
    r &= ~(CAPTURE_ENABLE|PLAYBACK_ENABLE);
    ad_write(sc, SP_INTERFACE_CONFIG, r);

    if (sc->mode == 2) {
	    ADWRITE(sc, AD1848_IADDR, CS_IRQ_STATUS);
	    ADWRITE(sc, AD1848_IDATA, 0);
    }
    /* Clear interrupt status */
    ADWRITE(sc, AD1848_STATUS, 0);
#ifdef AUDIO_DEBUG
    if (ad1848debug)
	ad1848_dump_regs(sc);
#endif
}

int
ad1848_set_speed(sc, argp)
    struct ad1848_softc *sc;
    u_long *argp;
{
    /*
     * The sampling speed is encoded in the least significant nible of I8. The
     * LSB selects the clock source (0=24.576 MHz, 1=16.9344 Mhz) and other
     * three bits select the divisor (indirectly):
     *
     * The available speeds are in the following table. Keep the speeds in
     * the increasing order.
     */
    typedef struct {
	int	speed;
	u_char	bits;
    } speed_struct;
    u_long arg = *argp;

    static speed_struct speed_table[] =  {
	{5510, (0 << 1) | 1},
	{5510, (0 << 1) | 1},
	{6620, (7 << 1) | 1},
	{8000, (0 << 1) | 0},
	{9600, (7 << 1) | 0},
	{11025, (1 << 1) | 1},
	{16000, (1 << 1) | 0},
	{18900, (2 << 1) | 1},
	{22050, (3 << 1) | 1},
	{27420, (2 << 1) | 0},
	{32000, (3 << 1) | 0},
	{33075, (6 << 1) | 1},
	{37800, (4 << 1) | 1},
	{44100, (5 << 1) | 1},
	{48000, (6 << 1) | 0}
    };

    int i, n, selected = -1;

    n = sizeof(speed_table) / sizeof(speed_struct);

    if (arg < speed_table[0].speed)
	selected = 0;
    if (arg > speed_table[n - 1].speed)
	selected = n - 1;

    for (i = 1 /*really*/ ; selected == -1 && i < n; i++)
	if (speed_table[i].speed == arg)
	    selected = i;
	else if (speed_table[i].speed > arg) {
	    int diff1, diff2;

	    diff1 = arg - speed_table[i - 1].speed;
	    diff2 = speed_table[i].speed - arg;

	    if (diff1 < diff2)
		selected = i - 1;
	    else
		selected = i;
	}

    if (selected == -1) {
	printf("ad1848: Can't find speed???\n");
	selected = 3;
    }

    sc->speed_bits = speed_table[selected].bits;
    sc->need_commit = 1;
    *argp = speed_table[selected].speed;

    return (0);
}

/*
 * Halt a DMA in progress.
 */
int
ad1848_halt_out_dma(addr)
    void *addr;
{
    struct ad1848_softc *sc = addr;
    u_char reg;
	
    DPRINTF(("ad1848: ad1848_halt_out_dma\n"));

    reg = ad_read(sc, SP_INTERFACE_CONFIG);
    ad_write(sc, SP_INTERFACE_CONFIG, (reg & ~PLAYBACK_ENABLE));
    sc->sc_locked = 0;

    return(0);
}

int
ad1848_halt_in_dma(addr)
    void *addr;
{
    struct ad1848_softc *sc = addr;
    u_char reg;
    
    DPRINTF(("ad1848: ad1848_halt_in_dma\n"));

    reg = ad_read(sc, SP_INTERFACE_CONFIG);
    ad_write(sc, SP_INTERFACE_CONFIG, (reg & ~CAPTURE_ENABLE));
    sc->sc_locked = 0;

    return(0);
}

int
ad1848_dma_init_input(addr, buf, cc)
    void *addr;
    void *buf;
    int cc;
{
    struct ad1848_softc *sc = addr;

    sc->sc_recrun = DMARUNNING;
    sc->sc_dma_flags = DMAMODE_READ | DMAMODE_LOOP;
    sc->sc_dma_bp = buf;
    sc->sc_dma_cnt = cc;
    isa_dmastart(sc->sc_isa, sc->sc_recdrq, buf, cc, NULL,
		 sc->sc_dma_flags, BUS_DMA_NOWAIT);
    DPRINTF(("ad1848_dma_init_input: %p %d\n", buf, cc));
    return 0;
}

/*
 * DMA input/output are called at splaudio().
 */
int
ad1848_dma_input(addr, p, cc, intr, arg)
    void *addr;
    void *p;
    int cc;
    void (*intr) __P((void *));
    void *arg;
{
    struct ad1848_softc *sc = addr;
    u_char reg;
    
    if (sc->sc_locked) {
	DPRINTF(("ad1848_dma_input: locked\n"));
	return 0;
    }
    
#ifdef AUDIO_DEBUG
    if (ad1848debug > 1)
	printf("ad1848_dma_input: cc=%d %p (%p)\n", cc, intr, arg);
#endif
    sc->sc_locked = 1;
    sc->sc_intr = intr;
    sc->sc_arg = arg;

    switch (sc->sc_recrun) {
    case NOTRUNNING:
	sc->sc_dma_flags = DMAMODE_READ;
	sc->sc_dma_bp = p;
	sc->sc_dma_cnt = cc;
	isa_dmastart(sc->sc_isa, sc->sc_recdrq, p, cc, NULL,
		     DMAMODE_READ, BUS_DMA_NOWAIT);
	goto startpcm;
    case DMARUNNING:
	sc->sc_recrun = PCMRUNNING;
    startpcm:
	if (sc->precision == 16)
	    cc >>= 1;
	if (sc->channels == 2)
	    cc >>= 1;
	cc--;
    
	if (sc->sc_lastcc != cc || sc->sc_mode != AUMODE_RECORD) {
	    ad_write(sc, SP_LOWER_BASE_COUNT, (u_char)(cc & 0xff));
	    ad_write(sc, SP_UPPER_BASE_COUNT, (u_char)((cc >> 8) & 0xff));

	    if (sc->mode == 2) {
		ad_write(sc, CS_LOWER_REC_CNT, (u_char)(cc & 0xff));
		ad_write(sc, CS_UPPER_REC_CNT, (u_char)((cc >> 8) & 0xff));
	    }

	    reg = ad_read(sc, SP_INTERFACE_CONFIG);
	    ad_write(sc, SP_INTERFACE_CONFIG, (CAPTURE_ENABLE|reg));

	    sc->sc_lastcc = cc;
	    sc->sc_mode = AUMODE_RECORD;
#ifdef AUDIO_DEBUG
	    if (ad1848debug > 1)
		    printf("ad1848_dma_input: started capture\n");
#endif
	}
    case PCMRUNNING:
	break;
    }
    
    return 0;
}

int
ad1848_dma_init_output(addr, buf, cc)
    void *addr;
    void *buf;
    int cc;
{
    struct ad1848_softc *sc = addr;

    sc->sc_playrun = DMARUNNING;
    sc->sc_dma_flags = DMAMODE_WRITE | DMAMODE_LOOP;
    sc->sc_dma_bp = buf;
    sc->sc_dma_cnt = cc;
    isa_dmastart(sc->sc_isa, sc->sc_drq, buf, cc, NULL,
		 sc->sc_dma_flags, BUS_DMA_NOWAIT);
    DPRINTF(("ad1848_dma_init_output: %p %d\n", buf, cc));
    return 0;
}

int
ad1848_dma_output(addr, p, cc, intr, arg)
    void *addr;
    void *p;
    int cc;
    void (*intr) __P((void *));
    void *arg;
{
    struct ad1848_softc *sc = addr;
    u_char reg;
    
    if (sc->sc_locked) {
	DPRINTF(("ad1848_dma_output: locked\n"));
	return 0;
    }
    
#ifdef AUDIO_DEBUG
    if (ad1848debug > 0)
	printf("ad1848_dma_output: cc=%d at %p 0x%p (0x%p)\n", cc, p, intr, arg);
#endif
    sc->sc_locked = 1;
    sc->sc_intr = intr;
    sc->sc_arg = arg;

    switch (sc->sc_playrun) {
    case NOTRUNNING:
	sc->sc_dma_flags = DMAMODE_WRITE;
	sc->sc_dma_bp = p;
	sc->sc_dma_cnt = cc;
	isa_dmastart(sc->sc_isa, sc->sc_drq, p, cc, NULL,
		     DMAMODE_WRITE, BUS_DMA_NOWAIT);
	goto startpcm;
    case DMARUNNING:
	sc->sc_playrun = PCMRUNNING;
    startpcm:
	if (sc->precision == 16)
	    cc >>= 1;
	if (sc->channels == 2)
	    cc >>= 1;
	cc--;

	if (sc->sc_lastcc != cc || sc->sc_mode != AUMODE_PLAY) {
	    ad_write(sc, SP_LOWER_BASE_COUNT, (u_char)(cc & 0xff));
	    ad_write(sc, SP_UPPER_BASE_COUNT, (u_char)((cc >> 8) & 0xff));

	    reg = ad_read(sc, SP_INTERFACE_CONFIG);
	    ad_write(sc, SP_INTERFACE_CONFIG, (PLAYBACK_ENABLE|reg));

	    sc->sc_lastcc = cc;
	    sc->sc_mode = AUMODE_PLAY;
	}
	break;
    case PCMRUNNING:
	break;
    }
    
    return 0;
}

int
ad1848_intr(arg)
	void *arg;
{
    struct ad1848_softc *sc = arg;
    int retval = 0;
    u_char status;
    
    /* Get intr status */
    status = ADREAD(sc, AD1848_STATUS);
    
#ifdef AUDIO_DEBUG
    if (ad1848debug > 1)
	printf("ad1848_intr: intr=%p status=%x\n", sc->sc_intr, status);
#endif
    sc->sc_locked = 0;
    sc->sc_interrupts++;
    
    /* Handle interrupt */
    if (sc->sc_intr && (status & INTERRUPT_STATUS)) {
	/* ACK DMA read because it may be in a bounce buffer */
	/* XXX Do write to mask DMA ? */
	if ((sc->sc_dma_flags & DMAMODE_READ) && sc->sc_recrun == NOTRUNNING)
	    isa_dmadone(sc->sc_isa, sc->sc_recdrq);
	(*sc->sc_intr)(sc->sc_arg);
	retval = 1;
    }

    /* clear interrupt */
    if (status & INTERRUPT_STATUS)
	ADWRITE(sc, AD1848_STATUS, 0);

    return(retval);
}

void *
ad1848_malloc(addr, size, pool, flags)
	void *addr;
	unsigned long size;
	int pool;
	int flags;
{
	struct ad1848_softc *sc = addr;

	return isa_malloc(sc->sc_isa, 4, size, pool, flags);
}

void
ad1848_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	isa_free(ptr, pool);
}

unsigned long
ad1848_round(addr, size)
	void *addr;
	unsigned long size;
{
	if (size > MAX_ISADMA)
		size = MAX_ISADMA;
	return size;
}

int
ad1848_mappage(addr, mem, off, prot)
	void *addr;
        void *mem;
        int off;
	int prot;
{
	return isa_mappage(mem, off, prot);
}

int
ad1848_get_props(addr)
	void *addr;
{
	struct ad1848_softc *sc = addr;

	return AUDIO_PROP_MMAP |
	       (sc->sc_drq != sc->sc_recdrq ? AUDIO_PROP_FULLDUPLEX : 0);
}
