#!./perl

BEGIN {
    if ($ENV{PERL_CORE}) {
	require Config; import Config;
	if ($] < 5.00326 || not $Config{'d_readdir'}) {
	    print "1..0 # Skip: readdir() not available\n";
	    exit 0;
	}
    }

    require($ENV{PERL_CORE} ? "../../t/test.pl" : "./t/test.pl");
    plan(16);

    use_ok('IO::Dir');
    IO::Dir->import(DIR_UNLINK);
}

use strict;

my $DIR = $^O eq 'MacOS' ? ":" : ".";

my $CLASS = "IO::Dir";
my $dot = $CLASS->new($DIR);
ok(defined($dot));

my @a = sort <*>;
my $first;
do { $first = $dot->read } while defined($first) && $first =~ /^\./;
ok(+(grep { $_ eq $first } @a));

my @b = sort($first, (grep {/^[^.]/} $dot->read));
ok(+(join("\0", @a) eq join("\0", @b)));

ok($dot->rewind,'rewind');
my @c = sort grep {/^[^.]/} $dot->read;
ok(+(join("\0", @b) eq join("\0", @c)));

ok($dot->close,'close');
{ local $^W; # avoid warnings on invalid dirhandle
ok(!$dot->rewind, "rewind on closed");
ok(!defined($dot->read));
}

open(FH,'>X') || die "Can't create x";
print FH "X";
close(FH) or die "Can't close: $!";

my %dir;
tie %dir, $CLASS, $DIR;
my @files = keys %dir;

# I hope we do not have an empty dir :-)
ok(scalar @files);

my $stat = $dir{'X'};
isa_ok($stat,'File::stat');
ok(defined($stat) && $stat->size == 1);

delete $dir{'X'};

ok(-f 'X');

my %dirx;
tie %dirx, $CLASS, $DIR, DIR_UNLINK;

my $statx = $dirx{'X'};
isa_ok($statx,'File::stat');
ok(defined($statx) && $statx->size == 1);

delete $dirx{'X'};

ok(!(-f 'X'));
