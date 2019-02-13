#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  Copyright (c) 2017, Reini Urban
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use ExtUtils::MakeMaker;
use Config;
use File::Copy qw(move copy);
use File::Spec;

unlink "lib/Storable/Limit.pm";

my $pm = { 'Storable.pm' => '$(INST_ARCHLIB)/Storable.pm' };
unless ($ENV{PERL_CORE}) {
    # the core Makefile takes care of this for core builds
    $pm->{"lib/Storable/Limit.pm"} = '$(INST_ARCHLIB)/Storable/Limit.pm';
}

WriteMakefile(
    NAME                => 'Storable',
    DISTNAME            => "Storable",
# We now ship this in t/
#    PREREQ_PM           => { 'Test::More' => '0.41' },
    PL_FILES        => { }, # prevent default behaviour
    PM              => $pm,
    PREREQ_PM           => { XSLoader => 0 },
    INSTALLDIRS => ($] >= 5.007 && $] < 5.012) ? 'perl' : 'site',
    VERSION_FROM    => '__Storable__.pm',
    ABSTRACT_FROM   => '__Storable__.pm',
    ($ExtUtils::MakeMaker::VERSION > 6.45 ?
     (META_MERGE        => { resources =>
                               { bugtracker => 'http://rt.perl.org/perlbug/' },
                            provides    => {
                                'Storable'  => {
                                    file        => 'Storable_pm.PL',
                                    version     => MM->parse_version('__Storable__.pm'),
                                },
                            },

                           },
    ) : ()),
    dist                => { SUFFIX => 'gz', COMPRESS => 'gzip -f' },
    clean               => { FILES => 'Storable-* Storable.pm lib' },
);

# Unlink the .pm file included with the distribution
1 while unlink "Storable.pm";

my $ivtype = $Config{ivtype};

# I don't know if the VMS folks ever supported long long on 5.6.x
if ($ivtype and $ivtype eq 'long long' and $^O !~ /^MSWin/) {
  print <<'EOM';

You appear to have a perl configured to use 64 bit integers in its scalar
variables.  If you have existing data written with an earlier version of
Storable which this version of Storable refuses to load with a

   Byte order is not compatible

error, then please read the section "64 bit data in perl 5.6.0 and 5.6.1"
in the Storable documentation for instructions on how to read your data.

(You can find the documentation at the end of Storable.pm in POD format)

EOM
}

# compute the maximum stacksize, before and after linking
package MY;

# FORCE finish of INST_DYNAMIC, avoid loading the old Storable (failed XS_VERSION check)
sub xlinkext {
    my $s = shift->SUPER::linkext(@_);
    $s =~ s|( :: .*)| $1 FORCE stacksize|;
    $s
}

sub depend {
    my $extra_deps = "";
    my $options = "";
    if ($ENV{PERL_CORE}) {
        $options = "--core";
    }
    else {
        # blib.pm needs arch/lib
        $extra_deps = ' Storable.pm';
    }
    my $linktype = uc($_[0]->{LINKTYPE});
    my $limit_pm = File::Spec->catfile('lib', 'Storable', 'Limit.pm');
    "
$limit_pm : stacksize \$(INST_$linktype)$extra_deps
	\$(MKPATH) \$(INST_LIB)
	\$(FULLPERLRUNINST) stacksize $options

release : dist
	git tag \$(VERSION)
	cpan-upload \$(DISTVNAME).tar\$(SUFFIX)
	git push
	git push --tags
"
}

sub test {
    my ($self, %attr) = @_;

    my $out = $self->SUPER::test(%attr);

    if ($ENV{PERL_CORE}) {
        $out =~ s!^(test(?:db)?_(?:static|dynamic)\b.*)!$1 lib/Storable/Limit.pm!gm;
    }

    $out;
}

sub postamble {
'
all :: Storable.pm
	$(NOECHO) $(NOOP)

Storable.pm :: Storable.pm.PL __Storable__.pm
	$(PERLRUN) Storable.pm.PL
'
}
