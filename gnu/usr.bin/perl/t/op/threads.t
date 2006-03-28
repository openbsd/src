#!./perl
BEGIN {
     chdir 't' if -d 't';
     @INC = '../lib';
     require './test.pl';	# for which_perl() etc
     $| = 1;
}

use strict;
use Config;

BEGIN {
     if (!$Config{useithreads}) {
	print "1..0 # Skip: no ithreads\n";
	exit 0;
     }
     if ($ENV{PERL_CORE_MINITEST}) {
       print "1..0 # Skip: no dynamic loading on miniperl, no threads\n";
       exit 0;
     }
     plan(3);
}
use threads;

# test that we don't get:
# Attempt to free unreferenced scalar: SV 0x40173f3c
fresh_perl_is(<<'EOI', 'ok', { }, 'delete() under threads');
use threads;
threads->new(sub { my %h=(1,2); delete $h{1}})->join for 1..2;
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
threads->new(sub { 1 })->join for (1..1);
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
threads->new(sub { $ref = $object } )->join; # $ref = $object causes problems
print "ok";
EOI
