.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa
AA_OBJDIR!=    if [ -d $(AA_DIR)/${__objdir} ]; then \
                        echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

LIB=ac
SRCS= ac.C
#CFLAGS=-I${.CURDIR}/../libaa
LDADD=-L$(AA_OBJDIR)
LDADD+=-laa
regress: all
.include <bsd.lib.mk>
