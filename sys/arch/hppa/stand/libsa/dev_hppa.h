/*	$OpenBSD: dev_hppa.h,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $	*/

struct hppa_dev {
	dev_t	bootdev;

	struct disklabel label;
};

#define CN_HPA		PAGE0->mem_cons.pz_hpa
#define CN_SPA		PAGE0->mem_cons.pz_spa
#define CN_LAYER	PAGE0->mem_cons.pz_layers
#define CN_IODC		PAGE0->mem_cons.pz_iodc_io
#define CN_CLASS	PAGE0->mem_cons.pz_class

#define KY_HPA		PAGE0->mem_kbd.pz_hpa
#define KY_SPA		PAGE0->mem_kbd.pz_spa
#define KY_LAYER	PAGE0->mem_kbd.pz_layers
#define KY_IODC		PAGE0->mem_kbd.pz_iodc_io
#define KY_CLASS	PAGE0->mem_kbd.pz_class

#define BT_HPA		PAGE0->mem_boot.pz_hpa    
#define BT_SPA		PAGE0->mem_boot.pz_spa    
#define BT_LAYER	PAGE0->mem_boot.pz_layers 
#define BT_IODC		PAGE0->mem_boot.pz_iodc_io
#define BT_CLASS	PAGE0->mem_boot.pz_class

#define MINIOSIZ	64		/* minimum buffer size for IODC call */
#define MAXIOSIZ	(64 * 1024)	/* maximum buffer size for IODC call */
#define	BTIOSIZ		(8 * 1024)	/* size of boot device I/O buffer */

#define IONBPG		(2 * 1024)	/* page alignment for I/O buffers */
#define IOPGSHIFT	11		/* LOG2(IONBPG) */
#define IOPGOFSET	(IONBPG-1)	/* byte offset into I/O buffer */

#define ANYSLOT	(-1)
#define NOSLOT	(-2)

extern char btbuf[];
extern int pdcbuf[];			/* PDC returns, pdc.c */
extern struct  pz_device ctdev;		/* cartridge tape (boot) device path */

int iodc_rw __P((char *, u_int, u_int, int func, struct pz_device *));

