#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More tests => 7;

BEGIN { use_ok('Shell'); }

my $so = Shell->new;
ok($so, 'Shell->new');

my $Is_VMS     = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Is_NetWare = $^O eq 'NetWare';

$Shell::capture_stderr = 1;

# Now test that that works ..

my $tmpfile = 'sht0001';
while ( -f $tmpfile ) {
    $tmpfile++;
}
END { -f $tmpfile && (open STDERR, '>&SAVERR' and unlink $tmpfile) }


open(SAVERR, ">&STDERR");
open(STDERR, ">$tmpfile");

xXx_not_there();  # Ok someone could have a program called this :(

# On os2 the warning is on by default...
ok(($^O eq 'os2' xor !(-s $tmpfile)), '$Shell::capture_stderr');

$Shell::capture_stderr = 0;

# someone will have to fill in the blanks for other platforms

if ($Is_VMS) {
    ok(directory(), 'Execute command');
    my @files = directory('*.*');
    ok(@files, 'Quoted arguments');

    ok(eq_array(\@files, [$so->directory('*.*')]), 'object method');
    eval { $so->directory };
    ok(!$@, '2 methods calls');
} elsif ($Is_MSWin32) {
    ok(dir(), 'Execute command');
    my @files = dir('*.*');
    ok(@files, 'Quoted arguments');

    ok(eq_array(\@files, [$so->dir('*.*')]), 'object method');
    eval { $so->dir };
    ok(!$@, '2 methods calls');
} else {
    ok(ls(), 'Execute command');
    my @files = ls('*');
    ok(@files, 'Quoted arguments');

    ok(eq_array(\@files, [$so->ls('*')]), 'object method');
    eval { $so->ls };
    ok(!$@, '2 methods calls');

}
open(STDERR, ">&SAVERR") ;
