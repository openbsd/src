#	$OpenBSD: Makefile,v 1.2 1996/04/26 18:22:51 mickey Exp $
#	$NetBSD: Makefile,v 1.4 1994/10/27 04:20:59 cgd Exp $

# uncomment one of these as appropriate for the board being used
#ETH_OBJ = ne2100.o
ETH_OBJ = wd80x3.o

# set this to the size of the eprom (decimal)
ROM_SIZE = 16384

.SUFFIXES: .s .c .o .list
.c.list:
	$(CC) $(CFLAGS) $(INC) -nostdinc -S $<
	$(AS) $*.s -a -o /dev/null > $@
	rm $*.s

.s.o:
	@echo $(AS) -o $*.o $< [$(DEFINES)]
	-@trap "/bin/rm -f $*.i X$*.c; exit 0" 0 1 2 3 10 15; \
	/bin/rm -f X$*.c; \
	ln -s $< X$*.c; \
	$(CC) -I${.CURDIR} -E $(CFLAGS) -nostdinc X$*.c > $*.i; \
	if [ $$? != 0 ]; then :; \
	else \
	 $(AS) $*.i -o $@; \
	fi

.s.list:
	@echo $(AS) -o $*.o $< [$(DEFINES)]
	-@trap "/bin/rm -f $*.i X$*.c; exit 0" 0 1 2 3 10 15; \
	/bin/rm -f X$*.c;	\
	ln -s $< X$*.c;	\
	$(CC) -E $(CFLAGS) -nostdinc X$*.c > $*.i;	\
	if [ $$? != 0 ]; then :;	\
	else \
		$(AS) $*.i -a -o /dev/null > $@;	\
	fi

# the relocation address (hex)
RELOC=	98000
CFLAGS=	-O2 $(DEFINES)
DEFINES=-DRELOC=0x$(RELOC) -DROM_SIZE=$(ROM_SIZE) -DTRACE=0 -Dprinte=printf \
 -DDEBUG -DUSE_BOOTP -DUSE_RARP #-DPhysBcopy=bcopy -DPhysBzero=bzero
NOMAN=	noman

PROG=	genprom
BOOT_OBJS+= start.o main.o cga.o kbd.o packet.o tftp.o arp.o \
 $(ETH_OBJ) misc.o asm.o

all: genprom boot.bin

boot.bin: $(BOOT_OBJS)
	${LD} -e _start -N -T ${RELOC} ${BOOT_OBJS}
	cp a.out boot.sym
	rm -f $@
	strip a.out
	dd if=a.out of=boot.v ibs=32 skip=1 obs=1024b
	./genprom <boot.v >$@
	rm -f a.out boot.v

# copy to dos floppy for testing (faster than burning an eprom)
dostest: boot.bin
	mcopy -n boot.bin a:

TAGS:	main.c cga.c kbd.c packet.c tftp.c arp.c wd80x3.c misc.c proto.h
	etags main.c cga.c kbd.c packet.c tftp.c arp.c wd80x3.c misc.c proto.h

clean:
	rm -f *.o *.list *~ genprom

cleandir: clean
	rm -f boot.bin boot.sym

.include <bsd.prog.mk>

