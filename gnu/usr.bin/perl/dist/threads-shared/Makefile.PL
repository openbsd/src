# Module makefile for threads::shared (using ExtUtils::MakeMaker)

require 5.008;

use strict;
use warnings;

use ExtUtils::MakeMaker;


# Used to check for a 'C' compiler
sub check_cc
{
    require File::Spec;

    my $cmd = $_[0];
    if (-x $cmd or MM->maybe_command($cmd)) {
        return (1);       # CC command found
    }
    for my $dir (File::Spec->path(), '.') {
        my $abs = File::Spec->catfile($dir, $cmd);
        if (-x $abs or MM->maybe_command($abs)) {
            return (1);   # CC command found
        }
    }
    return;
}

sub have_cc
{
    eval { require Config_m; };     # ExtUtils::FakeConfig (+ ActivePerl)
    if ($@) {
        eval { require Config; };   # Everyone else
    }
    my @chunks = split(/ /, $Config::Config{cc});
    # $Config{cc} may contain args; try to find out the program part
    while (@chunks) {
        if (check_cc("@chunks")) {
            return (1);   # CC command found
        }
        pop(@chunks);
    }
    return;
}


# Build options for different environments
my @conditional_params;
if (not grep { $_ eq 'PERL_CORE=1' } @ARGV) {
    # CPAN

    # Verify that a 'C' compiler is available
    if (! have_cc()) {
        die("OS unsupported:  ERROR: No 'C' compiler found to build 'threads::shared'\n");
    }

    push(@conditional_params, 'DEFINE' => '-DHAS_PPPORT_H',
                              'PREREQ_PM' => {
                                    'strict'            => 0,
                                    'warnings'          => 0,
                                    'Config'            => 0,
                                    'Carp'              => 0,
                                    'XSLoader'          => 0,
                                    'Scalar::Util'      => 0,
                                    'threads'           => 1.73,

                                    'Test'              => 0,
                                    'Test::More'        => 0,
                                    'ExtUtils::testlib' => 0,
                              });
}


# Create Makefile
WriteMakefile(
    'NAME'              => 'threads::shared',
    'AUTHOR'            => 'Artur Bergman, Jerry D. Hedden <jdhedden AT cpan DOT org>',
    'VERSION_FROM'      => 'shared.pm',
    'ABSTRACT_FROM'     => 'shared.pm',
    'PM' => {
        'shared.pm'     => '$(INST_LIBDIR)/shared.pm',
    },
    'INSTALLDIRS'       => (($] < 5.011) ? 'perl' : 'site'),

    ((ExtUtils::MakeMaker->VERSION() lt '6.25') ?
        ('PL_FILES' => { })            : ()),
    ((ExtUtils::MakeMaker->VERSION() gt '6.30') ?
        ('LICENSE'  => 'perl')         : ()),

    @conditional_params
);

# Additional 'make' targets
sub MY::postamble
{
    return <<'_EXTRAS_';
fixfiles:
	@dos2unix `cat MANIFEST`
	@$(CHMOD) 644 `cat MANIFEST`
	@$(CHMOD) 755 examples/*.pl

ppport:
	@( cd /tmp; perl -e 'use Devel::PPPort; Devel::PPPort::WriteFile("ppport.h");' )
	@if ! cmp -s ppport.h /tmp/ppport.h; then \
	    ( tkdiff ppport.h /tmp/ppport.h & ); \
	    perl /tmp/ppport.h; \
	fi
_EXTRAS_
}

# EOF
