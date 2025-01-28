use strict;
use warnings;
use Test2::Tools::Defer;
# HARNESS-NO-FORK

my $file = __FILE__;

my $START_LINE;
BEGIN {
    $START_LINE = __LINE__;
    def ok => (1, "truth");
    def is => (1, 1, "1 is 1");
    def is => ({}, {}, "hash is hash");

    def ok => (0, 'lies');
    def is => (0, 1, "1 is not 0");
    def is => ({}, [], "a hash is not an array");
}

use Test2::Bundle::Extended -target => 'Test2::Tools::Defer';

sub capture(&) {
    my $code = shift;

    my ($err, $out) = ("", "");

    my ($ok, $e);
    {
        local *STDOUT;
        local *STDERR;

        ($ok, $e) = Test2::Util::try(sub {
            open(STDOUT, '>', \$out) or die "Failed to open a temporary STDOUT: $!";
            open(STDERR, '>', \$err) or die "Failed to open a temporary STDERR: $!";

            $code->();
        });
    }

    die $e unless $ok;

    return {
        STDOUT => $out,
        STDERR => $err,
    };
}

is(
    intercept { do_def },
    array {
        filter_items { grep { $_->isa('Test2::Event::Ok') || $_->isa('Test2::Event::Fail') } @_ };

        event Ok => sub {
            call pass => 1;
            call name => 'truth';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 1;
            prop package => __PACKAGE__;
        };

        event Ok => sub {
            call pass => 1;
            call name => '1 is 1';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 2;
            prop package => __PACKAGE__;
        };

        event Ok => sub {
            call pass => 1;
            call name => 'hash is hash';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 3;
            prop package => __PACKAGE__;
        };

        event Ok => sub {
            call pass => 0;
            call name => 'lies';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 5;
            prop package => __PACKAGE__;
        };

        event Fail => sub {
            call name => '1 is not 0';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 6;
            prop package => __PACKAGE__;
        };

        event Fail => sub {
            call name => 'a hash is not an array';
            prop file => "(eval in Test2::Tools::Defer) " . __FILE__;
            prop line => $START_LINE + 7;
            prop package => __PACKAGE__;
        };

        end;
    },
    "got expected events"
);

def ok => (1, "truth");
def is => (1, 1, "1 is 1");
def is => ({}, {}, "hash is hash");

# Actually run some that pass
do_def();

like(
    dies { do_def() },
    qr/No tests to run/,
    "Fails if there are no tests"
);

my $line1 = __LINE__ + 1;
sub oops { die 'oops' }

my $line2 = __LINE__ + 1;
def oops => (1);
like( dies { do_def() }, <<EOT, "Exceptions in the test are propagated");
Exception: oops at $file line $line1.
--eval--
package main;
# line $line2 "(eval in Test2::Tools::Defer) $file"
&oops(\@\$args);
1;
--------
Tool:   oops
Caller: main, $file, $line2
\$args:  [
          1
        ];
EOT


{
    {
        package Foo;
        main::def ok => (1, "pass");
    }
    def ok => (1, "pass");

    my $new_exit = 0;
    my $out = capture { Test2::Tools::Defer::_verify(undef, 0, \$new_exit) };

    is($new_exit, 255, "exit set to 255 due to unrun tests");
    like(
        $out->{STDOUT},
        qr/not ok - deferred tests were not run/,
        "Got failed STDOUT line"
    );

    like(
        $out->{STDERR},
        qr/# 'main' has deferred tests that were never run/,
        "We see that main failed"
    );

    like(
        $out->{STDERR},
        qr/# 'Foo' has deferred tests that were never run/,
        "We see that Foo failed"
    );
}

{
    local $? = 101;
    def ok => (1, "pass");
    my $out = capture { Test2::Tools::Defer::_verify() };
    is($?, 101, "did not change exit code");
    like(
        $out->{STDOUT},
        qr/not ok - deferred tests were not run/,
        "Got failed STDOUT line"
    );

    like(
        $out->{STDERR},
        qr/# 'main' has deferred tests that were never run/,
        "We see that main failed"
    );
}

done_testing;
