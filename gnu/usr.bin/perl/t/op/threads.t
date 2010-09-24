#!perl

BEGIN {
     chdir 't' if -d 't';
     @INC = '../lib';
     require './test.pl';
     $| = 1;

     require Config;
     if (!$Config::Config{useithreads}) {
        print "1..0 # Skip: no ithreads\n";
        exit 0;
     }
     if ($ENV{PERL_CORE_MINITEST}) {
       print "1..0 # Skip: no dynamic loading on miniperl, no threads\n";
       exit 0;
     }

     plan(18);
}

use strict;
use warnings;
use threads;

# test that we don't get:
# Attempt to free unreferenced scalar: SV 0x40173f3c
fresh_perl_is(<<'EOI', 'ok', { }, 'delete() under threads');
use threads;
threads->create(sub { my %h=(1,2); delete $h{1}})->join for 1..2;
print "ok";
EOI

#PR24660
# test that we don't get:
# Attempt to free unreferenced scalar: SV 0x814e0dc.
fresh_perl_is(<<'EOI', 'ok', { }, 'weaken ref under threads');
use threads;
use Scalar::Util;
my $data = "a";
my $obj = \$data;
my $copy = $obj;
Scalar::Util::weaken($copy);
threads->create(sub { 1 })->join for (1..1);
print "ok";
EOI

#PR24663
# test that we don't get:
# panic: magic_killbackrefs.
# Scalars leaked: 3
fresh_perl_is(<<'EOI', 'ok', { }, 'weaken ref #2 under threads');
package Foo;
sub new { bless {},shift }
package main;
use threads;
use Scalar::Util qw(weaken);
my $object = Foo->new;
my $ref = $object;
weaken $ref;
threads->create(sub { $ref = $object } )->join; # $ref = $object causes problems
print "ok";
EOI

#PR30333 - sort() crash with threads
sub mycmp { length($b) <=> length($a) }

sub do_sort_one_thread {
   my $kid = shift;
   print "# kid $kid before sort\n";
   my @list = ( 'x', 'yy', 'zzz', 'a', 'bb', 'ccc', 'aaaaa', 'z',
                'hello', 's', 'thisisalongname', '1', '2', '3',
                'abc', 'xyz', '1234567890', 'm', 'n', 'p' );

   for my $j (1..99999) {
      for my $k (sort mycmp @list) {}
   }
   print "# kid $kid after sort, sleeping 1\n";
   sleep(1);
   print "# kid $kid exit\n";
}

sub do_sort_threads {
   my $nthreads = shift;
   my @kids = ();
   for my $i (1..$nthreads) {
      my $t = threads->create(\&do_sort_one_thread, $i);
      print "# parent $$: continue\n";
      push(@kids, $t);
   }
   for my $t (@kids) {
      print "# parent $$: waiting for join\n";
      $t->join();
      print "# parent $$: thread exited\n";
   }
}

do_sort_threads(2);        # crashes
ok(1);

# Change 24643 made the mistake of assuming that CvCONST can only be true on
# XSUBs. Somehow it can also end up on perl subs.
fresh_perl_is(<<'EOI', 'ok', { }, 'cloning constant subs');
use constant x=>1;
use threads;
$SIG{__WARN__} = sub{};
async sub {};
print "ok";
EOI

# From a test case by Tim Bunce in
# http://www.nntp.perl.org/group/perl.perl5.porters/63123
fresh_perl_is(<<'EOI', 'ok', { }, 'Ensure PL_linestr can be cloned');
use threads;
print do 'op/threads_create.pl' || die $@;
EOI


TODO: {
    no strict 'vars';   # Accessing $TODO from test.pl
    local $TODO = 'refcount issues with threads';

# Scalars leaked: 1
foreach my $BLOCK (qw(CHECK INIT)) {
    fresh_perl_is(<<EOI, 'ok', { }, "threads in $BLOCK block");
        use threads;
        $BLOCK { threads->create(sub {})->join; }
        print 'ok';
EOI
}

} # TODO

# Scalars leaked: 1
fresh_perl_is(<<'EOI', 'ok', { }, 'Bug #41138');
    use threads;
    leak($x);
    sub leak
    {
        local $x;
        threads->create(sub {})->join();
    }
    print 'ok';
EOI


# [perl #45053] Memory corruption with heavy module loading in threads
#
# run-time usage of newCONSTSUB (as done by the IO boot code) wasn't
# thread-safe - got occasional coredumps or malloc corruption
{
    local $SIG{__WARN__} = sub {};   # Ignore any thread creation failure warnings
    my @t;
    for (1..100) {
        my $thr = threads->create( sub { require IO });
        last if !defined($thr);      # Probably ran out of memory
        push(@t, $thr);
    }
    $_->join for @t;
    ok(1, '[perl #45053]');
}

sub matchit {
    is (ref $_[1], "Regexp");
    like ($_[0], $_[1]);
}

threads->new(\&matchit, "Pie", qr/pie/i)->join();

# tests in threads don't get counted, so
curr_test(curr_test() + 2);


# the seen_evals field of a regexp was getting zeroed on clone, so
# within a thread it didn't  know that a regex object contrained a 'safe'
# re_eval expression, so it later died with 'Eval-group not allowed' when
# you tried to interpolate the object

sub safe_re {
    my $re = qr/(?{1})/;	# this is literal, so safe
    eval { "a" =~ /$re$re/ };	# interpolating safe values, so safe
    ok($@ eq "", 'clone seen-evals');
}
threads->new(\&safe_re)->join();

# tests in threads don't get counted, so
curr_test(curr_test() + 1);

# This used to crash in 5.10.0 [perl #64954]

undef *a;
threads->new(sub {})->join;
pass("undefing a typeglob doesn't cause a crash during cloning");


# Test we don't get:
# panic: del_backref during global destruction.
# when returning a non-closure sub from a thread and subsequently starting
# a new thread.
fresh_perl_is(<<'EOI', 'ok', { }, 'No del_backref panic [perl #70748]');
use threads;
sub foo { return (sub { }); }
my $bar = threads->create(\&foo)->join();
threads->create(sub { })->join();
print "ok";
EOI

# Another, more reliable test for the same del_backref bug:
fresh_perl_like(
 <<'   EOJ', qr/ok/, {}, 'No del_backref panic [perl #70748] (2)'
   use threads;
   push @bar, threads->create(sub{sub{}})->join() for 1...10;
   print "ok";
   EOJ
);

# Simple closure-returning test: At least this case works (though it
# leaks), and we don't want to break it.
fresh_perl_like(<<'EOJ', qr/^foo\n/, {}, 'returning a closure');
use threads;
print create threads sub {
 my $x = "foo\n";
 sub{sub{$x}}
}=>->join->()()
 //"undef"
EOJ

# At the point of thread creation, $h{1} is on the temps stack.
# The weak reference $a, however, is visible from the symbol table.
fresh_perl_is(<<'EOI', 'ok', { }, 'Test for 34394ecd06e704e9');
    use threads;
    %h = (1, 2);
    use Scalar::Util 'weaken';
    $a = \$h{1};
    weaken($a);
    delete $h{1} && threads->create(sub {}, shift)->join();
    print 'ok';
EOI

# EOF
