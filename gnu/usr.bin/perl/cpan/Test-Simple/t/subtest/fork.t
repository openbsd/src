#!/usr/bin/perl -w
use strict;
use warnings;
use Config;
use IO::Pipe;
use Test::Builder;
use Test::More;

my $Can_Fork = $Config{d_fork} ||
               (($^O eq 'MSWin32' || $^O eq 'NetWare') and
                $Config{useithreads} and
                $Config{ccflags} =~ /-DPERL_IMPLICIT_SYS/
               );

if( !$Can_Fork ) {
    plan 'skip_all' => "This system cannot fork";
}
else {
    plan 'tests' => 1;
}

subtest 'fork within subtest' => sub {
    plan tests => 2;

    my $pipe = IO::Pipe->new;
    my $pid = fork;
    defined $pid or plan skip_all => "Fork not working";

    if ($pid) {
        $pipe->reader;
        my $child_output = do { local $/ ; <$pipe> };
        waitpid $pid, 0;

        is $?, 0, 'child exit status';
        like $child_output, qr/^[\s#]+Child Done\s*\z/, 'child output';
    } 
    else {
        $pipe->writer;

        # Force all T::B output into the pipe, for the parent
        # builder as well as the current subtest builder.
        no warnings 'redefine';
        *Test::Builder::output         = sub { $pipe };
        *Test::Builder::failure_output = sub { $pipe };
        *Test::Builder::todo_output    = sub { $pipe };
        
        diag 'Child Done';
        exit 0;
    }
};

