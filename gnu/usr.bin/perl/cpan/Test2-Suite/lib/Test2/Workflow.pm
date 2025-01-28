package Test2::Workflow;
use strict;
use warnings;

our $VERSION = '0.000162';

our @EXPORT_OK = qw/parse_args current_build build root_build init_root build_stack/;
use base 'Exporter';

use Test2::Workflow::Build;
use Test2::Workflow::Task::Group;
use Test2::API qw/intercept/;
use Scalar::Util qw/blessed/;

sub parse_args {
    my %input = @_;
    my $args = delete $input{args};
    my %out;
    my %props;

    my $caller = $out{frame} = $input{caller} || caller(defined $input{level} ? $input{level} : 1);
    delete @input{qw/caller level/};

    for my $arg (@$args) {
        if (my $r = ref($arg)) {
            if ($r eq 'HASH') {
                %props = (%props, %$arg);
            }
            elsif ($r eq 'CODE') {
                die "Code is already set, did you provide multiple code blocks at $caller->[1] line $caller->[2].\n"
                    if $out{code};

                $out{code} = $arg
            }
            else {
                die "Not sure what to do with $arg at $caller->[1] line $caller->[2].\n";
            }
            next;
        }

        if ($arg =~ m/^\d+$/) {
            push @{$out{lines}} => $arg;
            next;
        }

        die "Name is already set to '$out{name}', cannot set to '$arg', did you specify multiple names at $caller->[1] line $caller->[2].\n"
            if $out{name};

        $out{name} = $arg;
    }

    die "a name must be provided, and must be truthy at $caller->[1] line $caller->[2].\n"
        unless $out{name};

    die "a codeblock must be provided at $caller->[1] line $caller->[2].\n"
        unless $out{code};

    return { %props, %out, %input };
}

{
    my %ROOT_BUILDS;
    my @BUILD_STACK;

    sub root_build    { $ROOT_BUILDS{$_[0]} }
    sub current_build { @BUILD_STACK ? $BUILD_STACK[-1] : undef }
    sub build_stack   { @BUILD_STACK }

    sub init_root {
        my ($pkg, %args) = @_;
        $ROOT_BUILDS{$pkg} ||= Test2::Workflow::Build->new(
            name    => $pkg,
            flat    => 1,
            iso     => 0,
            async   => 0,
            is_root => 1,
            %args,
        );

        return $ROOT_BUILDS{$pkg};
    }

    sub build {
        my %params = @_;
        my $args = parse_args(%params);

        my $build = Test2::Workflow::Build->new(%$args);

        return $build if $args->{skip};

        push @BUILD_STACK => $build;

        my ($ok, $err);
        my $events = intercept {
            my $todo = $args->{todo} ? Test2::Todo->new(reason => $args->{todo}) : undef;
            $ok = eval { $args->{code}->(); 1 };
            $err = $@;
            $todo->end if $todo;
        };

        # Clear the stash
        $build->{stash} = [];
        $build->set_events($events);

        pop @BUILD_STACK;

        unless($ok) {
            my $hub = Test2::API::test2_stack->top;
            my $count = @$events;
            my $list = $count
                ? "Overview of unseen events:\n" . join "" => map "    " . blessed($_) . " " . $_->trace($hub)->debug . "\n", @$events
                : "";
            die <<"            EOT";
Exception in build '$args->{name}' with $count unseen event(s).
$err
$list
            EOT
        }

        return $build;
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Workflow - A test workflow is a way of structuring tests using
composable units.

=head1 DESCRIPTION

A test workflow is a way of structuring tests using composable units. A well
known example of a test workflow is L<RSPEC|http://rspec.info/>. RSPEC is
implemented using Test2::Workflow in L<Test2::Tools::Spec> along with several
extensions.

=head1 IMPORTANT CONCEPTS

=head2 BUILD

L<Test2::Workflow::Build>

A Build is used to compose tasks. Usually a build object is pushed to the stack
before running code that adds tasks to the build. Once the build sub is
complete the build is popped and returned. Usually a build is converted into a
root task or task group.

=head2 RUNNER

L<Test2::Workflow::Runner>

A runner takes the composed tasks and executes them in the proper order.

=head2 TASK

L<Test2::Workflow::Task>

A task is a unit of work to accomplish. There are 2 main types of task.

=head3 ACTION

An action is the most simple unit used in composition. An action is essentially
a name and a codeblock to run.

=head3 GROUP

A group is a task that is composed of other tasks.

=head1 EXPORTS

All exports are optional, you must request the ones you want.

=over 4

=item $parsed = parse_args(args => \@args)

=item $parsed = parse_args(args => \@args, level => $L)

=item $parsed = parse_args(args => \@args, caller => [caller($L)])

This will parse a "typical" task builders arguments. The C<@args> array MUST
contain a name (plain scalar containing text) and also a single CODE reference.
The C<@args> array MAY also contain any quantity of line numbers or hashrefs.
The resulting data structure will be a single hashref with all the provided
hashrefs squashed together, and the 'name', 'code', 'lines' and 'frame' keys
set from other arguments.

    {
        # All hashrefs from @args get squashed together:
        %squashed_input_hashref_data,

        # @args must have exactly 1 plaintext scalar that is not a number, it
        # is considered the name:
        name => 'name from input args'

        # Integer values are treated as line numbers
        lines => [ 35, 44 ],

        # Exactly 1 coderef must be provided in @args:
        code => \&some_code,

        # 'frame' contains the 'caller' data. This may be passed in directly,
        # obtained from the 'level' parameter, or automatically deduced.
        frame => ['A::Package', 'a_file.pm', 42, ...],
    }

=item $build = init_root($pkg, %args)

This will initialize (or return the existing) a build for the specified
package. C<%args> get passed into the L<Test2::Workflow::Build> constructor.
This uses the following defaults (which can be overridden using C<%args>):

    name    => $pkg,
    flat    => 1,
    iso     => 0,
    async   => 0,
    is_root => 1,

Note that C<%args> is completely ignored if the package build has already been
initialized.

=item $build = root_build($pkg)

This will return the root build for the specified package.

=item $build = current_build()

This will return the build currently at the top of the build stack (or undef).

=item $build = build($name, \%params, sub { ... })

This will push a new build object onto the build stash then run the provided
codeblock. Once the codeblock has finished running the build will be popped off
the stack and returned.

See C<parse_args()> for details about argument processing.

=back

=head1 SEE ALSO

=over 4

=item Test2::Tools::Spec

L<Test2::Tools::Spec> is an implementation of RSPEC using this library.

=back

=head1 SOURCE

The source code repository for Test2-Workflow can be found at
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

