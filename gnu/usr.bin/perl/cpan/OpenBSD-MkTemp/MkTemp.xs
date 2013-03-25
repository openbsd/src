#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <unistd.h>

MODULE = OpenBSD::MkTemp		PACKAGE = OpenBSD::MkTemp		


# $tmpdir = mkdtemp( "/tmp/tmpdirXXXXXXXXXX" );
char *
mkdtemp(SV *template)
	PREINIT:
		char *path;
	CODE:
		if (SvTAINTED(template))
			croak("tainted template");
		path = savesvpv(template);
		RETVAL = mkdtemp(path);
		if (RETVAL == NULL)
			Safefree(path);
	OUTPUT:
		RETVAL


# $fh = mkstemps_real( $template, suffixlen )
void
mkstemps_real(SV *template, int suffixlen)
	PREINIT:
		int fd;
	PPCODE:
		if (suffixlen < 0)
			croak("invalid suffixlen");
		if (SvTAINTED(template))
			croak("tainted template");
		/* detect read-only SVs */
		sv_catpv(template, "");
		fd = mkstemps(SvPV_nolen(template), suffixlen);
		SvSETMAGIC(template);
		if (fd != -1) {
			GV *gv = newGVgen("OpenBSD::MkTemp");
			PerlIO *io = PerlIO_fdopen(fd, "w+");
			if (do_open(gv, "+<&", 3, FALSE, 0, 0, io)) {
				mXPUSHs(sv_bless(newRV((SV*)gv),
				    gv_stashpv("OpenBSD::MkTemp",1)));
				SvREFCNT_dec(gv);
			}
		}

