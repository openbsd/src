/*	$OpenBSD: ledsvar.h,v 1.1 1997/01/13 00:29:25 kstailey Exp $	*/

extern volatile unsigned int led_n_patterns;
extern volatile unsigned int led_countmax;
extern volatile const unsigned char * volatile led_patterns;
extern volatile unsigned int led_countdown;
extern volatile unsigned int led_px;
