/*	$OpenBSD: bugio.c,v 1.7 2004/11/15 11:01:35 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <machine/bugio.h>
#include <machine/cpu.h>

int bugenvsz(void);

/*
 * BUG register preserving
 */

register_t sprg0, sprg1, sprg2, sprg3;
register_t bugsprg3;

#define	BUGCTXT(msr) \
	do { \
		msr = ppc_mfmsr(); \
		sprg0 = ppc_mfsprg0(); \
		sprg1 = ppc_mfsprg1(); \
		sprg2 = ppc_mfsprg2(); \
		sprg3 = ppc_mfsprg3(); \
		ppc_mtsprg3(bugsprg3); \
        	ppc_mtmsr(((msr | PSL_IP) & ~(PSL_EE | PSL_IR | PSL_DR))); \
	} while (0)

#define	OSCTXT(msr) \
	do { \
		bugsprg3 = ppc_mfsprg3(); \
		ppc_mtsprg0(sprg0); \
		ppc_mtsprg1(sprg1); \
		ppc_mtsprg2(sprg2); \
		ppc_mtsprg3(sprg3); \
        	ppc_mtmsr(msr); \
	} while (0)

/* Invoke the BUG */
#define MVMEPROM_CALL(x)	\
	__asm__ __volatile__ ("addi %r10,%r0," __STRING(x)); \
	__asm__ __volatile__ ("sc");

void
buginit()
{
	bugsprg3 = ppc_mfsprg3();
}


/* BUG - query board routines */
void
mvmeprom_brdid(id)
	struct mvmeprom_brdid *id;
{
	unsigned long msr;

	BUGCTXT(msr);
	MVMEPROM_CALL(MVMEPROM_BRD_ID);
	asm volatile ("mr %0, 3": "=r" (id):);
	OSCTXT(msr);
}

/* returns 0 if no characters ready to read */
int
mvmeprom_getchar()
{
	int ret;
	unsigned long msr;

	BUGCTXT(msr);
	MVMEPROM_CALL(MVMEPROM_INCHR);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	OSCTXT(msr);
	return ret;
}

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	int ret;
	unsigned long msr;

	BUGCTXT(msr);
	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	OSCTXT(msr);
	return (!(ret & 0x4));
}

void
mvmeprom_outln(start, end)
	char *start, *end;
{
	unsigned long msr;

	BUGCTXT(msr);
	asm volatile ("mr 3, %0": : "r" (start));
	asm volatile ("mr 4, %0": : "r" (end));
	MVMEPROM_CALL(MVMEPROM_OUTLN);
	OSCTXT(msr);
}

void
mvmeprom_outstr(start, end)
	char *start, *end;
{
	unsigned long msr;

	BUGCTXT(msr);
	asm volatile ("mr 3, %0": : "r" (start));
	asm volatile ("mr 4, %0": : "r" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
	OSCTXT(msr);
}

void
mvmeprom_outchar(c)
	int c;
{
	unsigned long msr;

	BUGCTXT(msr);
	asm  volatile ("mr 3, %0" :: "r" (c));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
	OSCTXT(msr);
}

/* BUG - return to bug routine */
void
mvmeprom_return()
{
	unsigned long msr;

	BUGCTXT(msr);
	MVMEPROM_CALL(MVMEPROM_RETURN);
	OSCTXT(msr);
	/*NOTREACHED*/
}


void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	unsigned long msr;

	BUGCTXT(msr);
	asm volatile ("mr 3, %0": : "r" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
	OSCTXT(msr);
}

int 
bugenvsz(void)
{
	register int ret;
	char tmp[1];
	void *ptr = tmp;
	unsigned long msr;

	BUGCTXT(msr);
	asm volatile ("mr 3, %0": : "r" (ptr));
	asm volatile ("li 5, 0x1");
	asm volatile ("li 5, 0x0"); /* get size */
	MVMEPROM_CALL(MVMEPROM_ENVIRON);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	OSCTXT(msr);
	
	return(ret);
}

struct bugenviron bugenviron; 
int bugenv_init = 0;
char bugenv_buf[1024];

#ifdef BUG_DEBUG
void bug_printenv(void);

void
bug_printenv(void)
{
	printf("Startup Mode: %c\n", bugenviron.s.s_mode);
	printf("Startup Menu: %c\n", bugenviron.s.s_menu);
	printf("Remote Start: %c\n", bugenviron.s.s_remotestart);
	printf("Probe Devs: %c\n", bugenviron.s.s_probe);
	printf("Negate Sysfail: %c\n", bugenviron.s.s_negsysfail);
	printf("Reset SCSI Bus: %c\n", bugenviron.s.s_resetscsi);
	printf("Ignore CFNA Block: %c\n", bugenviron.s.s_nocfblk);
	printf("SCSI sync method: %c\n", bugenviron.s.s_scsisync);

	printf("Auto Boot Enable: %c\n", bugenviron.b.b_enable);
	printf("Auto Boot on power-up Only: %c\n", bugenviron.b.b_poweruponly);
	printf("Auto Boot CLUN: %02x\n", bugenviron.b.b_clun);
	printf("Auto Boot DLUN: %02x\n", bugenviron.b.b_dlun);
	printf("Auto Boot Delay: %02x\n", bugenviron.b.b_delay);
	printf("Auto Boot String: %s\n", bugenviron.b.b_string);

	printf("ROM Boot Enable: %c\n", bugenviron.r.r_enable);
	printf("ROM Boot on power-up Only: %c\n", bugenviron.r.r_poweruponly);
	printf("ROM Boot Scan VME bus: %c\n", bugenviron.r.r_bootvme);
	printf("ROM Boot Delay: %02x\n", bugenviron.r.r_delay);
	printf("ROM Boot Start: %08x\n", bugenviron.r.r_start);
	printf("ROM Boot End: %08x\n", bugenviron.r.r_end);

	printf("Net Boot Enable: %c\n", bugenviron.n.n_enable);
	printf("Net Boot on power-up Only: %c\n", bugenviron.n.n_poweruponly);
	printf("Net Boot CLUN: %02x\n", bugenviron.n.n_clun);
	printf("Net Boot DLUN: %02x\n", bugenviron.n.n_dlun);
	printf("Net Boot Delay: %02x\n", bugenviron.n.n_delay);
	printf("Net Boot CFG param pointer: %08x\n", bugenviron.n.n_param);

	printf("Memory Size Enable: %c\n", bugenviron.m.m_sizeenable);
	printf("Memory Start: %08x\n", bugenviron.m.m_start);
	printf("Memory End: %08x\n", bugenviron.m.m_end);

	Debugger();
}
#else
#define	bug_printenv()	
#endif 

struct bugenviron *
mvmeprom_envrd(void)
{
	register int ret;
	char *ptr, *dptr, *ptr_end;
	int env_size = 0;
	int pkt_typ, pkt_len;
	unsigned long msr;

	env_size = bugenvsz();
        bzero(&bugenviron, sizeof(struct bugenviron)); 
        bzero(&bugenv_buf[0], 1024); 
	ptr = bugenv_buf;
	
	if (ptr != NULL) {

		BUGCTXT(msr);
		asm volatile ("mr 3, %0": : "r" (ptr));
		asm volatile ("mr 4, %0": : "r" (env_size));
		asm volatile ("li 5, 0x2");
		MVMEPROM_CALL(MVMEPROM_ENVIRON);
		asm volatile ("mr %0, 3" :  "=r" (ret));
		OSCTXT(msr);

		if (ret)
			return NULL;

		ptr_end = ptr + env_size;
		while (ptr <= ptr_end) {
	                pkt_typ = *ptr++;
			pkt_len = *ptr++;
			dptr = ptr;
			switch (pkt_typ) {
			case BUG_ENV_END:
				bugenv_init = 1; /* we have read the env */
				bug_printenv();				
				return(&bugenviron);
				break;
			case BUG_STARTUP_PARAM:
				/* All chars.  We can use bcopy. */
				bcopy(dptr, &bugenviron.s.s_mode, pkt_len);
				break;
			case BUG_AUTOBOOT_INFO:
				/* All chars.  We can use bcopy. */
				bcopy(dptr, &bugenviron.b.b_enable, pkt_len);
				break;
			case BUG_ROMBOOT_INFO:
				/* This data stream has integer info that 
				 * may not be word aligned.  We can't use 
				 * bcopy for the whole struct in this 
				 * instance. */
				bcopy(dptr, &bugenviron.r.r_enable, 4);
				dptr+=4;
				bcopy(dptr, &bugenviron.r.r_start, 4);
				dptr+=4;
				bcopy(dptr, &bugenviron.r.r_end, 4);
				break;
			case BUG_NETBOOT_INFO:
				/* This data stream has integer info that 
				 * may not be word aligned.  We can't use 
				 * bcopy for the whole struct in this 
				 * instance. */
				bcopy(dptr, &bugenviron.n.n_enable, 5);
				dptr+=5;
				bcopy(dptr, &bugenviron.n.n_param, 4);
				break;
			case BUG_MEMORY_INFO:
                                bugenviron.m.m_sizeenable = *dptr++;
				bcopy(dptr, &bugenviron.m.m_start, 4);
				dptr+=4;
				bcopy(dptr, &bugenviron.m.m_end, 4);
				break;
			}
			ptr += pkt_len;
		}
	}
	return NULL;
}
