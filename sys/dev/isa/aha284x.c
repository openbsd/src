/*	$NetBSD: aha284x.c,v 1.2 1996/01/13 02:06:30 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Michael Graff.  All rights reserved.
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
 *      This product includes software developed by Michael Graff.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/isa/isavar.h>

#include <dev/ic/aic7xxxvar.h>

#include <machine/pio.h>

static int ahe_probe __P((struct device *, void *, void *));
static void ahe_attach __P((struct device *, struct device *, void *));

struct cfdriver ahecd = {
        NULL,          /* devices found */
	"ahe",         /* device name */
	ahe_probe,     /* match routine */
	ahe_attach,    /* attach routine */
	DV_DULL,       /* device class */
        sizeof(struct ahc_softc),  /* size of private dev data */
};

/*
 * shouldn't this be in aic7xxxvar.h?
 */
int ahcintr __P((void *));

/*
 * Standard EISA Host ID regs  (Offset from slot base)
 * These seem to work on the aha284x as well (VLB card)
 */
#define HID0           0xC80   /* 0,1: msb of ID2, 2-7: ID1      */
#define HID1           0xC81   /* 0-4: ID3, 5-7: LSB ID2         */
#define HID2           0xC82   /* product                        */
#define HID3           0xC83   /* firmware revision              */

#define CHAR1(B1,B2) (((B1>>2) & 0x1F) | '@')
#define CHAR2(B1,B2) (((B1<<3) & 0x18) | ((B2>>5) & 0x7)|'@')
#define CHAR3(B1,B2) ((B2 & 0x1F) | '@')

typedef struct {
  ahc_type type;
  unsigned char id; /* The Last EISA Host ID reg */
} aic7770_sig;

aic7770_sig valid_ids[] = {
  /* Entries of other tested adaptors should be added here */
  { AHC_274,      0x70 }, /*aic7770 on Motherboard*/
  { AHC_274,      0x71 }, /*274x*/
  { AHC_284,      0x56 }, /*284x, BIOS enabled*/
  { AHC_284,      0x57 }  /*284x, BIOS disabled*/
};

int
ahe_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
        struct ahc_softc *ahc = match;
        struct isa_attach_args *ia = aux;

	char intdef;
	int iobase;
	u_char sig_id[4];
	int i;

#ifdef NEWCONFIG
        if (ia->ia_iobase == IOBASEUNK)
                return 0;
#endif

	/*
         * Make the offsets the same as for EISA
	 * 
	 * I have NO idea why the values in aic7xxx.c are all 0xc00 too
	 * high, but this hack fixes it.  This is the same hack that's in
	 * the 294x pci code.
         */
	iobase = ia->ia_iobase - 0xc00;

	for (i = 0; i < sizeof(sig_id); i++) {
	  /*
	   * An outb is required to prime these
	   * registers on VL cards
	   */
	  outb(iobase + HID0, HID0 + i);
	  sig_id[i] = inb(iobase + HID0 + i);
	}

	if (sig_id[0] == 0xff)
	  return 0;

	if (CHAR1(sig_id[0], sig_id[1]) != 'A'
	    || CHAR2(sig_id[0], sig_id[1]) != 'D'
	    || CHAR3(sig_id[0], sig_id[1]) != 'P'
	    || sig_id[2] != 0x77)
	  return 0;
	
	ahc->type = 0;

	for (i = 0; i < sizeof(valid_ids)/sizeof(aic7770_sig); i++)
	  if (sig_id[3] == valid_ids[i].id) {
	    ahc->type = valid_ids[i].type;
	    break;
	  }
	
	if (ahc->type == 0)
	  printf("%s: Unknown board type 0x%02x\n",
		 ahc->sc_dev.dv_xname, sig_id[3]);

	if (ahcprobe(ahc, iobase) == 0)
	  return 0;

	/*
	 * set up some other isa variables and make certain the irq the
	 * card is set at matches the one in the configuration file,
	 * it is wa defined there
	 */

	ia->ia_iosize = 0x100;  /* address range for the card */

	if (ia->ia_irq == IRQUNK)
		ia->ia_irq = ahc->sc_irq;  /* probed from the card */
	else
		if (ia->ia_irq != ahc->sc_irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			       ahc->sc_dev.dv_xname, ia->ia_irq, ahc->sc_irq);
			return 0;
		}

	/* Must be ok... */
	return 1;
}

void    
ahe_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
        struct ahc_softc *ahc = (void *)self;
        struct isa_attach_args *ia = aux;

#ifdef NEWCONFIG
        isa_establish(&ahc->sc_id, &ahc->sc_dev);
#endif
        ahc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_BIO,
					ahcintr, ahc);

	/*
	 * attach the devices on the bus
	 */
	ahcattach(ahc);
}
