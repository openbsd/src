#	@(#)Makefile	8.17 (Berkeley) 9/12/95

#
#  This Makefile uses the new Berkeley "make" program.  See Makefile.dist
#  for a more vanilla version.
#
#  Configuration files are created using "m4 file.mc > file.cf";
#  this may be easier than tweaking the Makefile.  You do need to
#  have a fairly modern M4 available (GNU m4 works).  On SunOS, use
#  /usr/5bin/m4.
#

M4=	m4
CHMOD=	chmod
ROMODE=	444
RM=	rm -f

.SUFFIXES:  .mc .cf

.mc.cf:
	$(RM) $@
	(cd ${.CURDIR} && $(M4) ${@:R}.mc > ${.OBJDIR}/$@)
	$(CHMOD) $(ROMODE) $@

ALL=	boat-anchor.cf chimera.cf pain.cf sun-lamp.cf trinity.cf \
	clientproto.cf netbsd-proto.cf tcpproto.cf uucpproto.cf

all: $(ALL)

clean cleandir:
	$(RM) $(ALL) core

depend install: 

distribution:
	install -c -o root -g wheel -m 444 netbsd-proto.cf \
	    ${DESTDIR}/etc/sendmail.cf

# this is overkill, but....
M4FILES=\
	../domain/Berkeley.EDU.m4 \
	../domain/CS.Berkeley.EDU.m4 \
	../domain/EECS.Berkeley.EDU.m4 \
	../domain/S2K.Berkeley.EDU.m4 \
	../feature/allmasquerade.m4 \
	../feature/always_add_domain.m4 \
	../feature/bestmx_is_local.m4 \
	../feature/bitdomain.m4 \
	../feature/domaintable.m4 \
	../feature/local_procmail.m4 \
	../feature/mailertable.m4 \
	../feature/nocanonify.m4 \
	../feature/nodns.m4 \
	../feature/notsticky.m4 \
	../feature/nouucp.m4 \
	../feature/nullclient.m4 \
	../feature/redirect.m4 \
	../feature/smrsh.m4 \
	../feature/stickyhost.m4 \
	../feature/use_cw_file.m4 \
	../feature/uucpdomain.m4 \
	../hack/cssubdomain.m4 \
	../m4/cf.m4 \
	../m4/cfhead.m4 \
	../m4/nullrelay.m4 \
	../m4/proto.m4 \
	../m4/version.m4 \
	../mailer/cyrus.m4 \
	../mailer/fax.m4 \
	../mailer/local.m4 \
	../mailer/mail11.m4 \
	../mailer/pop.m4 \
	../mailer/procmail.m4 \
	../mailer/smtp.m4 \
	../mailer/usenet.m4 \
	../mailer/uucp.m4 \
	../ostype/bsd4.4.m4 \
	../siteconfig/uucp.cogsci.m4 \
	../siteconfig/uucp.old.arpa.m4 \
	../siteconfig/uucp.ucbarpa.m4 \
	../siteconfig/uucp.ucbvax.m4 \

$(ALL):  $(M4FILES)

.include <bsd.prog.mk>
