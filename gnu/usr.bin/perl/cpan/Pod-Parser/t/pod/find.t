# Testing of Pod::Find
# Author: Marek Rouchal <marek@saftsack.fs.uni-bayreuth.de>

$| = 1;

BEGIN {
  if ($^O eq 'VMS') {
    print "1..0 # needs upstream patch from https://rt.cpan.org/Ticket/Display.html?id=55121";
    exit 0;
  }
}

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

my $vms_unix_rpt = 0;
my $vms_efs = 0;
my $unix_mode = 1;

if ($^O eq 'VMS') {
    $lib_dir = VMS::Filespec::unixify(File::Spec->catdir($THISDIR,'-','lib','pod'));
    $Qlib_dir = $lib_dir;
    $Qlib_dir =~ s#\/#::#g;

    $unix_mode = 0;
    if (eval 'require VMS::Feature') {
        $vms_unix_rpt = VMS::Feature::current("filename_unix_report");
        $vms_efs = VMS::Feature::current("efs_charset");
    } else {
        my $unix_rpt = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        my $efs_charset = $ENV{'DECC$EFS_CHARSET'} || '';
        $vms_unix_rpt = $unix_rpt =~ /^[ET1]/i; 
        $vms_efs = $efs_charset =~ /^[ET1]/i; 
    }

    # Traditional VMS mode only if VMS is not in UNIX compatible mode.
    $unix_mode = ($vms_efs && $vms_unix_rpt);
}

print "### 2. searching $lib_dir\n";
my %pods = pod_find($lib_dir);
my $result = join(',', sort values %pods);
print "### found $result\n";
my $compare = join(',', sort qw(
    Pod::Checker
    Pod::Find
    Pod::InputObjects
    Pod::ParseUtils
    Pod::Parser
    Pod::PlainText
    Pod::Select
    Pod::Usage
));
if ($^O eq 'VMS') {
    $compare = lc($compare);
    my $undollared = $Qlib_dir;
    $undollared =~ s/\$/\\\$/g;
    $undollared =~ s/\-/\\\-/g;
    $result =~ s/$undollared/pod::/g;
    $result =~ s/\$//g;
    my $count = 0;
    my @result = split(/,/,$result);
    my @compare = split(/,/,$compare);
    foreach(@compare) {
        $count += grep {/$_/} @result;
    }
    is($count/($#result+1)-1,$#compare);
}
elsif (File::Spec->case_tolerant || $^O eq 'dos') {
    is(lc $result,lc $compare);
}
else {
    is($result,$compare);
}

print "### 3. searching for File::Find\n";
$result = pod_where({ -inc => 1, -verbose => $VERBOSE }, 'File::Find')
  || 'undef - pod not found!';
print "### found $result\n";

require Config;
if ($^O eq 'VMS') { # privlib is perl_root:[lib] OK but not under mms
    if ($unix_mode) {
        $compare = "../lib/File/Find.pm";
    } else {
        $compare = "lib.File]Find.pm";
    }
    $result =~ s/perl_root:\[\-?\.?//i;
    $result =~ s/\[\-?\.?//i; # needed under `mms test`
    is($result,$compare);
}
else {
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

