/* $OpenBSD: memprobe.c,v 1.1 1997/03/31 03:12:14 weingart Exp $ */

#include <sys/param.h>
#include <libsa.h>


/* addrprobe(kloc): Probe memory at address kloc * 1024.
 *
 * This is a hack, but it seems to work ok.  Maybe this is
 * the *real* way that you are supposed to do probing???
 */
static int addrprobe(int kloc){
	volatile int *loc, i;
	static int pat[] = {
		0x00000000, 0xFFFFFFFF,
		0x01010101, 0x10101010,
		0x55555555, 0xCCCCCCCC
	};

	/* Get location */
	loc = (int *)(kloc * 1024);

	/* Probe address */
	for(i = 0; i < sizeof(pat)/sizeof(pat[0]); i++){
		*loc = pat[i];
		if(*loc != pat[i]) return(1);
	}

	return(0);
}


/* memprobe():  return probed memory size in KB for extended memory
 *
 * There is no need to do this in assembly language.  This are
 * much easier to debug in C anyways.
 */
int memprobe(void){
	int ram;

	for(ram = 1024; ram < 512*1024; ram += 4){

		printf("Probing memory: %d KB\r", ram-1024);
		if(addrprobe(ram)) break;
	}

	printf("\n");
	return(ram-1024);
}

