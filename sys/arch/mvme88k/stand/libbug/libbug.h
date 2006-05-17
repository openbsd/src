/*	$OpenBSD: libbug.h,v 1.5 2006/05/17 06:21:33 miod Exp $ */

/*
 * prototypes and such.   note that get/put char are in stand.h
 */

void	mvmeprom_delay(int);
int	mvmeprom_diskrd(struct mvmeprom_dskio *);
int	mvmeprom_diskwr(struct mvmeprom_dskio *);
struct mvmeprom_brdid *mvmeprom_brdid(void);
int	mvmeprom_netfopen(struct mvmeprom_netfopen *);
int	mvmeprom_netfread(struct mvmeprom_netfread *);
void	mvmeprom_outln(char *, char *);
void	mvmeprom_outstr(char *, char *);
void	mvmeprom_rtc_rd(struct mvmeprom_time *);

/*
 * bugcrt stuff
 */

struct mvmeprom_args {
	u_int	dev_lun;
	u_int	ctrl_lun;
	u_int	flags;
	u_int	ctrl_addr;
	u_int	entry;
	u_int	conf_blk;
	char	*arg_start;
	char	*arg_end;
	char	*nbarg_start;
	char	*nbarg_end;
};

extern struct mvmeprom_args bugargs;

void	bugexec(void (*)());
