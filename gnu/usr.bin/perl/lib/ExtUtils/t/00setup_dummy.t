#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use strict;
use Test::More tests => 9;
use File::Basename;
use File::Path;
use File::Spec;

if( $^O eq 'VMS' ) {
    # On older systems we might exceed the 8-level directory depth limit
    # imposed by RMS.  We get around this with a rooted logical, but we
    # can't create logical names with attributes in Perl, so we do it
    # in a DCL subprocess and put it in the job table so the parent sees it.
    open( BFDTMP, '>bfdtesttmp.com' ) || die "Error creating command file; $!";
    print BFDTMP <<'COMMAND';
$ BFD_TEST_ROOT = F$PARSE("SYS$DISK:[-]",,,,"NO_CONCEAL")-".][000000"-"]["-"].;"+".]"
$ DEFINE/JOB/NOLOG/TRANSLATION=CONCEALED BFD_TEST_ROOT 'BFD_TEST_ROOT'
COMMAND
    close BFDTMP;

    system '@bfdtesttmp.com';
    1 while unlink 'bfdtesttmp.com';
}


my %Files = (
             'Big-Dummy/lib/Big/Dummy.pm'     => <<'END',
package Big::Dummy;

$VERSION = 0.01;

=head1 NAME

Big::Dummy - Try "our" hot dog's

=cut

1;
END

             'Big-Dummy/Makefile.PL'          => <<'END',
use ExtUtils::MakeMaker;

# This will interfere with the PREREQ_PRINT tests.
printf "Current package is: %s\n", __PACKAGE__ unless "@ARGV" =~ /PREREQ/;

WriteMakefile(
    NAME          => 'Big::Dummy',
    VERSION_FROM  => 'lib/Big/Dummy.pm',
    PREREQ_PM     => { strict => 0 },
    ABSTRACT_FROM => 'lib/Big/Dummy.pm',
    AUTHOR        => 'Michael G Schwern <schwern@pobox.com>',
);
END

             'Big-Dummy/t/compile.t'          => <<'END',
print "1..2\n";

print eval "use Big::Dummy; 1;" ? "ok 1\n" : "not ok 1\n";
print "ok 2 - TEST_VERBOSE\n";
END

             'Big-Dummy/Liar/t/sanity.t'      => <<'END',
print "1..3\n";

print eval "use Big::Dummy; 1;" ? "ok 1\n" : "not ok 1\n";
print eval "use Big::Liar; 1;" ? "ok 2\n" : "not ok 2\n";
print "ok 3 - TEST_VERBOSE\n";
END

             'Big-Dummy/Liar/lib/Big/Liar.pm' => <<'END',
package Big::Liar;

$VERSION = 0.01;

1;
END

             'Big-Dummy/Liar/Makefile.PL'     => <<'END',
use ExtUtils::MakeMaker;

my $mm = WriteMakefile(
              NAME => 'Big::Liar',
              VERSION_FROM => 'lib/Big/Liar.pm',
              _KEEP_AFTER_FLUSH => 1
             );

print "Big::Liar's vars\n";
foreach my $key (qw(INST_LIB INST_ARCHLIB)) {
    print "$key = $mm->{$key}\n";
}
END

             'Problem-Module/Makefile.PL'   => <<'END',
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME    => 'Problem::Module',
);
END

             'Problem-Module/subdir/Makefile.PL'    => <<'END',
printf "\@INC %s .\n", (grep { $_ eq '.' } @INC) ? "has" : "doesn't have";

warn "I think I'm going to be sick\n";
die "YYYAaaaakkk\n";
END

            );

while(my($file, $text) = each %Files) {
    # Convert to a relative, native file path.
    $file = File::Spec->catfile(File::Spec->curdir, split m{\/}, $file);

    my $dir = dirname($file);
    mkpath $dir;
    open(FILE, ">$file");
    print FILE $text;
    close FILE;

    ok( -e $file, "$file created" );
}


pass("Setup done");
