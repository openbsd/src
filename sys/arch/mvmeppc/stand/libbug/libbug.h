/*	$OpenBSD: libbug.h,v 1.2 2002/03/14 01:26:41 millert Exp $ */

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

void	bugexec __P((void (*)()));
