# Testing of Pod::Find
# Author: Marek Rouchal <marek@saftsack.fs.uni-bayreuth.de>

$| = 1;

BEGIN {
  if ($^O eq 'VMS') {
    print "1..0 # needs upstream patch from https://rt.cpan.org/Ticket/Display.html?id=55121";
    exit 0;
  }
}

use strict;
use Test::More tests => 4;

BEGIN {
  # 1. load successful
  use_ok('Pod::Find', qw(pod_find pod_where));
}

use File::Spec;

require Cwd;
my $THISDIR = Cwd::cwd();
my $VERBOSE = $ENV{PERL_CORE} ? 0 : ($ENV{TEST_VERBOSE} || 0);
my $lib_dir = File::Spec->catdir($THISDIR,'lib');

if ($^O eq 'VMS') {
    $lib_dir = VMS::Filespec::unixify($lib_dir);
}

print "### 2. searching $lib_dir\n";
my %pods = pod_find($lib_dir);
my @results = values %pods;
print "### found @results\n";
my @compare = qw(
    Pod::Find
    Pod::InputObjects
    Pod::ParseUtils
    Pod::Parser
    Pod::PlainText
    Pod::Select
);
if (File::Spec->case_tolerant || $^O eq 'dos') {
    # must downcase before sorting
    map {$_ = lc $_} @compare;
    map {$_ = lc $_} @results;
}
my $compare = join(',', sort @compare);
my $result = join(',', sort @results);
is($result, $compare);

print "### 3. searching for File::Find\n";
$result = pod_where({ -inc => 1, -verbose => $VERBOSE }, 'File::Find')
  || 'undef - pod not found!';
print "### found $result\n";

require Config;
$compare = $ENV{PERL_CORE} ?
      File::Spec->catfile(File::Spec->updir, File::Spec->updir, 'lib','File','Find.pm')
      : File::Spec->catfile($Config::Config{privlibexp},"File","Find.pm");
my $resfile = _canon($result);
my $cmpfile = _canon($compare);
if($^O =~ /dos|win32/i && $resfile =~ /~\d(?=\\|$)/) {
    # we have ~1 short filenames
    $resfile = quotemeta($resfile);
    $resfile =~ s/\\~\d(?=\\|$)/[^\\\\]+/g;
    ok($cmpfile =~ /^$resfile$/, "pod_where found File::Find (with long filename matching)") ||
      diag("'$cmpfile' does not match /^$resfile\$/");
} else {
    is($resfile,$cmpfile,"pod_where found File::Find");
}

# Search for a documentation pod rather than a module
my $searchpod = 'Stuff';
print "### 4. searching for $searchpod.pod\n";
$result = pod_where(
  { -dirs => [ File::Spec->catdir( qw(t), 'pod', 'testpods', 'lib', 'Pod') ],
    -verbose => $VERBOSE }, $searchpod)
  || "undef - $searchpod.pod not found!";
print "### found $result\n";

$compare = File::Spec->catfile(
    qw(t), 'pod', 'testpods', 'lib', 'Pod' ,'Stuff.pm');
is(_canon($result),_canon($compare));


# make the path as generic as possible
sub _canon
{
  my ($path) = @_;
  $path = File::Spec->canonpath($path);
  my @comp = File::Spec->splitpath($path);
  my @dir = File::Spec->splitdir($comp[1]);
  $comp[1] = File::Spec->catdir(@dir);
  $path = File::Spec->catpath(@comp);
  $path = uc($path) if File::Spec->case_tolerant;
  print "### general path: $path\n" if $VERBOSE;
  $path;
}

