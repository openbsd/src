# $OpenBSD: Makefile.inc,v 1.4 2002/01/18 00:36:36 fgsch Exp $

includes:
	@cd ${LIBC_RSRCDIR}/include; \
	 for h in pthread.h pthread_np.h sched.h semaphore.h spinlock.h; do \
		cmp -s $$h ${DESTDIR}/usr/include/$$h > /dev/null 2>&1 || \
		(echo ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
			-m 444 $$h ${DESTDIR}/usr/include; \
		 ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
			$$h ${DESTDIR}/usr/include) || exit 1; \
	 done

