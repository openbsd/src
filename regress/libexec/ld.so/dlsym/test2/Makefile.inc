# $OpenBSD: Makefile.inc,v 1.3 2005/09/18 12:17:29 kurt Exp $

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
