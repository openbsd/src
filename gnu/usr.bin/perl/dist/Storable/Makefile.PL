#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  Copyright (c) 2017, Reini Urban
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use warnings;
use ExtUtils::MakeMaker 6.31;
use Config;
use File::Copy qw(move copy);
use File::Spec;

my $pm = { 'Storable.pm' => '$(INST_ARCHLIB)/Storable.pm' };

WriteMakefile(
    NAME                => 'Storable',
    AUTHOR              => 'Perl 5 Porters',
    LICENSE             => 'perl',
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
                                    file        => '__Storable__.pm',
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
    "

release : dist
	git tag \$(VERSION)
	cpan-upload \$(DISTVNAME).tar\$(SUFFIX)
	git push
	git push --tags
"
}

sub postamble {
'
all :: Storable.pm
	$(NOECHO) $(NOOP)

Storable.pm :: Storable.pm.PL __Storable__.pm
	$(PERLRUN) Storable.pm.PL
'
}
