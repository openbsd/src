# Testing of Pod::Find
# Author: Marek Rouchal <marek@saftsack.fs.uni-bayreuth.de>

BEGIN {
    chdir 't' if -d 't';
    # The ../../../../../lib is for finding lib/utf8.pm
    # when running under all-utf8 settings (pod/find.t)
    # does not directly require lib/utf8.pm but regular
    # expressions will need that.
    @INC = qw(../lib ../../../../../lib);
}

$| = 1;

use Test;

BEGIN { 
  plan tests => 4; 
  use File::Spec;
}

use Pod::Find qw(pod_find pod_where);

# load successful
ok(1);

require Cwd;
my $VERBOSE = 0;
my $lib_dir = File::Spec->catdir('pod', 'testpods', 'lib');
if ($^O eq 'VMS') {
    $lib_dir = VMS::Filespec::unixify(File::Spec->catdir('pod', 'testpods', 'lib'));
    $Qlib_dir = $lib_dir;
    $Qlib_dir =~ s#\/#::#g;
}
print "### searching $lib_dir\n";
my %pods = pod_find($lib_dir);
my $result = join(',', sort values %pods);
my $compare = join(',', sort qw(
    Pod::Stuff
));
if ($^O eq 'VMS') {
    $compare = lc($compare);
    $result = join(',', sort values %pods);
    my $undollared = $Qlib_dir;
    $undollared =~ s/\$/\\\$/g;
    $undollared =~ s/\-/\\\-/g;
    $result =~ s/$undollared/pod::/g;
    my $count = 0;
    my @result = split(/,/,$result);
    my @compare = split(/,/,$compare);
    foreach(@compare) {
        $count += grep {/$_/} @result;
    }
    ok($count/($#result+1)-1,$#compare);
}
elsif ($^O eq 'dos') {
    ok(lc $result,lc $compare);
}
else {
    ok($result,$compare);
}


print "### searching for File::Find\n";
$result = pod_where({ -inc => 1, -verbose => $VERBOSE }, 'File::Find')
  || 'undef - pod not found!';
print "### found $result\n";

require Config;
if ($^O eq 'VMS') { # privlib is perl_root:[lib] OK but not under mms
    $compare = "lib.File]Find.pm";
    $result =~ s/perl_root:\[\-?\.?//i;
    $result =~ s/\[\-?\.?//i; # needed under `mms test`
    ok($result,$compare);
}
else {
    $compare = File::Spec->catfile(File::Spec->updir, 'lib','File','Find.pm');
    ok(_canon($result),_canon($compare));
}

# Search for a documentation pod rather than a module
print "### searching for Stuff.pod\n";
my $search = File::Spec->catdir('pod', 'testpods', 'lib', 'Pod');
$result = pod_where({ -dirs => [$search], -verbose => $VERBOSE }, 'Stuff')
  || 'undef - Stuff.pod not found!';
print "### found $result\n";

$compare = File::Spec->catfile('pod', 'testpods', 'lib', 'Pod' ,'Stuff.pod');
ok(_canon($result),_canon($compare));

# make the path as generic as possible
sub _canon
{
  my ($path) = @_;
  $path = File::Spec->canonpath($path);
  my @comp = File::Spec->splitpath($path);
  my @dir = File::Spec->splitdir($comp[1]);
  $comp[1] = File::Spec->catdir(@dir);
  $path = File::Spec->catpath(@dir);
  $path = uc($path) if File::Spec->case_tolerant;
  $path;
}

