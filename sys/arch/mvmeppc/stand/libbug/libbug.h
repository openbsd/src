/*	$OpenBSD: libbug.h,v 1.1 2001/06/26 21:58:04 smurph Exp $ */

/*
 * prototypes and such.   note that get/put char are in stand.h
 */

void	mvmeprom_delay __P((int));
int	mvmeprom_diskrd __P((struct mvmeprom_dskio *));
int	mvmeprom_diskwr __P((struct mvmeprom_dskio *));
struct	mvmeprom_brdid *mvmeprom_getbrdid __P((void));
int	peekchar __P((void));
void	mvmeprom_outln __P((char *, char *));
void	mvmeprom_outstr __P((char *, char *));
void	mvmeprom_rtc_rd __P((struct mvmeprom_time *));
int	mvmeprom_netctrl __P((struct mvmeprom_netctrl *));
int     mvmeprom_netctrl_init __P((u_char, u_char));
int	mvmeprom_netctrl_hwa __P((u_char, u_char, void *, u_long *));
int	mvmeprom_netctrl_tx __P((u_char, u_char, void *, u_long *));
int	mvmeprom_netctrl_rx __P((u_char, u_char, void *, u_long *));
int	mvmeprom_netctrl_flush_rx __P((u_char, u_char));
int	mvmeprom_netctrl_reset __P((u_char, u_char));

/*
 * bugcrt stuff 
 */

extern struct mvmeprom_args bugargs;

void	bugexec __P((void (*)()));
