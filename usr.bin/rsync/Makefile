PREFIX	 = /usr/local
OBJS	 = blocks.o \
	   child.o \
	   client.o \
	   downloader.o \
	   fargs.o \
	   flist.o \
	   hash.o \
	   io.o \
	   log.o \
	   md4.o \
	   mkpath.o \
	   receiver.o \
	   sender.o \
	   server.o \
	   session.o \
	   socket.o \
	   symlinks.o \
	   uploader.o
ALLOBJS	 = $(OBJS) \
	   main.o
AFLS	 = afl/test-blk_recv \
	   afl/test-flist_recv
CFLAGS	+= -O0 -g -W -Wall -Wextra -Wno-unused-parameter
MANDIR	 = $(PREFIX)/man
BINDIR	 = $(PREFIX)/bin

all: openrsync

openrsync: $(ALLOBJS)
	$(CC) -o $@ $(ALLOBJS) -lm

afl: $(AFLS)

$(AFLS): $(OBJS)
	$(CC) -o $@ $*.c $(OBJS)

install: openrsync
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	install -m 0444 openrsync.1 $(DESTDIR)$(MANDIR)/man1
	install -m 0444 rsync.5 rsyncd.5 $(DESTDIR)$(MANDIR)/man5
	install -m 0555 openrsync $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/openrsync
	rm -f $(DESTDIR)$(MANDIR)/man1/openrsync.1
	rm -f $(DESTDIR)$(MANDIR)/man5/rsync.5
	rm -f $(DESTDIR)$(MANDIR)/man5/rsyncd.5

clean:
	rm -f $(ALLOBJS) openrsync $(AFLS)

$(ALLOBJS) $(AFLS): extern.h

blocks.o downloader.o hash.o md4.o: md4.h
