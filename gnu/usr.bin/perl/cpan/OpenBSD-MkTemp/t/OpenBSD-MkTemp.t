# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl OpenBSD-MkTemp.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use warnings;

use Test::More;
use Errno;
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


#
# How about some failures?
#

sub test_failure
{
    my($description, $func, $template, $expected_errno) = @_;
    my(@results, $err, $msg);

    eval {
	no strict 'refs';
	@results = &{"OpenBSD::MkTemp::$func"}($template)
    };
    $err = $!; $msg = $@;
    subtest $description => sub {
	plan tests => 3;
	ok(@results == 0,				"empty return");
	cmp_ok($err, '==', $expected_errno,		"correct errno");
	like($msg, qr/Unable to \Q$func\E\(\Q$template\E\): /,
							"correct die message");
    };
}

test_failure("mkdtemp failed on bad prefix", "mkdtemp",
	$file1 . "/fXXXXXXXXXX", Errno::ENOTDIR);
test_failure("mkdtemp failed on no access", "mkdtemp",
	"/fXXXXXXXXXX", Errno::EACCES)				if $> != 0;

test_failure("mkstemp failed on bad prefix", "mkstemp",
	$file1 . "/fXXXXXXXXXX", Errno::ENOTDIR);
test_failure("mkstemp failed on no access", "mkstemp",
	"/fXXXXXXXXXX", Errno::EACCES)				if $> != 0;

done_testing();

