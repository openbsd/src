# $OpenBSD: Makefile.inc,v 1.1.1.1 2005/09/30 14:51:52 kurt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

BB_DIR=${.CURDIR}/../libbb

BB_OBJDIR!=	if [ -d $(BB_DIR)/${__objdir} ]; then \
			echo "$(BB_DIR)/${__objdir}"; \
		else \
			echo "$(BB_DIR)"; \
		fi

CC_DIR=${.CURDIR}/../libcc

CC_OBJDIR!=	if [ -d $(CC_DIR)/${__objdir} ]; then \
			echo "$(CC_DIR)/${__objdir}"; \
		else \
			echo "$(CC_DIR)"; \
		fi

ZZ_DIR=${.CURDIR}/../libzz

ZZ_OBJDIR!=	if [ -d $(ZZ_DIR)/${__objdir} ]; then \
			echo "$(ZZ_DIR)/${__objdir}"; \
		else \
			echo "$(ZZ_DIR)"; \
		fi
