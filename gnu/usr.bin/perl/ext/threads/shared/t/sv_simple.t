use warnings;

BEGIN {
#    chdir 't' if -d 't';
#    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}


sub ok {
    my ($id, $ok, $name) = @_;

    $name = '' unless defined $name;
    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    return $ok;
}


use ExtUtils::testlib;
use strict;
BEGIN { print "1..10\n" };
use threads;
use threads::shared;
ok(1,1,"loaded");
my $test = "bar";
share($test);
ok(2,$test eq "bar","Test magic share fetch");
$test = "foo";
ok(3,$test eq "foo","Test magic share assign");
my $c = threads::shared::_refcnt($test);
threads->create(
		sub {
		    ok(4, $test eq "foo","Test magic share fetch after thread");
		    $test = "baz";
                    ok(5,threads::shared::_refcnt($test) > $c, "Check that threadcount is correct");
		    })->join();
ok(6,$test eq "baz","Test that value has changed in another thread");
ok(7,threads::shared::_refcnt($test) == $c,"Check thrcnt is down properly");
$test = "barbar";
ok(8, length($test) == 6, "Check length code");
threads->create(sub { $test = "barbarbar" })->join;
ok(9, length($test) == 9, "Check length code after different thread modified it");
threads->create(sub { undef($test)})->join();
ok(10, !defined($test), "Check undef value");







