/*	$OpenBSD: libbug.h,v 1.5 2004/11/15 14:03:21 miod Exp $ */

#include <machine/prom.h>

/*
 * prototypes and such.   note that get/put char are in stand.h
 */

void	mvmeprom_delay(int);
int	mvmeprom_diskrd(struct mvmeprom_dskio *);
int	mvmeprom_diskwr(struct mvmeprom_dskio *);
struct	mvmeprom_brdid *mvmeprom_getbrdid(void);
int	peekchar(void);
void	mvmeprom_outln(char *, char *);
void	mvmeprom_outstr(char *, char *);
void	mvmeprom_rtc_rd(struct mvmeprom_time *);
int	mvmeprom_netctrl(struct mvmeprom_netctrl *);
int     mvmeprom_netctrl_init(u_char, u_char);
int	mvmeprom_netctrl_hwa(u_char, u_char, void *, u_long *);
int	mvmeprom_netctrl_tx(u_char, u_char, void *, u_long *);
int	mvmeprom_netctrl_rx(u_char, u_char, void *, u_long *);
int	mvmeprom_netctrl_flush_rx(u_char, u_char);
int	mvmeprom_netctrl_reset(u_char, u_char);

/*
 * bugcrt stuff 
 */

extern struct mvmeprom_args bugargs;

void	bugexec(void (*)());

/* Invoke the BUG */
#define MVMEPROM_CALL(x)	\
	__asm__ __volatile__ ("addi %r10,%r0," __STRING(x)); \
	__asm__ __volatile__ ("sc");
