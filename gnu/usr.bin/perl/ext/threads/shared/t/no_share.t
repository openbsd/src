use warnings;

BEGIN {
#    chdir 't' if -d 't';
#    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
       print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
    $SIG{__WARN__} = sub { $warnmsg = shift; };
}


sub ok {
    my ($id, $ok, $name) = @_;

    $name = '' unless defined $name;
    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    return $ok;
}

our $warnmsg;
use ExtUtils::testlib;
use strict;
BEGIN { print "1..5\n" };
use threads::shared;
use threads;
ok(1,1,"loaded");
ok(2,$warnmsg =~ /Warning, threads::shared has already been loaded/,
    "threads has warned us");
my $test = "bar";
share($test);
ok(3,$test eq "bar","Test disabled share not interfering");
threads->create(
               sub {
                   ok(4,$test eq "bar","Test disabled share after thread");
                   $test = "baz";
                   })->join();
# Value should either remain unchanged or be value set by other thread
ok(5,$test eq "bar" || $test eq 'baz',"Test that value is an expected one");


