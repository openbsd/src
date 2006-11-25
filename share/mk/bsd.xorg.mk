.include <bsd.own.mk>
XORG_PREFIX?=   /usr/X11R6
.if exists(${XORG_PREFIX}/share/mk/bsd.xorg.mk)
. include       "${XORG_PREFIX}/share/mk/bsd.xorg.mk"
.endif
