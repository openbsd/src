/*	$OpenBSD: dvma.h,v 1.2 1996/04/28 10:49:03 deraadt Exp $ */

char * dvma_mapin(char *pkt, int len);
void dvma_mapout(char *dmabuf, int len);

char * dvma_alloc(int len);

