/*	$OpenBSD: cons.h,v 1.2 2015/02/28 17:54:54 miod Exp $	*/

extern void *zs_conschan;

extern int  zs_getc(void *arg);
extern void zs_putc(void *arg, int c);

struct zschan;
struct zschan *zs_get_chan_addr(int zsc_unit, int channel);

#ifdef	KGDB
struct zs_chanstate;

void zs_kgdb_init(void);
void zskgdb(struct zs_chanstate *);
#endif
