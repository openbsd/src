#!/perl -w
use 5.010;
use strict;

# This tests properties of dual-life modules:
#
# * Are all dual-life programs being generated in utils/?

chdir 't';
require './test.pl';

use Config;
if ( $Config{usecrosscompile} ) {
  skip_all( "Not all files are available during cross-compilation" );
}

plan('no_plan');

use File::Basename;
use File::Find;
use File::Spec::Functions;

# Exceptions that are found in dual-life bin dirs but aren't
# installed by default; some occur only during testing:
my $not_installed = qr{^(?:
  \.\./cpan/Archive-Tar/bin/ptar.*
   |
  \.\./cpan/JSON-PP/bin/json_pp
   |
  \.\./cpan/IO-Compress/bin/zipdetails
   |
  \.\./cpan/Encode/bin/u(?:cm(?:2table|lint|sort)|nidump)
   |
  \.\./cpan/Module-(?:Metadata|Build)
                               /MB-[\w\d]+/Simple/(?:test_install/)?bin/.*
)\z}ix;

my %dist_dir_exe;

$dist_dir_exe{lc "podselect.PL"} = "../cpan/Pod-Parser/podselect";
$dist_dir_exe{lc "podchecker.PL"} = "../cpan/Pod-Checker/podchecker";
$dist_dir_exe{lc "pod2usage.PL"} = "../cpan/Pod-Usage/pod2usage";

foreach (qw (pod2man pod2text)) {
    $dist_dir_exe{lc "$_.PL"} = "../cpan/podlators/$_";
};
$dist_dir_exe{'pod2html.pl'} = '../ext/Pod-Html';

my @programs;

find(
  { no_chidr => 1, wanted => sub {
    my $name = $File::Find::name;
    return if $name =~ /blib/;
    return unless $name =~ m{/(?:bin|scripts?)/\S+\z} && $name !~ m{/t/};

    push @programs, $name;
  }},
  qw( ../cpan ../dist ../ext ),
);

my $ext = $^O eq 'VMS' ? '.com' : '';

for my $f ( @programs ) {
  $f =~ s/\.\z// if $^O eq 'VMS';
  next if $f =~ $not_installed;
  my $bn = basename($f);
  if(grep { /\A(?i:$bn)\z/ } keys %dist_dir_exe) {
    ok( -f "$dist_dir_exe{lc $bn}$ext", "$f$ext");
  } else {
    ok( -f catfile('..', 'utils', "$bn$ext"), "$f$ext" );
  }
}

