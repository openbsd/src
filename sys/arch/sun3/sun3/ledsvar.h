/*	$OpenBSD: ledsvar.h,v 1.2 1997/01/16 04:04:26 kstailey Exp $	*/

extern volatile unsigned int led_n_patterns;
extern volatile unsigned int led_countmax;
extern volatile const unsigned char * volatile led_patterns;
extern volatile unsigned int led_countdown;
extern volatile unsigned int led_px;

extern int ledrw __P((struct uio *));
