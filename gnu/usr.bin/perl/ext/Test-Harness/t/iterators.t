#!/usr/bin/perl -w

use strict;
use lib 't/lib';

use Test::More tests => 76;

use File::Spec;
use TAP::Parser;
use TAP::Parser::IteratorFactory;
use Config;

sub array_ref_from {
    my $string = shift;
    my @lines = split /\n/ => $string;
    return \@lines;
}

# we slurp __DATA__ and then reset it so we don't have to duplicate our TAP
my $offset = tell DATA;
my $tap = do { local $/; <DATA> };
seek DATA, $offset, 0;

my $did_setup    = 0;
my $did_teardown = 0;

my $setup    = sub { $did_setup++ };
my $teardown = sub { $did_teardown++ };

package NoForkProcess;
use vars qw( @ISA );
@ISA = qw( TAP::Parser::Iterator::Process );

sub _use_open3 {return}

package main;

my @schedule = (
    {   name     => 'Process',
        subclass => 'TAP::Parser::Iterator::Process',
        source   => {
            command => [
                $^X,
                File::Spec->catfile(
                    (   $ENV{PERL_CORE}
                        ? ( File::Spec->updir(), 'ext', 'Test-Harness' )
                        : ()
                    ),
                    't',
                    'sample-tests',
                    'out_err_mix'
                )
            ],
            merge    => 1,
            setup    => $setup,
            teardown => $teardown,
        },
        after => sub {
            is $did_setup,    1, "setup called";
            is $did_teardown, 1, "teardown called";
        },
        need_open3 => 15,
    },
    {   name     => 'Array',
        subclass => 'TAP::Parser::Iterator::Array',
        source   => array_ref_from($tap),
    },
    {   name     => 'Stream',
        subclass => 'TAP::Parser::Iterator::Stream',
        source   => \*DATA,
    },
    {   name     => 'Process (Perl -e)',
        subclass => 'TAP::Parser::Iterator::Process',
        source =>
          { command => [ $^X, '-e', 'print qq/one\ntwo\n\nthree\n/' ] },
    },
    {   name     => 'Process (NoFork)',
        subclass => 'TAP::Parser::Iterator::Process',
        class    => 'NoForkProcess',
        source =>
          { command => [ $^X, '-e', 'print qq/one\ntwo\n\nthree\n/' ] },
    },
);

sub _can_open3 {
    return $Config{d_fork};
}

my $factory = TAP::Parser::IteratorFactory->new;
for my $test (@schedule) {
    SKIP: {
        my $name       = $test->{name};
        my $need_open3 = $test->{need_open3};
        skip "No open3", $need_open3 if $need_open3 && !_can_open3();
        my $subclass = $test->{subclass};
        my $source   = $test->{source};
        my $class    = $test->{class};
        my $iter
          = $class
          ? $class->new($source)
          : $factory->make_iterator($source);
        ok $iter,     "$name: We should be able to create a new iterator";
        isa_ok $iter, 'TAP::Parser::Iterator',
          '... and the object it returns';
        isa_ok $iter, $subclass, '... and the object it returns';

        can_ok $iter, 'exit';
        ok !defined $iter->exit,
          "$name: ... and it should be undef before we are done ($subclass)";

        can_ok $iter, 'next';
        is $iter->next, 'one', "$name: next() should return the first result";

        is $iter->next, 'two',
          "$name: next() should return the second result";

        is $iter->next, '', "$name: next() should return the third result";

        is $iter->next, 'three',
          "$name: next() should return the fourth result";

        ok !defined $iter->next,
          "$name: next() should return undef after it is empty";

        is $iter->exit, 0,
          "$name: ... and exit should now return 0 ($subclass)";

        is $iter->wait, 0, "$name: wait should also now return 0 ($subclass)";

        if ( my $after = $test->{after} ) {
            $after->();
        }
    }
}

{

    # coverage tests for the ctor

    my $stream = $factory->make_iterator( IO::Handle->new );

    isa_ok $stream, 'TAP::Parser::Iterator::Stream';

    my @die;

    eval {
        local $SIG{__DIE__} = sub { push @die, @_ };

        $factory->make_iterator( \1 );    # a ref to a scalar
    };

    is @die, 1, 'coverage of error case';

    like pop @die, qr/Can't iterate with a SCALAR/,
      '...and we died as expected';
}

{

    # coverage test for VMS case

    my $stream = $factory->make_iterator(
        [   'not ',
            'ok 1 - I hate VMS',
        ]
    );

    is $stream->next, 'not ok 1 - I hate VMS',
      'coverage of VMS line-splitting case';

    # coverage test for VMS case - nothing after 'not'

    $stream = $factory->make_iterator(
        [   'not ',
        ]
    );

    is $stream->next, 'not ', '...and we find "not" by itself';
}

SKIP: {
    skip "No open3", 4 unless _can_open3();

    # coverage testing for TAP::Parser::Iterator::Process ctor

    my @die;

    eval {
        local $SIG{__DIE__} = sub { push @die, @_ };

        $factory->make_iterator( {} );
    };

    is @die, 1, 'coverage testing for TPI::Process';

    like pop @die, qr/Must supply a command to execute/,
      '...and we died as expected';

    my $parser = $factory->make_iterator(
        {   command => [
                $^X,
                File::Spec->catfile( 't', 'sample-tests', 'out_err_mix' )
            ],
            merge => 1,
        }
    );

    is $parser->{err}, '',    'confirm we set err to empty string';
    is $parser->{sel}, undef, '...and selector to undef';

    # And then we read from the parser to sidestep the Mac OS / open3
    # bug which frequently throws an error here otherwise.
    $parser->next;
}
__DATA__
one
two

three
