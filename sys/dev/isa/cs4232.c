/*
 * ISA PnP bus autoconfiguration.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>


#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include "cs4232.h"

#ifdef CONFIG_CS4232
int probe_cs4232 __P((void));

/* a note about the CS4232 stuff. its really pretty bad. if you dont have a
 * CS4232 or compatible then you should really (really) turn off CONFIG_CS4232.
 * i dont THINK that this will do any damage, but who knows? its really a 
 * bad fix. (also note, in order for it to work it assumes that you have a
 * io port 534-537 clear as well as irq 5 and dma 0,1 all free.
 * this is all really lousy also. if it crashes your machine, sorry.
 */

/*
 * sound/cs4232.c
 * 
 * The low level driver for Crystal CS4232 based cards. The CS4232 is a PnP
 * compatible chip which contains a CS4231A codec, SB emulation, a MPU401
 * compatible MIDI port, joystick and synthesizer and IDE CD-ROM interfaces.
 * This is just a temporary driver until full PnP support gets inplemented.
 * Just the WSS codec, FM synth and the MIDI ports are supported. Other
 * interfaces are left uninitialized.
 * 
 * Copyright by Hannu Savolainen 1995
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



#define KEY_PORT	0x279	/* Same as LPT1 status port */
#define CSN_NUM		0x01	/* Just a random number */

#define CS_OUT(a) 		outb( KEY_PORT,  a)
#define CS_OUT2(a, b)		{CS_OUT(a);CS_OUT(b);}
#define CS_OUT3(a, b, c)	{CS_OUT(a);CS_OUT(b);CS_OUT(c);}



static unsigned char crystal_key[] =	/* A 32 byte magic key sequence */
{
    0x96, 0x35, 0x9a, 0xcd, 0xe6, 0xf3, 0x79, 0xbc,
    0x5e, 0xaf, 0x57, 0x2b, 0x15, 0x8a, 0xc5, 0xe2,
    0xf1, 0xf8, 0x7c, 0x3e, 0x9f, 0x4f, 0x27, 0x13,
    0x09, 0x84, 0x42, 0xa1, 0xd0, 0x68, 0x34, 0x1a
};

int
probe_cs4232(void)
{
	int             i;

	/*
	 * Verify that the I/O port range is free.
	 */

	if (0) {
		printf("cs4232.c: I/O port 0x534 not free\n");
		return 0;
	}
    /*
     * This version of the driver doesn't use the PnP method when
     * configuring the card but a simplified method defined by Crystal.
     * This means that just one CS4232 compatible device can exist on the
     * system. Also this method conflicts with possible PnP support in
     * the OS. For this reason driver is just a temporary kludge.
     */

   /*
    * Wake up the card by sending a 32 byte Crystal key to the key port.
    */
	for (i = 0; i < 32; i++)
		CS_OUT(crystal_key[i]);

   /*
    * Now set the CSN (Card Select Number).
    */

/*   CS_OUT2(0x06, 0x01); */

   /*
    * Ensure that there is no other codec using the same address.
    */

	CS_OUT2(0x15, 0x00);	/* Select logical device 0 (WSS/SB/FM) */

   /*
    * Then set some config bytes. First logical device 0
    */

	CS_OUT3(0x47, 0x05, 0x34);	/* WSSbase */

	if (0)			/* Not free */
		CS_OUT3(0x48, 0x03, 0x88)	/* FMbase off */
	else
		CS_OUT3(0x48, 0x03, 0x88);	/* FMbase 0x388 */

	CS_OUT3(0x42, 0x02, 0x20);	/* SBbase off */
	CS_OUT2(0x22, 0x05);	/* SB+WSS IRQ */
	CS_OUT2(0x2a, 0x01);	/* SB+WSS DMA */

	if (0)
		CS_OUT2(0x25, 0x00)	/* WSS DMA2 */
	else
		CS_OUT2(0x25, 0x00);	/* No WSS DMA2 */

	CS_OUT2(0x33, 0x01);	/* Activate logical dev 0 */

/*
 * init logical device 2...
 *
 */
	CS_OUT2(0x15,0x02);
	CS_OUT3(0x47,0x01,0x20);
	CS_OUT2(0x33,0x01);


/*
* Initialize logical device 3 (MPU)
*/

#if (defined(CONFIG_MPU401) || defined(CONFIG_MPU_EMU)) && defined(CONFIG_MIDI)
	if (mpu_base != 0 && mpu_irq != 0) {
		CS_OUT2(0x15, 0x03);	/* Select logical device 3 (MPU) */
		CS_OUT3(0x47, (mpu_base >> 8) & 0xff, mpu_base & 0xff);	/* MPUbase */
		CS_OUT2(0x22, mpu_irq);	/* MPU IRQ */
		CS_OUT2(0x33, 0x01);	/* Activate logical dev 3 */
}
#endif

/*
* Finally activate the chip
*/
	CS_OUT(0x79);

/*
* Then try to detect the codec part of the chip
*/
	delay(15000);

	outb(0x534,0x40); /* this seems to be needed... */	
	return 1;
}

#endif
