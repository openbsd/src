#include "bug.h"

asm	("		text");
/*asm	("_stack:	word _stack0xFC0000;	stack");*/
asm	("stack:	word stack");
asm	("		word _start");
asm	("		align 8");

struct bugenv bugenv;
extern char *end, *edata;

start()
{
	register int dlun 	asm("r2");
	register int clun 	asm("r3");
	register int ipl  	asm("r4");
	register int (*entryptr)() asm("r6");
	register int *cfg	asm("r7");
	register char *strstr	asm("r8");
	register char *endstr	asm("r9");
	int i;
	char *str;
	
asm	(";	enable SFU1");
asm	("		ldcr	r10,cr1");
asm	("		xor	r10,r10,0x8");
asm	("		stcr	r10,cr1");

	bugenv.clun = clun;
	bugenv.dlun = dlun;
	bugenv.ipl  = ipl;
	bugenv.entry= entryptr;

	bzero(&edata,((char *)&end - (char *)&edata));
	for (str = strstr, i = 0; str <= strstr; str++, i++) {
		bugenv.bootargs[i] = *str;
	}
	bugenv.bootargs[i] = 0;

	main(&bugenv);	
	bugreturn();
}
