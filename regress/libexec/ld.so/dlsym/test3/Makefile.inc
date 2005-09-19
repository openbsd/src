# $OpenBSD: Makefile.inc,v 1.1.1.1 2005/09/19 03:34:18 kurt Exp $

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

DD_DIR=${.CURDIR}/../libdd

DD_OBJDIR!=	if [ -d $(DD_DIR)/${__objdir} ]; then \
			echo "$(DD_DIR)/${__objdir}"; \
		else \
			echo "$(DD_DIR)"; \
		fi

EE_DIR=${.CURDIR}/../libee

EE_OBJDIR!=	if [ -d $(EE_DIR)/${__objdir} ]; then \
			echo "$(EE_DIR)/${__objdir}"; \
		else \
			echo "$(EE_DIR)"; \
		fi
