#define CACHELINESIZE   32                      /* For now              XXX */

void
syncicache(from, len)  
	void *from;
	int len;
{
	int l = len;
	void *p = from;

	do {
		asm volatile ("dcbst 0,%0" :: "r"(p));
		p += CACHELINESIZE;
	} while ((l -= CACHELINESIZE) > 0);
	asm volatile ("sync");
	do {
		asm volatile ("icbi 0,%0" :: "r"(from));
		from += CACHELINESIZE;
	} while ((len -= CACHELINESIZE) > 0);
	asm volatile ("isync");
}

