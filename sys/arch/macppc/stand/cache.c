/*	$OpenBSD: cache.c,v 1.2 2003/10/16 04:30:09 drahn Exp $	*/
#define CACHELINESIZE   32                      /* For now              XXX */

void
syncicache(void *from, int len)
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
