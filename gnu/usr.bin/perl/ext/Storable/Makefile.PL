#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use ExtUtils::MakeMaker;
use Config;

WriteMakefile(
    NAME                => 'Storable',
    DISTNAME            => "Storable",
    MAN3PODS            => {},
# We now ship this in t/
#    PREREQ_PM           => { 'Test::More' => '0.41' },
    INSTALLDIRS		=> $] >= 5.007 ? 'perl' : 'site',
    VERSION_FROM        => 'Storable.pm',
    dist                => { SUFFIX => 'gz', COMPRESS => 'gzip -f' },
);

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
