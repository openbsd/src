int	dtopKBDGetc __P((dev_t dev));
void	dtopKBDPutc __P((dev_t dev, int c));

/*
 * Device numbers.
 */
#define	DTOPKBD_PORT	0
#define	DTOPMOUSE_PORT	1
