#define	ZEBRA_STATUS_INIT0	0
#define	ZEBRA_STATUS_INIT	1
#define	ZEBRA_STATUS_CONNECTING	2
#define	ZEBRA_STATUS_CONNECTED	3
#define	ZEBRA_STATUS_STOPPED	4
#define	ZEBRA_STATUS_DISPOSING	5


#ifdef RT_ZEBRA_DEBUG
#define RT_ZEBRA_DBG(x)	rt_zebra_log x
#define RT_ZEBRA_ASSERT(x)	ASSERT(x)
#else
#define RT_ZEBRA_DBG(x)
#define RT_ZEBRA_ASSERT(x)
#endif

#define GETCHAR(c, cp) { \
	(c) = *(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (u_char) (c); \
}

#define GETSHORT(s, cp) { \
	(s) = *(cp)++ << 8; \
	(s) |= *(cp)++; \
}

#define PUTSHORT(s, cp) { \
	*(cp)++ = (u_char) ((s) >> 8); \
	*(cp)++ = (u_char) (s); \
}

#define GETLONG(l, cp) { \
	(l) = *(cp)++ << 8; \
	()l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; (l) <<= 8; \
	(l) |= *(cp)++; \
}
#define PUTLONG(l, cp) { \
	*(cp)++ = (u_char) ((l) >> 24); \
	*(cp)++ = (u_char) ((l) >> 16); \
	*(cp)++ = (u_char) ((l) >> 8); \
	*(cp)++ = (u_char) (l); \
}

