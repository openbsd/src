/*	$OpenBSD: dvma.h,v 1.2 2001/07/04 08:33:49 niklas Exp $	*/


void dvma_init();

char * dvma_mapin(char *pkt, int len);
void dvma_mapout(char *dmabuf, int len);

char * dvma_alloc(int len);
void dvma_free(char *dvma, int len);

