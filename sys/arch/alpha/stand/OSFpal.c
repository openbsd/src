
#include <sys/types.h>

#include <machine/prom.h>
#include <machine/rpb.h>

void
OSFpal()
{
	struct rpb *r;
	struct ctb *t;
	struct pcs *p;
	long result;
	int offset;

	r = (struct rpb *)HWRPB_ADDR;
	offset = r->rpb_pcs_size * cpu_number();
	p = (struct pcs *)((u_int8_t *)r + r->rpb_pcs_off + offset);

	printf("VMS PAL revision: 0x%lx\n",
	    p->pcs_palrevisions[PALvar_OpenVMS]);
	printf("OSF PAL rev: 0x%lx\n", p->pcs_palrevisions[PALvar_OSF1]);
	(void)switch_palcode();
	printf("Switch to OSF PAL code succeeded.\n");
}

