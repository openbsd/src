# $OpenBSD: Makefile.inc,v 1.7 2014/07/16 20:02:17 okan Exp $

includes:
	@cd ${SRCDIR}/include; \
	 for h in pthread.h pthread_np.h sched.h semaphore.h; do \
		cmp -s $$h ${DESTDIR}/usr/include/$$h > /dev/null 2>&1 || \
		(echo ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
			-m 444 $$h ${DESTDIR}/usr/include; \
		 ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
			$$h ${DESTDIR}/usr/include) || exit 1; \
	 done
