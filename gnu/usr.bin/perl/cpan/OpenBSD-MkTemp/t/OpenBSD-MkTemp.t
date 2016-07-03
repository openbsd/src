# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl OpenBSD-MkTemp.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More;
use Errno;
use FileHandle;
use Scalar::Util qw( openhandle );
BEGIN { use_ok('OpenBSD::MkTemp') };

#########################

my $tmpdir = $ENV{TMPDIR} || "/tmp";
my $top_base = "$tmpdir/test.";

my $dir = OpenBSD::MkTemp::mkdtemp($top_base . "X" x 10);

if (! $dir) {
    BAIL_OUT("unable to create test directory: $!\n(is $tmpdir unwritable?)");
}

# clean things up afterwards
eval 'END { my $ret = $?; system("rm", "-rf", $dir); $? = $ret }';

like($dir, qr/^\Q$top_base\E[a-zA-Z0-9]{10}$/, "mkdtemp output format");
ok(-d $dir, "mkdtemp created directory");
my $mode = (stat(_))[2];
cmp_ok($mode & 07777, '==', 0700, "mkdtemp directory mode");


my $base = "$dir/f.";
my $template = $base . "X" x 10;

my($fh1, $file1) = mkstemp($template);

like($file1, qr/^\Q$base\E[a-zA-Z0-9]{10}$/, "mkstemp output format");
ok(-f $file1, "mkstemp created filed");
my @stat = stat(_);
cmp_ok($stat[2] & 07777, '==', 0600, "mkstemp file mode");

my @fstat = stat($fh1);
is_deeply(\@stat, \@fstat, "file name matches the handle");


my($fh2, $file2) = OpenBSD::MkTemp::mkstemps($template, ".foo");

like($file2, qr/^\Q$base\E[a-zA-Z0-9]{10}\.foo$/, "mkstemps output format");
ok(-f $file2, "mkstemps created filed");
@stat = stat(_);
cmp_ok($stat[2] & 07777, '==', 0600, "mkstemps file mode");

@fstat = stat($fh2);
is_deeply(\@stat, \@fstat, "file name matches the handle");


my $fileno = fileno($fh2);
undef $fh2;
open(F, ">$file2")		|| die "$0: unable to open $file2: $!";
cmp_ok(fileno(F), '==', $fileno, "mkstemp file handle ref counting");

subtest "mkstemp in scalar context" => sub {
    plan tests => 2;
    ok my $fh = OpenBSD::MkTemp::mkstemp($template);
    is openhandle($fh), $fh, "mkstemp returns a filehandle in scalar mode";
};

subtest "mkstemps in scalar context" => sub {
    plan tests => 2;
    ok my $fh = OpenBSD::MkTemp::mkstemps($template, ".foo");
    is openhandle($fh), $fh, "mkstemps returns a filehandle in scalar mode";
};

#
# How about some failures?
#

my $d2 = OpenBSD::MkTemp::mkdtemp($file1 . "/fXXXXXXXXXX");
my $err = $!;
subtest "mkdtemp failed on bad prefix" => sub {
    plan tests => 2;
    ok(! defined($d2),				"no directory name");
    cmp_ok($err, '==', Errno::ENOTDIR,		"right errno");
};

if ($> != 0) {
    $d2 = OpenBSD::MkTemp::mkdtemp("/fXXXXXXXXXX");
    $err = $!;
    subtest "mkdtemp failed on no access" => sub {
	plan tests => 2;
	ok(! defined($d2),			"no directory name");
	cmp_ok($err, '==', Errno::EACCES,	"right errno");
    };
}

my($fh3, $file3) = mkstemp($file1 . "/fXXXXXXXXXX");
$err = $!;
subtest "mkstemp failed on bad prefix" => sub {
    plan tests => 3;
    ok(! defined($fh3),				"no filehandle");
    ok(! defined($file3),			"no filename");
    cmp_ok($err, '==', Errno::ENOTDIR,		"right errno");
};

if ($> != 0) {
    ($fh3, $file3) = mkstemp("/fXXXXXXXXXX");
    $err = $!;
    subtest "mkstemp failed on no access" => sub {
	plan tests => 3;
	ok(! defined($fh3),			"no filehandle");
	ok(! defined($file3),			"no filename");
	cmp_ok($err, '==', Errno::EACCES,	"right errno");
    };
}

eval { OpenBSD::MkTemp::mkstemps_real("foo", 0) };
like($@, qr/read-only value/, "unwritable template");

eval { my $f = "foo"; OpenBSD::MkTemp::mkstemps_real($f, -3) };
like($@, qr/invalid suffix/, "invalid suffix");

done_testing();

