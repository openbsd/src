#!perl -w

use strict;
use Config qw(%Config);
use ExtUtils::MakeMaker;

my @extra;
push(@extra, DEFINE => "-DU32_ALIGNMENT_REQUIRED") unless free_u32_alignment();
push(@extra, INSTALLDIRS => 'perl') if $] >= 5.008 && $] < 5.012;

if ($^O eq 'VMS') {
    if (defined($Config{ccname})) {
        if (grep(/VMS_VAX/, @INC) && ($Config{ccname} eq 'DEC')) {
            # VAX compiler optimizer even as late as v6.4 gets stuck
            push(@extra, OPTIMIZE => "/Optimize=(NODISJOINT)");
        }
    }
}


WriteMakefile(
    'NAME'	   => 'Digest::MD5',
    'VERSION_FROM' => 'MD5.pm',
    'ABSTRACT'     => 'Perl interface to the MD-5 algorithm',
    'AUTHOR'       => 'Gisle Aas <gisle@activestate.com>',
    'LICENSE'      => 'perl',
    'MIN_PERL_VERSION' => 5.006,
    'PREREQ_PM'    => {
			'Digest::base' => '1.00',
			'XSLoader' => 0,
		      },
    'META_MERGE'   => {
        resources  => {
            repository => 'http://github.com/gisle/digest-md5',
        }
    },
    @extra,
);



sub free_u32_alignment
{
    $|=1;
    if (exists $Config{d_u32align}) {
       print "Perl's config says that U32 access must ";
       print "not " unless $Config{d_u32align};
       print "be aligned.\n";
       return !$Config{d_u32align};
    }
    
    if ($^O eq 'VMS' || $^O eq 'MSWin32') {
       print "Assumes that $^O implies free alignment for U32 access.\n";
       return 1;
    }
    
    if ($^O eq 'hpux' && $Config{osvers} < 11.0) {
       print "Will not test for free alignment on older HP-UX.\n";
       return 0;
    }
    
    print "Testing alignment requirements for U32... ";
    open(ALIGN_TEST, ">u32align.c") or die "$!";
    print ALIGN_TEST <<'EOT'; close(ALIGN_TEST);
/*--------------------------------------------------------------*/
/*  This program allocates a buffer of U8 (char) and then tries */
/*  to access it through a U32 pointer at every offset.  The    */
/*  program  is expected to die with a bus error/seg fault for  */
/*  machines that do not support unaligned integer read/write   */
/*--------------------------------------------------------------*/

#include <stdio.h>
#include "EXTERN.h"
#include "perl.h"

#ifdef printf
 #undef printf
#endif

int main(int argc, char** argv, char** env)
{
#if BYTEORDER == 0x1234 || BYTEORDER == 0x4321
    volatile U8 buf[] = "\0\0\0\1\0\0\0\0";
    volatile U32 *up;
    int i;

    if (sizeof(U32) != 4) {
	printf("sizeof(U32) is not 4, but %d\n", sizeof(U32));
	exit(1);
    }

    fflush(stdout);

    for (i = 0; i < 4; i++) {
	up = (U32*)(buf + i);
	if (! ((*up == 1 << (8*i)) ||   /* big-endian */
	       (*up == 1 << (8*(3-i)))  /* little-endian */
	      )
	   )
	{
	    printf("read failed (%x)\n", *up);
	    exit(2);
	}
    }

    /* write test */
    for (i = 0; i < 4; i++) {
	up = (U32*)(buf + i);
	*up = 0xBeef;
	if (*up != 0xBeef) {
	    printf("write failed (%x)\n", *up);
	    exit(3);
	}
    }

    printf("no restrictions\n");
    exit(0);
#else
    printf("unusual byteorder, playing safe\n");
    exit(1);
#endif
    return 0;
}
/*--------------------------------------------------------------*/
EOT

    my $cc_cmd = "$Config{cc} $Config{ccflags} -I$Config{archlibexp}/CORE";
    my $exe = "u32align$Config{exe_ext}";
    $cc_cmd .= " -o $exe";
    my $rc;
    $rc = system("$cc_cmd $Config{ldflags} u32align.c $Config{libs}");
    if ($rc) {
	print "Can't compile test program.  Will ensure alignment to play safe.\n\n";
	unlink("u32align.c", $exe, "u32align$Config{obj_ext}");
	return 0;
    }

    $rc = system("./$exe");
    unlink("u32align.c", $exe, "u32align$Config{obj_ext}");

    return 1 unless $rc;

    if ($rc > 0x80) {
	(my $cp = $rc) >>= 8;
	print "Test program exit status was $cp\n";
    }
    if ($rc & 0x80) {
	$rc &= ~0x80;
	unlink("core") && print "Core dump deleted\n";
    }
    print "signal $rc\n" if $rc && $rc < 0x80;
    return 0;
}

BEGIN {
    # compatibility with older versions of MakeMaker
    my $developer = -d ".git";
    my %mm_req = (
        LICENCE => 6.31,
        META_MERGE => 6.45,
        META_ADD => 6.45,
        MIN_PERL_VERSION => 6.48,
    );
    undef(*WriteMakefile);
    *WriteMakefile = sub {
        my %arg = @_;
        for (keys %mm_req) {
            unless (eval { ExtUtils::MakeMaker->VERSION($mm_req{$_}) }) {
                warn "$_ $@" if $developer;
                delete $arg{$_};
            }
        }
        ExtUtils::MakeMaker::WriteMakefile(%arg);
    };
}
