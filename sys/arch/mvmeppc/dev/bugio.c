/*	$OpenBSD: bugio.c,v 1.2 2001/07/04 08:31:31 niklas Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <machine/prom.h>


/* BUG - timing routine */
void
mvmeprom_delay(msec)
	int msec; /* This is r3 */
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0" :: "r"(msec));
	MVMEPROM_CALL(MVMEPROM_DELAY);
        ppc_set_msr(omsr);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_diskrd(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	asm volatile ("mr 3, %0" :: "r"(arg));
	MVMEPROM_CALL(MVMEPROM_NETRD);
	asm volatile ("mr %0, 3" :  "=r" (ret));
        ppc_set_msr(omsr);
	return ((ret & 0x8));
}

/* returns 0: success, nonzero: error */
int
mvmeprom_diskwr(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	asm volatile ("mr 3, %0" :: "r"(arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("mr %0, 3" :  "=r" (ret));
        ppc_set_msr(omsr);
	return ((ret & 0x8));
}

/* BUG - query board routines */
struct mvmeprom_brdid *
mvmeprom_brdid()
{
	struct mvmeprom_brdid *id;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	MVMEPROM_CALL(MVMEPROM_BRD_ID);
	asm volatile ("mr %0, 3": "=r" (id):);
        ppc_set_msr(omsr);
	return (id);
}

/* returns 0 if no characters ready to read */
int
mvmeprom_getchar()
{
	int ret;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	MVMEPROM_CALL(MVMEPROM_INCHR);
	asm volatile ("mr %0, 3" :  "=r" (ret));
        ppc_set_msr(omsr);
	return ret;
}

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	int ret;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("mr %0, 3" :  "=r" (ret));
        ppc_set_msr(omsr);
	return (!(ret & 0x4));
}

/* returns 0: success, nonzero: error */
int
mvmeprom_netctrl(arg)
	struct mvmeprom_netctrl *arg;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0":: "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETCTRL);
        ppc_set_msr(omsr);
	return (arg->status);
}

int 
mvmeprom_netctrl_init(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 0; /* init */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}

int 
mvmeprom_netctrl_hwa(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 1; /* get hw address */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_tx(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 2; /* transmit */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_rx(clun, dlun, addr, len)
u_char	clun;
u_char	dlun;
void 	*addr;
u_long  *len;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 3; /* receive */
	niocall.addr = addr;
	niocall.len = *len;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	*len = niocall.len;
	return(niocall.status);
}

int 
mvmeprom_netctrl_flush_rx(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 4; /* reset */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}

int 
mvmeprom_netctrl_reset(clun, dlun)
u_char	clun;
u_char	dlun;
{
	struct mvmeprom_netctrl niocall;
	niocall.clun = clun;
	niocall.dlun = dlun;
	niocall.status = 0;
	niocall.cmd = 5; /* reset */
	niocall.addr = 0;
	niocall.len = 0;
	niocall.flags = 0;
	mvmeprom_netctrl(&niocall);
	return(niocall.status);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_netfopen(arg)
	struct mvmeprom_netfopen *arg;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETFOPEN);
        ppc_set_msr(omsr);
	return (arg->status);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_netfread(arg)
	struct mvmeprom_netfread *arg;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETFREAD);
        ppc_set_msr(omsr);
	return (arg->status);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_netrd(arg)
	struct mvmeprom_netio *arg;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETRD);
        ppc_set_msr(omsr);
	return (arg->status);
}

/* returns 0: success, nonzero: error */
int
mvmeprom_netwr(arg)
	struct mvmeprom_netio *arg;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETWR);
        ppc_set_msr(omsr);
	return (arg->status);
}

void
mvmeprom_outln(start, end)
	char *start, *end;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (start));
	asm volatile ("mr 4, %0": : "r" (end));
	MVMEPROM_CALL(MVMEPROM_OUTLN);
        ppc_set_msr(omsr);
}

void
mvmeprom_outstr(start, end)
	char *start, *end;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (start));
	asm volatile ("mr 4, %0": : "r" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
        ppc_set_msr(omsr);
}

void
mvmeprom_outchar(c)
	int c;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm  volatile ("mr 3, %0" :: "r" (c));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
        ppc_set_msr(omsr);
}

/* BUG - return to bug routine */
void
mvmeprom_return()
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	MVMEPROM_CALL(MVMEPROM_RETURN);
        ppc_set_msr(omsr);
	/*NOTREACHED*/
}


void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	asm volatile ("mr 3, %0": : "r" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
        ppc_set_msr(omsr);
}

int 
bugenvsz(void)
{
	register int ret;
	char tmp[1];
	void *ptr = tmp;
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));
	
	asm volatile ("mr 3, %0": : "r" (ptr));
	asm volatile ("li 5, 0x1");
	asm volatile ("li 5, 0x0"); /* get size */
	MVMEPROM_CALL(MVMEPROM_ENVIRON);
	asm volatile ("mr %0, 3" :  "=r" (ret));
        ppc_set_msr(omsr);
	
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
	unsigned long omsr = ppc_get_msr();
        ppc_set_msr(((omsr | PSL_IP) &~ PSL_EE));

	env_size = bugenvsz();
        bzero(&bugenviron, sizeof(struct bugenviron)); 
        bzero(&bugenv_buf[0], 1024); 
	ptr = bugenv_buf;
	
	if (ptr != NULL) {

		asm volatile ("mr 3, %0": : "r" (ptr));
		asm volatile ("mr 4, %0": : "r" (env_size));
		asm volatile ("li 5, 0x2");
		MVMEPROM_CALL(MVMEPROM_ENVIRON);
		asm volatile ("mr %0, 3" :  "=r" (ret));

		if (ret) { /* scram if we have an error */
			ppc_set_msr(omsr);
			return NULL;
		}
		ptr_end = ptr + env_size;
		while (ptr <= ptr_end) {
	                pkt_typ = *ptr++;
			pkt_len = *ptr++;
			dptr = ptr;
			switch (pkt_typ) {
			case BUG_ENV_END:
				bugenv_init = 1; /* we have read the env */
				bug_printenv();				
				ppc_set_msr(omsr);
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
        ppc_set_msr(omsr);
	return NULL;
}

