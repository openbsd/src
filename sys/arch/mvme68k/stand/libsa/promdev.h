/*	$OpenBSD: promdev.h,v 1.2 1996/04/28 10:49:14 deraadt Exp $ */

int prom_iopen(struct saioreq **sipp);
void prom_iclose(struct saioreq *sip);

