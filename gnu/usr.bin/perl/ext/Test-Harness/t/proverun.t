#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', 'lib' );
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More;
use File::Spec;
use App::Prove;

my @SCHEDULE;

BEGIN {

    # to add a new test to proverun, just list the name of the file in
    # t/sample-tests and a name for the test.  The rest is handled
    # automatically.
    my @tests = (
        {   file => 'simple',
            name => 'Create empty',
        },
        {   file => 'todo_inline',
            name => 'Passing TODO',
        },
    );
    foreach my $test (@tests) {

        # let's fully expand that filename
        $test->{file} = File::Spec->catfile(
            (   $ENV{PERL_CORE}
                ? ( File::Spec->updir(), 'ext', 'Test-Harness' )
                : ()
            ),
            't',
            'sample-tests',
            $test->{file}
        );
    }
    @SCHEDULE = (
        map {
            {   name   => $_->{name},
                args   => [ $_->{file} ],
                expect => [
                    [   'new',
                        'TAP::Parser::Iterator::Process',
                        {   merge   => undef,
                            command => [
                                'PERL',
                                $_->{file},
                            ],
                            setup    => \'CODE',
                            teardown => \'CODE',

                        }
                    ]
                ]
            }
          } @tests
    );

    plan tests => @SCHEDULE * 3;
}

# Waaaaay too much boilerplate

package FakeProve;
use vars qw( @ISA );

@ISA = qw( App::Prove );

sub new {
    my $class = shift;
    my $self  = $class->SUPER::new(@_);
    $self->{_log} = [];
    return $self;
}

sub get_log {
    my $self = shift;
    my @log  = @{ $self->{_log} };
    $self->{_log} = [];
    return @log;
}

package main;

{
    use TAP::Parser::Iterator::Process;
    use TAP::Formatter::Console;

    # Patch TAP::Parser::Iterator::Process
    my @call_log = ();

    local $^W;    # no warnings

    my $orig_new = TAP::Parser::Iterator::Process->can('new');

    # Avoid "used only once" warning
    *TAP::Parser::Iterator::Process::new
      = *TAP::Parser::Iterator::Process::new = sub {
        push @call_log, [ 'new', @_ ];

        # And then new turns round and tramples on our args...
        $_[1] = { %{ $_[1] } };
        $orig_new->(@_);
      };

    # Patch TAP::Formatter::Console;
    my $orig_output = \&TAP::Formatter::Console::_output;
    *TAP::Formatter::Console::_output = sub {

        # push @call_log, [ '_output', @_ ];
    };

    sub get_log {
        my @log = @call_log;
        @call_log = ();
        return @log;
    }
}

sub _slacken {
    my $obj = shift;
    if ( my $ref = ref $obj ) {
        if ( 'HASH' eq ref $obj ) {
            return { map { $_ => _slacken( $obj->{$_} ) } keys %$obj };
        }
        elsif ( 'ARRAY' eq ref $obj ) {
            return [ map { _slacken($_) } @$obj ];
        }
        elsif ( 'SCALAR' eq ref $obj ) {
            return $obj;
        }
        else {
            return \$ref;
        }
    }
    else {
        return $obj;
    }
}

sub is_slackly($$$) {
    my ( $got, $want, $msg ) = @_;
    return is_deeply _slacken($got), _slacken($want), $msg;
}

# ACTUAL TEST
for my $test (@SCHEDULE) {
    my $name = $test->{name};

    my $app = FakeProve->new;
    $app->process_args( '--norc', @{ $test->{args} } );

    # Why does this make the output from the test spew out of
    # our STDOUT?
    ok eval { $app->run }, 'run returned true';
    ok !$@, 'no errors' or diag $@;

    my @log = get_log();

    # Bodge: we don't know what pathname will be used for the exe so we
    # obliterate it here. Need to test that it's sane.
    for my $call (@log) {
        if ( 'HASH' eq ref $call->[2] && exists $call->[2]->{command} ) {
            $call->[2]->{command}->[0] = 'PERL';
        }
    }

    is_slackly \@log, $test->{expect}, "$name: command args OK";

    # use Data::Dumper;
    # diag Dumper(
    #     {   got    => \@log,
    #         expect => $test->{expect}
    #     }
    # );
}

