/*	$OpenBSD: leds.c,v 1.5 1997/02/14 20:35:02 kstailey Exp $	*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/uio.h>

#include <machine/psl.h>

#include "ledsvar.h"

#define MAXPVLEN 10240
#define MAXCDOWN 128

static volatile unsigned char pattern[MAXPVLEN] = {
	0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80,
	0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f
};

volatile unsigned int led_n_patterns = 16;
volatile const unsigned char * volatile led_patterns = &pattern[0];
volatile unsigned int led_countmax = 5;
volatile unsigned int led_countdown = 0;
volatile unsigned int led_px = 0;

int
ledrw(uio)
	struct uio *uio;
{
	unsigned int v[2];
	int s;
	unsigned int o;
	int err;

	if (uio->uio_offset > MAXPVLEN+sizeof(v))
		return(0);
	s = splhigh();
	v[0] = led_countmax;
	v[1] = led_n_patterns;
	splx(s);
	o = uio->uio_offset;
	if (o < sizeof(v)) {
		err = uiomove(((caddr_t)&v[0])+o, sizeof(v)-o, uio);
		if (err)
			return(err);
		o = sizeof(v);
		if (uio->uio_rw == UIO_WRITE) {
			if ((v[0] > MAXCDOWN) ||
			    (v[1] < 1) || (v[1] > MAXPVLEN))
				return(EIO);
			s = splhigh();
			led_countmax = v[0];
			led_n_patterns = v[1];
			led_countdown = 0;
			led_px = 0;
			splx(s);
		}
	}
	o -= sizeof(v);
	if (o >= v[1])
		return(0);
	if (uio->uio_resid > 0) {
		err = uiomove((caddr_t)&pattern[o], v[1]-o, uio);
		if (err)
			return(err);
	}
	return(0);
}
