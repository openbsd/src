#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <unistd.h>

MODULE = OpenBSD::MkTemp		PACKAGE = OpenBSD::MkTemp		


# $tmpdir = mkdtemp( "/tmp/tmpdirXXXXXXXXXX" );
void
mkdtemp(SV *template)
	PREINIT:
		char *path;
	PPCODE:
		if (SvTAINTED(template))
			croak("tainted template");
		path = savesvpv(template);
		if (mkdtemp(path) == NULL) {
			Safefree(path);
			croak("Unable to mkdtemp(%s): %s",
				SvPV_nolen(template), strerror(errno));
		}
		mXPUSHp(path, strlen(path));
		Safefree(path);


# ($fh, $file) = mkstemps( "/tmp/tmpfileXXXXXXXXXX", $suffix);
void
mkstemps(SV *template, ...)
	PROTOTYPE: $;$
	PREINIT:
		const char *t, *s;
		SV *path;
		STRLEN t_len, s_len;
		int fd;
	PPCODE:
		if (SvTAINTED(template))
			croak("tainted template");
		if (items > 1) {
			s = SvPV(ST(1), s_len);
			if (s_len && SvTAINTED(ST(1)))
				croak("tainted suffix");
		} else {
			s_len = 0;
		}
		t = SvPV(template, t_len);
		path = sv_2mortal(newSV(t_len + s_len));
		sv_setpvn(path, t, t_len);
		if (s_len)
			sv_catpvn(path, s, s_len);
		fd = mkstemps(SvPV_nolen(path), s_len);
		if (fd != -1) {
			GV *gv = newGVgen("OpenBSD::MkTemp");
			PerlIO *io = PerlIO_fdopen(fd, "w+");
			if (do_open(gv, "+<&", 3, FALSE, 0, 0, io)) {
				mPUSHs(sv_bless(newRV((SV*)gv),
				    gv_stashpv("OpenBSD::MkTemp",1)));
				SvREFCNT_dec(gv);
				PUSHs(path);
			} else {
				close(fd);
				unlink(SvPV_nolen(path));
				croak("Unable to create IO");
			}
		} else {
			sv_setpvn(path, t, t_len);
			if (s_len)
				sv_catpvn(path, s, s_len);
			croak("Unable to mkstemp(%s): %s", SvPV_nolen(path),
					strerror(errno));
		}

