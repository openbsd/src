/*	$OpenBSD: pnpinfo.c,v 1.1.1.1 1996/08/11 15:48:55 shawn Exp $	*/
/*
 * Copyright (c) 1996, Sujal M. Patel
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
 */

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <machine/cpufunc.h>

#if defined(__OpenBSD__)
#include <i386/sysarch.h>
#include <machine/pio.h>
#endif

#include "pnpinfo.h"


#define SEND(d, r)	{ outb (ADDRESS, d); outb (WRITE_DATA, r); }


/* The READ_DATA port that we are using currently */
static int rd_port;


void DELAY __P((int i));
int power __P((int base, int exp));
void send_Initiation_LFSR();
int get_serial __P((unsigned char *data));
int get_resource_info __P((char *buffer, int len));
int handle_small_res __P((unsigned char *resinfo, int item, int len));
void handle_large_res __P((unsigned char *resinfo, int item, int len));
void dump_resdata __P((unsigned char *data, int csn));
int isolation_protocol();


/*
 * DELAY does accurate delaying in user-space.
 * This function busy-waits.
 */
void
DELAY (i)
	int i;
{
	struct timeval t;
	long start, stop;

	gettimeofday (&t, NULL);
	start = t.tv_sec * 1000000 + t.tv_usec;
	do {
		gettimeofday (&t, NULL);
		stop = t.tv_sec * 1000000 + t.tv_usec;
	} while (start + i > stop);
}


int
power(base, exp)
	int base, exp;
{
	if (exp <= 1)
		return base;
	else
		return base * power(base, exp - 1);
}


/*
 * Send Initiation LFSR as described in "Plug and Play ISA Specification,
 * Intel May 94."
 */
void
send_Initiation_LFSR()
{
	int cur, i;

	/* Reset the LSFR */
	outb(ADDRESS, 0);
	outb(ADDRESS, 0);

	cur = 0x6a;
	outb(ADDRESS, cur);

	for (i = 1; i < 32; i++) {
		cur = (cur >> 1) | (((cur ^ (cur >> 1)) << 7) & 0xff);
		outb(ADDRESS, cur);
	}
}


/*
 * Get the device's serial number.  Returns 1 if the serial is valid.
 */
int
get_serial(data)
	unsigned char *data;
{
	int i, bit, valid = 0, sum = 0x6a;

	bzero(data, sizeof(char) * 9);

	for (i = 0; i < 72; i++) {
		bit = inb((rd_port << 2) | 0x3) == 0x55;
		DELAY(250);	/* Delay 250 usec */

		/* Can't Short Circuit the next evaluation, so 'and' is last */
		bit = (inb((rd_port << 2) | 0x3) == 0xaa) && bit;
		DELAY(250);	/* Delay 250 usec */

		valid = valid || bit;

		if (i < 64)
			sum = (sum >> 1) |
			    (((sum ^ (sum >> 1) ^ bit) << 7) & 0xff);

		data[i / 8] = (data[i / 8] >> 1) | (bit ? 0x80 : 0);
	}

	valid = valid && (data[8] == sum);

	return valid;
}


/*
 * Fill's the buffer with resource info from the device.
 * Returns 0 if the device fails to report
 */
int
get_resource_info(buffer, len)
	char *buffer;
	int len;
{
	int i, j;

	for (i = 0; i < len; i++) {
		outb(ADDRESS, STATUS);
		for (j = 0; j < 100; j++) {
			if ((inb((rd_port << 2) | 0x3)) & 0x1)
				break;
			DELAY(1);
		}
		if (j == 100) {
			printf("PnP device failed to report resource data\n");
			return 0;
		}
		outb(ADDRESS, RESOURCE_DATA);
		buffer[i] = inb((rd_port << 2) | 0x3);
	}
	return 1;
}


void
report_dma_info (x)
	int x;
{
	switch (x & 0x3) {
	case 0:
		printf ("DMA: 8-bit only\n");
		break;
	case 1:
		printf ("DMA: 8-bit and 16-bit\n");
		break;
	case 2:
		printf ("DMA: 16-bit only\n");
		break;
#ifdef DIAGNOSTIC
	case 3:
		printf ("DMA: Reserved\n");
		break;
#endif
	}

	if (x & 0x4)
		printf ("DMA: Device is a bus master\n");
	else
		printf ("DMA: Device is not a bus master\n");

	if (x & 0x8)
		printf ("DMA: May execute in count by byte mode\n");
	else
		printf ("DMA: May not execute in count by byte mode\n");

	if (x & 0x10)
		printf ("DMA: May execute in count by word mode\n");
	else
		printf ("DMA: May not execute in count by word mode\n");

	switch ((x & 0x60) >> 5) {
	case 0:
		printf ("DMA: Compatibility mode\n");
		break;
	case 1:
		printf ("DMA: Type A DMA channel\n");
		break;
	case 2:
		printf ("DMA: Type B DMA channel\n");
		break;
	case 3:
		printf ("DMA: Type F DMA channel\n");
		break;
	}
}


void
report_memory_info (x)
	int x;
{
	if (x & 0x1)
		printf ("Memory Range: Writeable\n");
	else
		printf ("Memory Range: Not writeable (ROM)\n");

	if (x & 0x2)
		printf ("Memory Range: Read-cacheable, write-through\n");
	else
		printf ("Memory Range: Non-cacheable\n");

	if (x & 0x4)
		printf ("Memory Range: Decode supports high address\n");
	else
		printf ("Memory Range: Decode supports range length\n");

	switch ((x & 0x18) >> 3) {
	case 0:
		printf ("Memory Range: 8-bit memory only\n");
		break;
	case 1:
		printf ("Memory Range: 16-bit memory only\n");
		break;
	case 2:
		printf ("Memory Range: 8-bit and 16-bit memory supported\n");
		break;
#ifdef DIAGNOSTIC
	case 3:
		printf ("Memory Range: Reserved\n");
		break;
#endif
	}

	if (x & 0x20)
		printf ("Memory Range: Memory is shadowable\n");
	else
		printf ("Memory Range: Memory is not shadowable\n");

	if (x & 0x40)
		printf ("Memory Range: Memory is an expansion ROM\n");
	else
		printf ("Memory Range: Memory is not an expansion ROM\n");

#ifdef DIAGNOSTIC
	if (x & 0x80)
		printf ("Memory Range: Reserved (Device is brain-damaged)\n");
#endif
}


/*
 *  Small Resource Tag Handler
 *
 *  Returns 1 if checksum was valid (and an END_TAG was received).
 *  Returns -1 if checksum was invalid (and an END_TAG was received).
 *  Returns 0 for other tags.
 */
int
handle_small_res(resinfo, item, len)
	int item, len;
	unsigned char *resinfo;
{
	int i;

	switch (item) {
	case PNP_VERSION:
		printf("PnP Version: %d.%d\n",
		    resinfo[0] >> 4,
		    resinfo[0] & (0xf));
		printf("Vendor Version: %d\n", resinfo[1]);
		break;
	case LOG_DEVICE_ID:
		printf("Logical Device ID: %c%c%c%02x%02x (%08x)\n",
		    ((resinfo[0] & 0x7c) >> 2) + 64,
		    (((resinfo[0] & 0x03) << 3) |
			((resinfo[1] & 0xe0) >> 5)) + 64,
		    (resinfo[1] & 0x1f) + 64,
		    resinfo[2], resinfo[3], *(int *)resinfo);
	
		if (resinfo[4] & 0x1)
			printf ("Device powers up active\n");
		if (resinfo[4] & 0x2)
			printf ("Device supports I/O Range Check\n");
		if (resinfo[4] > 0x3)
			printf ("Reserved register funcs %02x\n",
				resinfo[4]);

		if (len == 6)
			printf("Vendor register funcs %02x\n", resinfo[5]);
		break;
	case COMP_DEVICE_ID:
		printf("Compatible Device ID: %c%c%c%02x%02x (%08x)\n",
		    ((resinfo[0] & 0x7c) >> 2) + 64,
		    (((resinfo[0] & 0x03) << 3) |
			((resinfo[1] & 0xe0) >> 5)) + 64,
		    (resinfo[1] & 0x1f) + 64,
		    resinfo[2], resinfo[3], *(int *)resinfo);
		break;
	case IRQ_FORMAT:
		printf("IRQ: ");

		for (i = 0; i < 8; i++)
			if (resinfo[0] & (char) (power(2, i)))
				printf("%d ", i);
		for (i = 0; i < 8; i++)
			if (resinfo[1] & (char) (power(2, i)))
				printf("%d ", i + 8);
		printf("\n");
		if (len == 3) {
			if (resinfo[2] & 0x1)
				printf("IRQ: High true edge sensitive\n");
			if (resinfo[2] & 0x2)
				printf("IRQ: Low true edge sensitive\n");
			if (resinfo[2] & 0x4)
				printf("IRQ: High true level sensitive\n");
			if (resinfo[2] & 0x8)
				printf("IRQ: Low true level sensitive\n");
		}
		break;
	case DMA_FORMAT:
		printf("DMA: ");
		for (i = 0; i < 8; i++)
			if (resinfo[0] & (char) (power(2, i)))
				printf("%d ", i);
		printf ("\n");
		report_dma_info (resinfo[1]);
		break;
	case START_DEPEND_FUNC:
		printf("Start Dependent Function\n");
		if (len == 1) {
			switch (resinfo[0]) {
			case 0:
				printf("Good Configuration\n");
				break;
			case 1:
				printf("Acceptable Configuration\n");
				break;
			case 2:
				printf("Sub-optimal Configuration\n");
				break;
			}
		}
		break;
	case END_DEPEND_FUNC:
		printf("End Dependent Function\n");
		break;
	case IO_PORT_DESC:
		if (resinfo[0])
			printf("Device decodes the full 16-bit ISA address\n");
		else
			printf("Device does not decode the full 16-bit ISA address\n");
		printf("I/O Range maximum address: 0x%x\n",
		    resinfo[1] + (resinfo[2] << 8));
		printf("I/O Range maximum address: 0x%x\n",
		    resinfo[3] + (resinfo[4] << 8));
		printf("I/O alignment for minimum: %d\n",
		    resinfo[5]);
		printf("I/O length: %d\n", resinfo[6]);
		break;
	case FIXED_IO_PORT_DESC:
		printf ("I/O Range base address: 0x%x\n",
		    resinfo[1] + (resinfo[2] << 8));
		printf("I/O length: %d\n", resinfo[3]);
		break;
#ifdef DIAGNOSTIC
	case SM_RES_RESERVED:
		printf("Reserved Tag Detected\n");
		break;
#endif
	case SM_VENDOR_DEFINED:
		printf("*** Small Vendor Tag Detected\n");
		break;
	case END_TAG:
		printf("End Tag\n\n");
		/* XXX Record and Verify Checksum */
		return 1;
		break;
	}
	return 0;
}


void
handle_large_res(resinfo, item, len)
	int item, len;
	unsigned char *resinfo;
{
	int i;

	switch (item) {
	case MEMORY_RANGE_DESC:
		report_memory_info(resinfo[0]);
		printf("Memory range minimum address: 0x%x\n",
		    (resinfo[1] << 8) + (resinfo[2] << 16));
		printf("Memory range maximum address: 0x%x\n",
		    (resinfo[3] << 8) + (resinfo[4] << 16));
		printf("Memory range base alignment: 0x%x\n",
		    (i = (resinfo[5] + (resinfo[6] << 8))) ? i : (1 << 16));
		printf("Memory range length: 0x%x\n",
		    (resinfo[7] + (resinfo[8] << 8)) * 256);
		break;
	case ID_STRING_ANSI:
		printf("Device Description: ");

		for (i = 0; i < len; i++) {
			printf("%c", resinfo[i]);
		}
		printf("\n");
		break;
	case ID_STRING_UNICODE:
		printf("ID String Unicode Detected (Undefined)\n");
		break;
	case LG_VENDOR_DEFINED:
		printf("Large Vendor Defined Detected\n");
		break;
	case _32BIT_MEM_RANGE_DESC:
		printf("32bit Memory Range Desc Unimplemented\n");
		break;
	case _32BIT_FIXED_LOC_DESC:
		printf("32bit Fixed Location Desc Unimplemented\n");
		break;
	case LG_RES_RESERVED:
		printf("Large Reserved Tag Detected\n");
		break;
	}
}


/*
 * Dump all the information about configurations.
 */
void
dump_resdata(data, csn)
	unsigned char *data;
	int csn;
{
	int i, large_len;
	unsigned char tag, *resinfo;

#ifdef DEBUG
	printf("Card assigned CSN #%d\n", csn);
#endif
	printf("Board Vendor ID: %c%c%c%02x%02x\n",
	    ((data[0] & 0x7c) >> 2) + 64,
	    (((data[0] & 0x03) << 3) | ((data[1] & 0xe0) >> 5)) + 64,
	    (data[1] & 0x1f) + 64, data[2], data[3]);
	printf("Board Serial Number: %08x\n", *(int *)&(data[4]));

	SEND(SET_CSN, csn); /* Move this out of this function XXX */
	outb(ADDRESS, STATUS);

	/* Allows up to 1kb of Resource Info,  Should be plenty */
	for (i = 0; i < 1024; i++) {
		if (!get_resource_info(&tag, 1))
			return;

#define TYPE	(tag >> 7)
#define	S_ITEM	(tag >> 3)
#define S_LEN	(tag & 0x7)
#define	L_ITEM	(tag & 0x7f)

		if (TYPE == 0) {
			/* Handle small resouce data types */

			resinfo = malloc(S_LEN);
			if (!get_resource_info(resinfo, S_LEN))
				return;

			if (handle_small_res(resinfo, S_ITEM, S_LEN) == 1)
				return;
			free(resinfo);
		} else {
			/* Handle large resouce data types */

			if (!get_resource_info((char *) &large_len, 2))
				return;

			resinfo = malloc(large_len);
			if (!get_resource_info(resinfo, large_len))
				return;

			handle_large_res(resinfo, L_ITEM, large_len);
			free(resinfo);
		}
	}
}


/*
 * Run the isolation protocol. Use rd_port as the READ_DATA port value (caller
 * should try multiple READ_DATA locations before giving up). Upon exiting,
 * all cards are aware that they should use rd_port as the READ_DATA port;
 */
int
isolation_protocol()
{
	int csn;
	unsigned char data[9];

	send_Initiation_LFSR();

	/* Reset CSN for All Cards */
	SEND(0x02, 0x04);

	for (csn = 1; (csn < MAX_CARDS); csn++) {
		/* Wake up cards without a CSN */
		SEND(WAKE, 0);
		SEND(SET_RD_DATA, rd_port);
		outb(ADDRESS, SERIAL_ISOLATION);
		DELAY(1000);	/* Delay 1 msec */

		if (get_serial(data))
			dump_resdata(data, csn);
		else
			break;
	}
	return csn - 1;
}


void
main()
{
	int num_pnp_devs;

#if defined(__OpenBSD__)
	if (i386_iopl(1)) {
		perror("i386_iopl");
		exit(1);
  	}
#endif
	printf("Checking for Plug-n-Play devices...\n");

	/* Try various READ_DATA ports from 0x203-0x3ff */
	for (rd_port = 0x80; (rd_port < 0xff); rd_port += 0x10) {
#ifdef DEBUG
		printf("Trying Read_Port at %x\n", (rd_port << 2) | 0x3);
#endif
		num_pnp_devs = isolation_protocol(rd_port);
		if (num_pnp_devs)
			break;
	}
	if (!num_pnp_devs) {
		printf("No Plug-n-Play devices were found\n");
		return;
	}
}
