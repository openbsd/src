PREFIX	 = /usr/local
CFLAGS	+= -g -W -Wall
OBJS 	 = acctproc.o \
	   base64.o \
	   certproc.o \
	   chngproc.o \
	   dbg.o \
	   dnsproc.o \
	   fileproc.o \
	   http.o \
	   jsmn.o \
	   json.o \
	   keyproc.o \
	   main.o \
	   netproc.o \
	   revokeproc.o \
	   rsa.o \
	   sandbox-pledge.o \
	   util.o \
	   util-pledge.o

letskencrypt: $(OBJS)
	$(CC) -o $@ $(OBJS) -ltls -lssl -lcrypto

rsa.o acctproc.o keyproc.o: rsa.h

jsmn.o json.o: jsmn.h

http.o netproc.o: http.h

install: letskencrypt
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/man/man1
	install -m 0755 letskencrypt $(DESTDIR)$(PREFIX)/bin
	install -m 0644 letskencrypt.1 $(DESTDIR)$(PREFIX)/man/man1

$(OBJS): extern.h

clean:
	rm -f letskencrypt $(OBJS)
