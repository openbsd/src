package Test2::Tools::AsyncSubtest;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::IPC;
use Test2::AsyncSubtest;
use Test2::API qw/context/;
use Carp qw/croak/;

our @EXPORT = qw/async_subtest fork_subtest thread_subtest/;
use base 'Exporter';

sub async_subtest {
    my $name = shift;
    my ($params, $code);
    $params = shift(@_) if @_ && ref($_[0]) eq 'HASH';
    $code   = shift(@_) if @_ && ref($_[0]) eq 'CODE';

    my $ctx = context();

    my $subtest = Test2::AsyncSubtest->new(name => $name, context => 1, hub_init_args => $params);

    $subtest->run($code, $subtest) if $code;

    $ctx->release;
    return $subtest;
}

sub fork_subtest {
    my $name = shift;
    my ($params, $code);
    $params = shift(@_) if @_ && ref($_[0]) eq 'HASH';
    $code   = shift(@_) if @_ && ref($_[0]) eq 'CODE';

    my $ctx = context();

    croak "fork_subtest requires a CODE reference as the second argument"
        unless ref($code) eq 'CODE';

    my $subtest = Test2::AsyncSubtest->new(name => $name, context => 1, hub_init_args => $params);

    $subtest->run_fork($code, $subtest);

    $ctx->release;
    return $subtest;
}

sub thread_subtest {
    my $name = shift;
    my ($params, $code);
    $params = shift(@_) if @_ && ref($_[0]) eq 'HASH';
    $code   = shift(@_) if @_ && ref($_[0]) eq 'CODE';

    my $ctx = context();

    croak "thread_subtest requires a CODE reference as the second argument"
        unless ref($code) eq 'CODE';

    my $subtest = Test2::AsyncSubtest->new(name => $name, context => 1, hub_init_args => $params);

    $subtest->run_thread($code, $subtest);

    $ctx->release;
    return $subtest;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::AsyncSubtest - Tools for writing async subtests.

=head1 DESCRIPTION

These are tools for writing async subtests. Async subtests are subtests which
can be started and stashed so that they can continue to receive events while
other events are also being generated.

=head1 SYNOPSIS

    use Test2::Bundle::Extended;
    use Test2::Tools::AsyncSubtest;

    my $ast1 = async_subtest local => sub {
        ok(1, "Inside subtest");
    };

    my $ast2 = fork_subtest child => sub {
        ok(1, "Inside subtest in another process");
    };

    # You must call finish on the subtests you create. Finish will wait/join on
    # any child processes and threads.
    $ast1->finish;
    $ast2->finish;

    done_testing;

=head1 EXPORTS

Everything is exported by default.

=over 4

=item $ast = async_subtest $name

=item $ast = async_subtest $name => sub { ... }

=item $ast = async_subtest $name => \%hub_params, sub { ... }

Create an async subtest. Run the codeblock if it is provided.

=item $ast = fork_subtest $name => sub { ... }

=item $ast = fork_subtest $name => \%hub_params, sub { ... }

Create an async subtest. Run the codeblock in a forked process.

=item $ast = thread_subtest $name => sub { ... }

=item $ast = thread_subtest $name => \%hub_params, sub { ... }

B<** DISCOURAGED **> Threads are fragile. Thread tests are not even run unless
the AUTHOR_TESTING or T2_DO_THREAD_TESTS env vars are enabled.

Create an async subtest. Run the codeblock in a thread.

=back

=head1 NOTES

=over 4

=item Async Subtests are always buffered.

Always buffered.

=item Do not use done_testing() yourself.

using done_testing() inside an async subtest will not work properly, the async
subtest must be finalized by calling C<< $st->finish >>.

=back

=head1 SOURCE

The source code repository for Test2-AsyncSubtest can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
