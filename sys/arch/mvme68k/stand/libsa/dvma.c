
/*
 * The easiest way to deal with the need for DVMA mappings is
 * to just map the first four megabytes of RAM into DVMA space.
 * That way, dvma_mapin can just compute the DVMA alias address,
 * and dvma_mapout does nothing.
 */

#include <sys/param.h>

#define	DVMA_BASE 0x00000000
#define	DVMA_MASK 0x00ffFFff
#define DVMA_MAPLEN 0x400000	/* 4 MB */

void
dvma_init()
{
#if 0
	int segva, sme;

	for (segva = 0; segva < DVMA_MAPLEN; segva += NBSG) {
		sme = get_segmap(segva);
		set_segmap((DVMA_BASE | segva), sme);
	}
#endif
}

/* Convert a local address to a DVMA address. */
char *
dvma_mapin(char *addr, int len)
{
	int va = (int)addr;

	va |= DVMA_BASE;
	return ((char *) va);
}

/* Convert a DVMA address to a local address. */
char *
dvma_mapout(char *dmabuf, int len)
{
	if (dmabuf < (char*)DVMA_BASE)
		panic("dvma_mapout");
	return (dmabuf - DVMA_BASE);
}

extern char *alloc(int len);
char *
dvma_alloc(int len)
{
	char *mem;

	mem = alloc(len);
	if (!mem)
		return(mem);
	return(dvma_mapin(mem, len));
}

extern void free(void *ptr, int len);
void
dvma_free(char *dvma, int len)
{
	char *mem;

	mem = dvma_mapout(dvma, len);
	if (mem)
		free(mem, len);
}
