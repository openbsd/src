/* 
 * Initialize a Personal Decstation baseboard framebuffer,
 * so it can be used as a bitmapped glass-tty console device.
 */
extern int
xcfbinit __P((struct fbinfo *fi, caddr_t base, int unit, int silent));
