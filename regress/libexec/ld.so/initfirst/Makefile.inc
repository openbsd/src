# $OpenBSD: Makefile.inc,v 1.1 2011/11/08 23:51:02 guenther Exp $

.include <bsd.obj.mk>

IF1_DIR=${.CURDIR}/../libif1

IF1_OBJDIR!=	if [ -d $(IF1_DIR)/${__objdir} ]; then \
			echo "$(IF1_DIR)/${__objdir}"; \
		else \
			echo "$(IF1_DIR)"; \
		fi

IF2_DIR=${.CURDIR}/../libif2

IF2_OBJDIR!=	if [ -d $(IF2_DIR)/${__objdir} ]; then \
			echo "$(IF2_DIR)/${__objdir}"; \
		else \
			echo "$(IF2_DIR)"; \
		fi

IF3_DIR=${.CURDIR}/../libif3

IF3_OBJDIR!=	if [ -d $(IF3_DIR)/${__objdir} ]; then \
			echo "$(IF3_DIR)/${__objdir}"; \
		else \
			echo "$(IF3_DIR)"; \
		fi

NORMAL_DIR=${.CURDIR}/../libnormal

NORMAL_OBJDIR!=	if [ -d $(NORMAL_DIR)/${__objdir} ]; then \
			echo "$(NORMAL_DIR)/${__objdir}"; \
		else \
			echo "$(NORMAL_DIR)"; \
		fi
