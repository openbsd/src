/* $OpenBSD: ite_blank.h,v 1.1 1999/11/05 17:15:34 espie Exp $ */
void ite_restart_blanker __P((struct ite_softc *));
void ite_reset_blanker __P((struct ite_softc *));
void ite_disable_blanker __P((struct ite_softc *));
void ite_enable_blanker __P((struct ite_softc *));

extern int blank_time;
