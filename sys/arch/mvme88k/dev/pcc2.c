#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/pcc2.h>

int	m1x7pccprobe(struct device *parent, struct cfdata *self, void *aux);
void	m1x7pccattach(struct device *parent, struct device *self, void *aux);

int abort_handler();
extern void abort_intrv();
extern void pcc_intrv();
extern int intrh_debug;
extern int machineid;
extern void badtrap();
extern int submatch( struct device *parent, struct cfdata *self, void *aux);
/* static */ u_int *pcc_io_base;
static u_int *pcc_vector_base;

static void abort_setup();
void timer2_intr();

struct pcctwosoftc {
	struct device	sc_dev;
	caddr_t		sc_vaddr;
	caddr_t		sc_paddr;
	struct pcctworeg *sc_pcc2;
};

void pcctwoattach __P((struct device *, struct device *, void *));
int  pcctwoprobe __P((struct device *, void *, void *));
int  pcctwoabort __P((struct frame *));

struct cfdriver pcctwocd = {
	NULL, "pcctwo", pcctwomatch, pcctwoattach,
	DV_DULL, sizeof(struct pcctwosoftc), 0
};

struct pcctworeg *sys_pcc2 = NULL;

int
pcctwomatch(struct device *parent, struct cfdata *self, void *aux)
{
#if defined(__m88k__)
	if (machineid == 0x187) {
		return 1;
	}
#endif
	return 0;
}

void
pcctwoattach(struct device *parent, struct device *self, void *aux)
{
	struct cfdata *cf;
	volatile char *ibvr;	/* Interrupt Base Vector Register */
	u_int ibv;		/* Interrupt Base Vector, offset from vbr */
	u_int *iv;		/* interupt vector */
	int i;
	u_int vector_base;

	/* attach memory mapped io space */
	/* map 0xfffe1000 - 0xfffe102f, 0xfffe2800 */
	/* ppc_io_base = mmio(0xfffe1000, 1800, PG_RW|PG_CI); */
	pcc_io_base = 0xfffe1000; /* should really be return of virtmem alloc */
	/* set PCC vector base */
	ibv = PCC_IBVR(pcc_io_base) & 0xf0;
	ibvr = &PCC_IBVR(pcc_io_base);
	printf("pcc:ibvr %x *ibvr %x ibv %x\n",ibvr,*ibvr, ibv);
	pcc_vector_base =  (u_int *)ibv;
	asm volatile ("movec vbr,%0": "=d" (vector_base));
	printf("pcc:vector_base %x\n",vector_base);
	/* register "standard interupt handlers */

	abort_setup();
	iv = (u_int *)(vector_base + (ibv * 4));
printf("iv %x\n",iv);
	for (i = 0; i <= SOFT2_VECTOR; i++) {
		iv[i] = (u_int)pcc_intrv;
	}
	/*
	timer2_setup();
	*/
	iv = (u_int *)(vector_base + (ibv + TICK2_VECTOR) * 4);
	*iv = (u_int)&pcc_intrv;

#ifdef DEBUG
	if (intrh_debug)
		pr_intrh();
#endif
	if ((cf = config_search(submatch, self, aux)) != NULL) {
		return;
	}
	return ;
}
asm ("	.text");
asm ("	.global _pcc_intrv");
asm ("_pcc_intrv:");
asm ("	link a6,#0");
asm ("	movml a0/a1/d0/d1,sp@-");
asm ("	movel a6,a0");
asm ("	addql #4,a0");
asm ("	movel a0,sp@-");
asm ("	jbsr  _pcc_handler");
asm ("	addql #4,sp");
asm ("	movml sp@+,a0/a1/d0/d1");
asm ("	unlk a6");
asm ("	jra rei");

asm ("	.global _abort_intrv");
asm ("_abort_intrv:");
asm ("	movml a0/a1/d0/d1,sp@-");
asm ("	jbsr  _abort_handler");
asm ("	movml sp@+,a0/a1/d0/d1");
asm ("	jra rei");
/* asm ("	.previous"); */

void *m147le_arg;
void
pcc_handler(struct exception_frame *except)
{
	u_int vector;
	int handled = 0;

#if 0
	printf("except %x\n",except);
	printf("sr %x\n",except->sr);
	printf("pc %x\n",except->pc);
	printf("type %x\n",except->type);
#endif
	vector = except->vo;
/*	printf("vector %x\n",vector); */
	vector = (vector/4 - (u_int)pcc_vector_base);
/*	printf("vector %x\n",vector); */

	switch (vector) {
	case AC_FAIL_VECTOR:
		printf("ac_fail vector\n");
		break;
	case BERR_VECTOR:
		printf("berr vector\n");
		printf("pcc_handler:invalid vector %x\n",vector);
		break;
	case ABORT_VECTOR:
		printf("abort vector\n");
		abort_handler();
		handled = 1;
		break;
	case SERIAL_VECTOR:
		printf("serial vector\n");
		PCC_SERIAL_ICR(0xfffe1000) = 0;
		break;
	case LANCE_VECTOR:
		leintr(m147le_arg);
		handled = 1;
		break;
	case SCSIPORT_VECTOR:
		printf("scsiport vector\n");
		m147sc_scintr();
		break;
	case SCSIDMA_VECTOR:
		printf("scsidma vector\n");
		m147sc_dmaintr();
		break;
	case PRINTER_VECTOR:
		printf("printer vector\n");
		break;
	case TICK1_VECTOR:
		printf("tick1 vector\n");
		printf("pcc_handler:invalid vector %x\n",vector);
		break;
	case TICK2_VECTOR:
		timer2_intr(except);
		handled = 1;
		break;
	case SOFT1_VECTOR:
		printf("soft1 vector\n");
		break;
	case SOFT2_VECTOR:
		printf("soft2 vector\n");
		break;
	default:
		printf("pcc_handler:invalid vector %x\n",vector);
	}
	
	if (handled == 0) {
		printf("except %x\n",except);
		printf("sr %x\n",except->sr);
		printf("pc %x\n",except->pc);
		printf("type %x\n",except->type);
	}
}


int
abort_handler()
{
	printf("aicr = 0x%x\n",PCC_ABRT_ICR(pcc_io_base));
	PCC_ABRT_ICR(pcc_io_base) = 0x88;
	printf("aicr = 0x%x\n",PCC_ABRT_ICR(pcc_io_base));
	Debugger();
	return 0;
}
static void abort_setup()
{
	printf("PCC_ABRT_ICR %x\n",&PCC_ABRT_ICR(pcc_io_base));
	printf("aicr = 0x%x\n",PCC_ABRT_ICR(pcc_io_base));
	PCC_ABRT_ICR(pcc_io_base) = 0x88;
	printf("aicr = 0x%x\n",PCC_ABRT_ICR(pcc_io_base));
}

/* timer2 (clock) driver */

/*const u_int timer_reload = 0;  /* .4096 sec ? */
/* const u_int timer_reload = 62870;  1/60 sec ? */
const u_int timer_reload = 63936;  /* 1/100 sec ? */

#if 0
void
timer2_setup()
{
	u_int *io_base;
	pcc_io_base = 0xfffe1000; /* should really be return of virtmem alloc */
	io_base = pcc_io_base;
	printf("pcc_io_base %x io_base %x\n",pcc_io_base, io_base);
	printf("PCC_TIMER2_PRE %x\n",&PCC_TIMER2_PRE(io_base));
	printf("PCC_TIMER2_CTR %x\n",&PCC_TIMER2_CTR(io_base));
	printf("PCC_TIMER2_ICR %x\n",&PCC_TIMER2_ICR(io_base));
	PCC_TIMER2_PRE(io_base) = timer_reload;
	PCC_TIMER2_CTR(io_base) = 0x7;
	PCC_TIMER2_ICR(io_base) = 0x8e;
}
#endif
void
timer2_intr(struct exception_frame *except)
{
	u_int *io_base;
	pcc_io_base = 0xfffe1000; /* should really be return of virtmem alloc */
	io_base = pcc_io_base;

	if (0x80 && PCC_TIMER2_ICR(io_base)) {
		PCC_TIMER2_ICR(io_base) = 0x8e;
		/* hardclock(); */
		hardclock(except);

	} else {
		printf("timer2_intr: vector called without interrupt\n");
	}
	/* REALLY UGLY  HACK */
	bugtty_chkinput();

	return;
}
