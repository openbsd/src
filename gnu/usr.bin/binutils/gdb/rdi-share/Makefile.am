## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

noinst_LIBRARIES = libangsd.a

libangsd_a_SOURCES = ardi.c angel_bytesex.c crc.c devsw.c drivers.c etherdrv.c \
                     hostchan.c hsys.c logging.c msgbuild.c params.c rx.c \
                     serdrv.c serpardr.c tx.c unixcomm.c

noinst_HEADERS = adp.h adperr.h angel.h ardi.h armdbg.h buffers.h \
                 chandefs.h channels.h chanpriv.h crc.h dbg_conf.h dbg_cp.h \
                 dbg_hif.h dbg_rdi.h devclnt.h devices.h devsw.h drivers.h \
                 angel_endian.h ethernet.h host.h hostchan.h hsys.h logging.h \
                 msgbuild.h params.h rxtx.h sys.h unixcomm.h angel_bytesex.h

EXTRA_DIST = README.CYGNUS

INCLUDES =  -DRETRANS -DARM_RELEASE='"Berkeley Licence for Cygnus"'
