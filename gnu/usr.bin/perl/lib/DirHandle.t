#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (not $Config{'d_readdir'}) {
	print "1..0\n";
	exit 0;
    }
}

use DirHandle;
require './test.pl';

plan(5);

# Fetching the list of files in two different ways and expecting them 
# to be the same is a race condition when tests are running in parallel.
# So go somewhere quieter.
my $chdir;
if ($ENV{PERL_CORE} && -d 'uni') {
  chdir 'uni';
  $chdir++;
};

$dot = new DirHandle ($^O eq 'MacOS' ? ':' : '.');

ok(defined($dot));

@a = sort <*>;
do { $first = $dot->read } while defined($first) && $first =~ /^\./;
ok(+(grep { $_ eq $first } @a));

@b = sort($first, (grep {/^[^.]/} $dot->read));
ok(+(join("\0", @a) eq join("\0", @b)));

$dot->rewind;
@c = sort grep {/^[^.]/} $dot->read;
cmp_ok(+(join("\0", @b), 'eq', join("\0", @c)));

$dot->close;
$dot->rewind;
ok(!defined($dot->read));

if ($chdir) {
  chdir "..";
}
