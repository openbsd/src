SUBDIR= 	keycap cursor fontedit fonts kcon loadfont scon \
		userkeys vttest ispcvt mcon pcvtdoc

.if make(clean) || make(cleandir) || make(obj)
SUBDIR+=	vgaio kbdio set2061
SUBDIR+=	demo
.endif

.include <bsd.subdir.mk>
