#!./perl

BEGIN { unshift(@INC,'../lib') if -d '../lib'; }

use VMS::Filespec;
use File::Spec;

foreach (<DATA>) {
  chomp;
  s/\s*#.*//;
  next if /^\s*$/;
  push(@tests,$_);
}

require './test.pl';
plan(tests => scalar(2*@tests)+6);

foreach $test (@tests) {
  ($arg,$func,$expect) = split(/\s+/,$test);

  $expect = undef if $expect eq 'undef';
  $rslt = eval "$func('$arg')";
  is($@, '', "eval ${func}('$arg')");
  is($rslt, $expect, "${func}('$arg'): '$rslt'");
}

$defwarn = <<'EOW';
# Note: This failure may have occurred because your default device
# was set using a non-concealed logical name.  If this is the case,
# you will need to determine by inspection that the two resultant
# file specifications shown above are in fact equivalent.
EOW

is(uc(rmsexpand('[]')),   "\U$ENV{DEFAULT}", 'rmsexpand()') || print $defwarn;
is(rmsexpand('from.here'),"\L$ENV{DEFAULT}from.here") || print $defwarn;
is(rmsexpand('from'),     "\L$ENV{DEFAULT}from")      || print $defwarn;

is(rmsexpand('from.here','cant:[get.there];2'),
   'cant:[get.there]from.here;2')                     || print $defwarn;


# Make sure we're using redirected mkdir, which strips trailing '/', since
# the CRTL's mkdir can't handle this.
ok(mkdir('testdir/',0777),      'using redirected mkdir()');
ok(rmdir('testdir/'),           '    rmdir()');

__DATA__

# lots of underscores used to minimize collision with existing logical names

# Basic VMS to Unix filespecs
__some_:[__where_.__over_]__the_.__rainbow_    unixify /__some_/__where_/__over_/__the_.__rainbow_
[.__some_.__where_.__over_]__the_.__rainbow_   unixify __some_/__where_/__over_/__the_.__rainbow_
[-.__some_.__where_.__over_]__the_.__rainbow_  unixify ../__some_/__where_/__over_/__the_.__rainbow_
[.__some_.--.__where_.__over_]__the_.__rainbow_        unixify __some_/../../__where_/__over_/__the_.__rainbow_
[.__some_...__where_.__over_]__the_.__rainbow_ unixify __some_/.../__where_/__over_/__the_.__rainbow_
[...__some_.__where_.__over_]__the_.__rainbow_ unixify .../__some_/__where_/__over_/__the_.__rainbow_
[.__some_.__where_.__over_...]__the_.__rainbow_        unixify __some_/__where_/__over_/.../__the_.__rainbow_
[.__some_.__where_.__over_...] unixify __some_/__where_/__over_/.../
[.__some_.__where_.__over_.-]  unixify __some_/__where_/__over_/../
[]	unixify		./
[-]	unixify		../
[--]	unixify		../../
[...]	unixify		.../

# and back again
/__some_/__where_/__over_/__the_.__rainbow_    vmsify  __some_:[__where_.__over_]__the_.__rainbow_
__some_/__where_/__over_/__the_.__rainbow_     vmsify  [.__some_.__where_.__over_]__the_.__rainbow_
../__some_/__where_/__over_/__the_.__rainbow_  vmsify  [-.__some_.__where_.__over_]__the_.__rainbow_
__some_/../../__where_/__over_/__the_.__rainbow_       vmsify  [-.__where_.__over_]__the_.__rainbow_
.../__some_/__where_/__over_/__the_.__rainbow_ vmsify  [...__some_.__where_.__over_]__the_.__rainbow_
__some_/.../__where_/__over_/__the_.__rainbow_ vmsify  [.__some_...__where_.__over_]__the_.__rainbow_
/__some_/.../__where_/__over_/__the_.__rainbow_        vmsify  __some_:[...__where_.__over_]__the_.__rainbow_
__some_/__where_/...   vmsify  [.__some_.__where_...]
/__where_/...  vmsify  __where_:[...]
.	vmsify	[]
..	vmsify	[-]
../..	vmsify	[--]
.../	vmsify	[...]
/	vmsify	sys$disk:[000000]

# Fileifying directory specs
__down_:[__the_.__garden_.__path_]     fileify __down_:[__the_.__garden_]__path_.dir;1
[.__down_.__the_.__garden_.__path_]    fileify [.__down_.__the_.__garden_]__path_.dir;1
/__down_/__the_/__garden_/__path_      fileify /__down_/__the_/__garden_/__path_.dir;1
/__down_/__the_/__garden_/__path_/     fileify /__down_/__the_/__garden_/__path_.dir;1
__down_/__the_/__garden_/__path_       fileify __down_/__the_/__garden_/__path_.dir;1
__down_:[__the_.__garden_]__path_      fileify __down_:[__the_.__garden_]__path_.dir;1
__down_:[__the_.__garden_]__path_.     fileify # N.B. trailing . ==> null type
__down_:[__the_]__garden_.__path_      fileify undef
/__down_/__the_/__garden_/__path_.     fileify # N.B. trailing . ==> null type
/__down_/__the_/__garden_.__path_      fileify undef

# and pathifying them
__down_:[__the_.__garden_]__path_.dir;1        pathify __down_:[__the_.__garden_.__path_]
[.__down_.__the_.__garden_]__path_.dir pathify [.__down_.__the_.__garden_.__path_]
/__down_/__the_/__garden_/__path_.dir  pathify /__down_/__the_/__garden_/__path_/
__down_/__the_/__garden_/__path_.dir   pathify __down_/__the_/__garden_/__path_/
__down_:[__the_.__garden_]__path_      pathify __down_:[__the_.__garden_.__path_]
__down_:[__the_.__garden_]__path_.     pathify # N.B. trailing . ==> null type
__down_:[__the_]__garden_.__path_      pathify undef
/__down_/__the_/__garden_/__path_.     pathify # N.B. trailing . ==> null type
/__down_/__the_/__garden_.__path_      pathify undef
__down_:[__the_.__garden_]__path_.dir;2        pathify #N.B. ;2
__path_        pathify __path_/
/__down_/__the_/__garden_/.    pathify /__down_/__the_/__garden_/./
/__down_/__the_/__garden_/..   pathify /__down_/__the_/__garden_/../
/__down_/__the_/__garden_/...  pathify /__down_/__the_/__garden_/.../
__path_.notdir pathify undef

# Both VMS/Unix and file/path conversions
__down_:[__the_.__garden_]__path_.dir;1        unixpath        /__down_/__the_/__garden_/__path_/
/__down_/__the_/__garden_/__path_      vmspath __down_:[__the_.__garden_.__path_]
__down_:[__the_.__garden_.__path_]     unixpath        /__down_/__the_/__garden_/__path_/
__down_:[__the_.__garden_.__path_...]  unixpath        /__down_/__the_/__garden_/__path_/.../
/__down_/__the_/__garden_/__path_.dir  vmspath __down_:[__the_.__garden_.__path_]
[.__down_.__the_.__garden_]__path_.dir unixpath        __down_/__the_/__garden_/__path_/
__down_/__the_/__garden_/__path_       vmspath [.__down_.__the_.__garden_.__path_]
__path_        vmspath [.__path_]
/	vmspath	sys$disk:[000000]

# Redundant characters in Unix paths
//__some_/__where_//__over_/../__the_.__rainbow_       vmsify  __some_:[__where_]__the_.__rainbow_
/__some_/__where_//__over_/./__the_.__rainbow_ vmsify  __some_:[__where_.__over_]__the_.__rainbow_
..//../	vmspath	[--]
./././	vmspath	[]
./../.	vmsify	[-]

# Our override of File::Spec->canonpath can do some strange things
__dev:[__dir.000000]__foo     File::Spec->canonpath   __dev:[__dir.000000]__foo
__dev:[__dir.][000000]__foo   File::Spec->canonpath   __dev:[__dir]__foo
