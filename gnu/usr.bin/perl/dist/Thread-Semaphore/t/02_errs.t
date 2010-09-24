use strict;
use warnings;

use Thread::Semaphore;

use Test::More 'tests' => 12;

my $err = qr/^Semaphore .* is not .* integer: /;

eval { Thread::Semaphore->new(undef); };
like($@, $err, $@);
eval { Thread::Semaphore->new(0.5); };
like($@, $err, $@);
eval { Thread::Semaphore->new('foo'); };
like($@, $err, $@);

my $s = Thread::Semaphore->new();
ok($s, 'New semaphore');

eval { $s->down(undef); };
like($@, $err, $@);
eval { $s->down(-1); };
like($@, $err, $@);
eval { $s->down(1.5); };
like($@, $err, $@);
eval { $s->down('foo'); };
like($@, $err, $@);

eval { $s->up(undef); };
like($@, $err, $@);
eval { $s->up(-1); };
like($@, $err, $@);
eval { $s->up(1.5); };
like($@, $err, $@);
eval { $s->up('foo'); };
like($@, $err, $@);

exit(0);

# EOF
