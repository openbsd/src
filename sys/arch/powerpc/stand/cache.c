/*	$OpenBSD: cache.c,v 1.2 1999/11/09 06:30:15 rahnds Exp $	*/
#define CACHELINESIZE   32                      /* For now              XXX */

void
syncicache(from, len)  
	void *from;
	int len;
{
	int l = len;
	void *p = from;

	do {
		asm volatile ("dcbf %1,%0" :: "r"(p), "r"(0));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	asm volatile ("sync");
	do {
		asm volatile ("icbi %1,%0" :: "r"(from), "r"(0));
		from += CACHELINESIZE;
	} while ((len -= CACHELINESIZE) > 0);
	asm volatile ("isync");
}

