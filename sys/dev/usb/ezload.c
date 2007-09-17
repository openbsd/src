/*	$OpenBSD: ezload.c,v 1.9 2007/09/17 01:40:38 fgsch Exp $ */
/*	$NetBSD: ezload.c,v 1.5 2002/07/11 21:14:25 augustss Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by  Lennart Augustsson <lennart@augustsson.net>.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/ezload.h>

/*
 * Vendor specific request code for Anchor Upload/Download
 */

/* This one is implemented in the core */
#define ANCHOR_LOAD_INTERNAL	0xA0

/* This is the highest internal RAM address for the AN2131Q */
#define ANCHOR_MAX_INTERNAL_ADDRESS  0x1B3F

/*
 * EZ-USB Control and Status Register.  Bit 0 controls 8051 reset
 */
#define ANCHOR_CPUCS_REG	0x7F92
#define  ANCHOR_RESET		0x01

/*
 * Although USB does not limit you here, the Anchor docs
 * quote 64 as a limit, and Mato@activewireinc.com suggested
 * to use 16.
 */
#define ANCHOR_CHUNK 16

/*
 * This is a firmware loader for ezusb (Anchor) devices. When the firmware
 * has been downloaded the device will simulate a disconnect and when it
 * is next recognized by the USB software it will appear as another
 * device.
 */

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ezloaddebug) printf x
#define DPRINTFN(n,x)	if (ezloaddebug>(n)) printf x
int ezloaddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

usbd_status
ezload_reset(usbd_device_handle dev, int reset)
{
	usb_device_request_t req;
	uByte rst;

	DPRINTF(("ezload_reset: reset=%d\n", reset));

	rst = reset ? ANCHOR_RESET : 0;
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ANCHOR_LOAD_INTERNAL;
	USETW(req.wValue, ANCHOR_CPUCS_REG);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	return (usbd_do_request(dev, &req, &rst));
}

usbd_status
ezload_download(usbd_device_handle dev, const char *name, const u_char *buf,
    size_t buflen)
{
	usb_device_request_t req;
	usbd_status err = 0;
	u_int8_t length;
	u_int16_t address;
	u_int len, offs;

	for (;;) {
		length = *buf++;
		if (length == 0)
			break;

		address = UGETW(buf); buf += 2;
#if 0
		if (address + length > ANCHOR_MAX_INTERNAL_ADDRESS)
			return (USBD_INVAL);
#endif

		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
		req.bRequest = ANCHOR_LOAD_INTERNAL;
		USETW(req.wIndex, 0);
		for (offs = 0; offs < length; offs += ANCHOR_CHUNK) {
			len = length - offs;
			if (len > ANCHOR_CHUNK)
				len = ANCHOR_CHUNK;
			USETW(req.wValue, address + offs);
			USETW(req.wLength, len);
			DPRINTFN(2,("ezload_download: addr=0x%x len=%d\n",
				    address + offs, len));
			err = usbd_do_request(dev, &req, (u_char *)buf);
			if (err)
				break;

			buf += len;
		}
		if (err)
			break;
	}

	return (err);
}

usbd_status
ezload_downloads_and_reset(usbd_device_handle dev, char **names)
{
	usbd_status err;
	size_t buflen;
	u_char *buf;
	int error;

	/*(void)ezload_reset(dev, 1);*/
	err = ezload_reset(dev, 1);
	if (err)
		return (err);
	usbd_delay_ms(dev, 250);

	while (*names != NULL) {
		error = loadfirmware(*names, &buf, &buflen);
		if (error)
			return (error);

		err = ezload_download(dev, *names, buf, buflen);
		free(buf, M_DEVBUF);
		if (err)
			return (err);
		names++;
	}
	if (err)
		return (err);
	usbd_delay_ms(dev, 250);
	/*(void)ezload_reset(dev, 0);*/
	err = ezload_reset(dev, 0);
	usbd_delay_ms(dev, 250);
	return (err);
}
