#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($ENV{PERL_CORE}) {
        @INC = '../lib';
    }
}
use Cwd;

use Config;
use strict;
use warnings;
use File::Spec;
use File::Path;

use Test::More tests => 24;

my $IsVMS = $^O eq 'VMS';
my $IsMacOS = $^O eq 'MacOS';

# check imports
can_ok('main', qw(cwd getcwd fastcwd fastgetcwd));
ok( !defined(&chdir),           'chdir() not exported by default' );
ok( !defined(&abs_path),        '  nor abs_path()' );
ok( !defined(&fast_abs_path),   '  nor fast_abs_path()');


# XXX force Cwd to bootsrap its XSUBs since we have set @INC = "../lib"
# XXX and subsequent chdir()s can make them impossible to find
eval { fastcwd };

# Must find an external pwd (or equivalent) command.

my $pwd = $^O eq 'MSWin32' ? "cmd" : "pwd";
my $pwd_cmd =
    ($^O eq "NetWare") ?
        "cd" :
    ($IsMacOS) ?
        "pwd" :
        (grep { -x && -f } map { "$_/$pwd$Config{exe_ext}" }
	                   split m/$Config{path_sep}/, $ENV{PATH})[0];

$pwd_cmd = 'SHOW DEFAULT' if $IsVMS;
if ($^O eq 'MSWin32') {
    $pwd_cmd =~ s,/,\\,g;
    $pwd_cmd = "$pwd_cmd /c cd";
}
$pwd_cmd =~ s=\\=/=g if ($^O eq 'dos');

SKIP: {
    skip "No native pwd command found to test against", 4 unless $pwd_cmd;

    print "# native pwd = '$pwd_cmd'\n";

    local @ENV{qw(PATH IFS CDPATH ENV BASH_ENV)};
    my ($pwd_cmd_untainted) = $pwd_cmd =~ /^(.+)$/; # Untaint.
    chomp(my $start = `$pwd_cmd_untainted`);

    # Win32's cd returns native C:\ style
    $start =~ s,\\,/,g if ($^O eq 'MSWin32' || $^O eq "NetWare");
    # DCL SHOW DEFAULT has leading spaces
    $start =~ s/^\s+// if $IsVMS;
    SKIP: {
        skip("'$pwd_cmd' failed, nothing to test against", 4) if $?;
        skip("/afs seen, paths unlikely to match", 4) if $start =~ m|/afs/|;

	# Darwin's getcwd(3) (which Cwd.xs:bsd_realpath() uses which
	# Cwd.pm:getcwd uses) has some magic related to the PWD
	# environment variable: if PWD is set to a directory that
	# looks about right (guess: has the same (dev,ino) as the '.'?),
	# the PWD is returned.  However, if that path contains
	# symlinks, the path will not be equal to the one returned by
	# /bin/pwd (which probably uses the usual walking upwards in
	# the path -trick).  This situation is easy to reproduce since
	# /tmp is a symlink to /private/tmp.  Therefore we invalidate
	# the PWD to force getcwd(3) to (re)compute the cwd in full.
	# Admittedly fixing this in the Cwd module would be better
	# long-term solution but deleting $ENV{PWD} should not be
	# done light-heartedly. --jhi
	delete $ENV{PWD} if $^O eq 'darwin';

	my $cwd        = cwd;
	my $getcwd     = getcwd;
	my $fastcwd    = fastcwd;
	my $fastgetcwd = fastgetcwd;

	is($cwd,        $start, 'cwd()');
	is($getcwd,     $start, 'getcwd()');
	is($fastcwd,    $start, 'fastcwd()');
	is($fastgetcwd, $start, 'fastgetcwd()');
    }
}

my @test_dirs = qw{_ptrslt_ _path_ _to_ _a_ _dir_};
my $Test_Dir     = File::Spec->catdir(@test_dirs);

mkpath([$Test_Dir], 0, 0777);
Cwd::chdir $Test_Dir;

foreach my $func (qw(cwd getcwd fastcwd fastgetcwd)) {
  my $result = eval "$func()";
  is $@, '';
  dir_ends_with( $result, $Test_Dir, "$func()" );
}

# Cwd::chdir should also update $ENV{PWD}
dir_ends_with( $ENV{PWD}, $Test_Dir, 'Cwd::chdir() updates $ENV{PWD}' );
my $updir = File::Spec->updir;
Cwd::chdir $updir;
print "#$ENV{PWD}\n";
Cwd::chdir $updir;
print "#$ENV{PWD}\n";
Cwd::chdir $updir;
print "#$ENV{PWD}\n";
Cwd::chdir $updir;
print "#$ENV{PWD}\n";
Cwd::chdir $updir;
print "#$ENV{PWD}\n";

rmtree($test_dirs[0], 0, 0);

{
  my $check = ($IsVMS   ? qr|\b((?i)t)\]$| :
	       $IsMacOS ? qr|\bt:$| :
			  qr|\bt$| );
  
  like($ENV{PWD}, $check);
}

SKIP: {
    skip "no symlinks on this platform", 2 unless $Config{d_symlink};

    mkpath([$Test_Dir], 0, 0777);
    symlink $Test_Dir, "linktest";

    my $abs_path      =  Cwd::abs_path("linktest");
    my $fast_abs_path =  Cwd::fast_abs_path("linktest");
    my $want          =  File::Spec->catdir("t", $Test_Dir);

    like($abs_path,      qr|$want$|);
    like($fast_abs_path, qr|$want$|);

    rmtree($test_dirs[0], 0, 0);
    unlink "linktest";
}

if ($ENV{PERL_CORE}) {
    chdir '../ext/Cwd/t';
    unshift @INC, '../../../lib';
}

# Make sure we can run abs_path() on files, not just directories
my $path = 'cwd.t';
path_ends_with(Cwd::abs_path($path), 'cwd.t', 'abs_path() can be invoked on a file');
path_ends_with(Cwd::fast_abs_path($path), 'cwd.t', 'fast_abs_path() can be invoked on a file');

$path = File::Spec->catfile(File::Spec->updir, 't', $path);
path_ends_with(Cwd::abs_path($path), 'cwd.t', 'abs_path() can be invoked on a file');
path_ends_with(Cwd::fast_abs_path($path), 'cwd.t', 'fast_abs_path() can be invoked on a file');


#############################################
# These routines give us sort of a poor-man's cross-platform
# directory or path comparison capability.

sub bracketed_form_dir {
  return join '', map "[$_]", 
    grep length, File::Spec->splitdir(File::Spec->canonpath( shift() ));
}

sub dir_ends_with {
  my ($dir, $expect) = (shift, shift);
  my $bracketed_expect = quotemeta bracketed_form_dir($expect);
  like( bracketed_form_dir($dir), qr|$bracketed_expect$|i, (@_ ? shift : ()) );
}

sub bracketed_form_path {
  return join '', map "[$_]", 
    grep length, File::Spec->splitpath(File::Spec->canonpath( shift() ));
}

sub path_ends_with {
  my ($dir, $expect) = (shift, shift);
  my $bracketed_expect = quotemeta bracketed_form_path($expect);
  like( bracketed_form_path($dir), qr|$bracketed_expect$|i, (@_ ? shift : ()) );
}
