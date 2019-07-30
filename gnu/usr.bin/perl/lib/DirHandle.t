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
use Test::More tests => 5;

# Fetching the list of files in two different ways and expecting them 
# to be the same is a race condition when tests are running in parallel.
# So go somewhere quieter.
my $chdir;
if ($ENV{PERL_CORE} && -d 'uni') {
  chdir 'uni';
  push @INC, '../../lib';
  $chdir++;
};

$dot = DirHandle->new('.');

is(defined $dot, 1);

@a = sort <*>;
do { $first = $dot->read } while defined($first) && $first =~ /^\./;
ok(+(grep { $_ eq $first } @a));

@b = sort($first, (grep {/^[^.]/} $dot->read));
ok(+(join("\0", @a) eq join("\0", @b)));

$dot->rewind;
@c = sort grep {/^[^.]/} $dot->read;
cmp_ok(join("\0", @b), 'eq', join("\0", @c));

$dot->close;
$dot->rewind;
is(defined $dot->read, '');

if ($chdir) {
  chdir "..";
}
