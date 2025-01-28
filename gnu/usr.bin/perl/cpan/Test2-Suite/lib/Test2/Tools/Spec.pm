package Test2::Tools::Spec;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;
use Test2::Workflow qw/parse_args build current_build root_build init_root build_stack/;

use Test2::API qw/test2_add_callback_testing_done/;

use Test2::Workflow::Runner();
use Test2::Workflow::Task::Action();
use Test2::Workflow::Task::Group();
use Test2::Tools::Mock();
use Test2::Util::Importer();

use vars qw/@EXPORT @EXPORT_OK/;
push @EXPORT => qw{describe cases};
push @EXPORT_OK => qw{include_workflow include_workflows spec_defaults};

my %HANDLED;
sub import {
    my $class = shift;
    my @caller = caller(0);

    my %root_args;
    my %runner_args;
    my @import;
    while (my $arg = shift @_) {
        if ($arg =~ s/^-//) {
            my $val = shift @_;

            if (Test2::Workflow::Runner->can($arg)) {
                $runner_args{$arg} = $val;
            }
            elsif (Test2::Workflow::Task::Group->can($arg)) {
                $root_args{$arg} = $val;
            }
            elsif ($arg eq 'root_args') {
                %root_args = (%root_args, %$val);
            }
            elsif ($arg eq 'runner_args') {
                %runner_args = (%runner_args, %$val);
            }
            else {
                croak "Unrecognized arg: $arg";
            }
        }
        else {
            push @import => $arg;
        }
    }

    if ($HANDLED{$caller[0]}++) {
        croak "Package $caller[0] has already been initialized"
            if keys(%root_args) || keys(%runner_args);
    }
    else {
        my $root = init_root(
            $caller[0],
            frame => \@caller,
            code => sub { 1 },
            %root_args,
        );

        my $runner = Test2::Workflow::Runner->new(%runner_args);

        Test2::Tools::Mock->add_handler(
            $caller[0],
            sub {
                my %params = @_;
                my ($class, $caller, $builder, $args) = @params{qw/class caller builder args/};

                my $do_it = eval "package $caller->[0];\n#line $caller->[2] \"$caller->[1]\"\nsub { \$runner\->add_mock(\$builder->()) }";

                # Running
                if (@{$runner->stack}) {
                    $do_it->();
                }
                else { # Not running
                    my $action = Test2::Workflow::Task::Action->new(
                        code     => $do_it,
                        name     => "mock $class",
                        frame    => $caller,
                        scaffold => 1,
                    );

                    my $build = current_build() || $root;

                    $build->add_primary_setup($action);
                    $build->add_stash($builder->()) unless $build->is_root;
                }

                return 1;
            }
        );

        test2_add_callback_testing_done(
            sub {
                return unless $root->populated;
                my $g = $root->compile;
                $runner->push_task($g);
                $runner->run;
            }
        );
    }

    Test2::Util::Importer->import_into($class, $caller[0], @import);
}

{
    no warnings 'once';
    *cases             = \&describe;
    *include_workflows = \&include_workflow;
}

sub describe {
    my @caller = caller(0);

    my $want = wantarray;

    my $build = build(args => \@_, caller => \@caller, stack_stop => defined $want ? 1 : 0);

    return $build if defined $want;

    my $current = current_build() || root_build($caller[0])
        or croak "No current workflow build!";

    $current->add_primary($build);
}

sub include_workflow {
    my @caller = caller(0);

    my $build = current_build() || root_build(\$caller[0])
        or croak "No current workflow build!";

    for my $task (@_) {
        croak "include_workflow only accepts Test2::Workflow::Task objects, got: $task"
            unless $task->isa('Test2::Workflow::Task');

        $build->add_primary($task);
    }
}

sub defaults {
    my %params = @_;

    my ($package, $tool) = @params{qw/package tool/};

    my @stack = (root_build($package), build_stack());
    return unless @stack;

    my %out;
    for my $build (@stack) {
        %out = () if $build->stack_stop;
        my $new = $build->defaults->{$tool} or next;
        %out = (%out, %$new);
    }

    return \%out;
}


# Generate a bunch of subs that only have minor differences between them.
BEGIN {
    @EXPORT = qw{
        tests it
        case
        before_all  around_all  after_all
        before_case around_case after_case
        before_each around_each after_each
    };

    @EXPORT_OK = qw{
        mini
        iso   miso
        async masync
    };

    my %stages = (
        case  => ['add_variant'],
        tests => ['add_primary'],
        it    => ['add_primary'],

        iso  => ['add_primary'],
        miso => ['add_primary'],

        async  => ['add_primary'],
        masync => ['add_primary'],

        mini => ['add_primary'],

        before_all => ['add_setup'],
        after_all  => ['add_teardown'],
        around_all => ['add_setup', 'add_teardown'],

        before_case => ['add_variant_setup'],
        after_case  => ['add_variant_teardown'],
        around_case => ['add_variant_setup', 'add_variant_teardown'],

        before_each => ['add_primary_setup'],
        after_each  => ['add_primary_teardown'],
        around_each => ['add_primary_setup', 'add_primary_teardown'],
    );

    my %props = (
        case  => [],
        tests => [],
        it    => [],

        iso  => [iso => 1],
        miso => [iso => 1, flat => 1],

        async  => [async => 1],
        masync => [async => 1, flat => 1],

        mini => [flat => 1],

        before_all => [scaffold => 1],
        after_all  => [scaffold => 1],
        around_all => [scaffold => 1, around => 1],

        before_case => [scaffold => 1],
        after_case  => [scaffold => 1],
        around_case => [scaffold => 1, around => 1],

        before_each => [scaffold => 1],
        after_each  => [scaffold => 1],
        around_each => [scaffold => 1, around => 1],
    );

    sub spec_defaults {
        my ($tool, %params) = @_;
        my @caller = caller(0);

        croak "'$tool' is not a spec tool"
            unless exists $props{$tool} || exists $stages{$tool};

        my $build = current_build() || root_build($caller[0])
            or croak "No current workflow build!";

        my $old = $build->defaults->{$tool} ||= {};
        $build->defaults->{$tool} = { %$old, %params };
    }

    my $run = "";
    for my $func (@EXPORT, @EXPORT_OK) {
        $run .= <<"        EOT";
#line ${ \(__LINE__ + 1) } "${ \__FILE__ }"
sub $func {
    my \@caller = caller(0);
    my \$args = parse_args(args => \\\@_, caller => \\\@caller);
    my \$action = Test2::Workflow::Task::Action->new(\@{\$props{$func}}, %\$args);

    return \$action if defined wantarray;

    my \$build = current_build() || root_build(\$caller[0])
        or croak "No current workflow build!";

    if (my \$defaults = defaults(package => \$caller[0], tool => '$func')) {
        for my \$attr (keys \%\$defaults) {
            next if defined \$action->\$attr;
            my \$sub = "set_\$attr";
            \$action->\$sub(\$defaults->{\$attr});
        }
    }

    \$build->\$_(\$action) for \@{\$stages{$func}};
}
        EOT
    }

    my ($ok, $err);
    {
        local $@;
        $ok = eval "$run\n1";
        $err = $@;
    }

    die $@ unless $ok;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Spec - RSPEC implementation on top of Test2::Workflow

=head1 DESCRIPTION

This uses L<Test2::Workflow> to implement an RSPEC variant. This variant
supports isolation and/or concurrency via forking or threads.

=head1 SYNOPSIS

    use Test2::Bundle::Extended;
    use Test2::Tools::Spec;

    describe foo => sub {
        before_all  once => sub { ... };
        before_each many => sub { ... };

        after_all  once => sub { ... };
        after_each many => sub { ... };

        case condition_a => sub { ... };
        case condition_b => sub { ... };

        tests foo => sub { ... };
        tests bar => sub { ... };
    };

    done_testing;

=head1 EXPORTS

All of these use the same argument pattern. The first argument must always be a
name for the block. The last argument must always be a code reference.
Optionally a configuration hash can be inserted between the name and the code
reference.

    FUNCTION "name" => sub { ... };

    FUNCTION "name" => {...}, sub { ... };

=over 4

=item NAME

The first argument to a Test2::Tools::Spec function MUST be a name. The name
does not need to be unique.

=item PARAMS

This argument is optional. If present this should be a hashref.

Here are the valid keys for the hashref:

=over 8

=item flat => $bool

If this is set to true then the block will not render as a subtest, instead the
events will be inline with the parent subtest (or main test).

=item async => $bool

Set this to true to mark a block as being capable of running concurrently with
other test blocks. This does not mean the block WILL be run concurrently, just
that it can be.

=item iso => $bool

Set this to true if the block MUST be run in isolation. If this is true then
the block will run in its own forked process.

These tests will be skipped on any platform that does not have true forking, or
working/enabled threads.

Threads will ONLY be used if the T2_WORKFLOW_USE_THREADS env var is set. Thread
tests are only run if the T2_DO_THREAD_TESTS env var is set.

=item todo => $reason

Use this to mark an entire block as TODO.

=item skip => $reason

Use this to prevent a block from running at all.

=back

=item CODEREF

This argument is required. This should be a code reference that will run some
assertions.

=back

=head2 ESSENTIALS

=over 4

=item tests NAME => sub { ... }

=item tests NAME => \%params, sub { ... }

=item tests($NAME, \%PARAMS, \&CODE)

=item it NAME => sub { ... }

=item it NAME => \%params, sub { ... }

=item it($NAME, \%PARAMS, \&CODE)

This defines a test block. Test blocks are essentially subtests. All test
blocks will be run, and are expected to produce events. Test blocks can run
multiple times if the C<case()> function is also used.

C<it()> is an alias to C<tests()>.

These ARE NOT inherited by nested describe blocks.

=item case NAME => sub { ... }

=item case NAME => \%params, sub { ... }

=item case($NAME, \%PARAMS, \&CODE)

This lets you specify multiple conditions in which the test blocks should be
run. Every test block within the same group (C<describe>) will be run once per
case.

These ARE NOT inherited by nested describe blocks, but nested describe blocks
will be executed once per case.

=item before_each NAME => sub { ... }

=item before_each NAME => \%params, sub { ... }

=item before_each($NAME, \%PARAMS, \&CODE)

Specify a codeblock that should be run multiple times, once before each
C<tests()> block is run. These will run AFTER C<case()> blocks but before
C<tests()> blocks.

These ARE inherited by nested describe blocks.

=item before_case NAME => sub { ... }

=item before_case NAME => \%params, sub { ... }

=item before_case($NAME, \%PARAMS, \&CODE)

Same as C<before_each()>, except these blocks run BEFORE C<case()> blocks.

These ARE NOT inherited by nested describe blocks.

=item before_all NAME => sub { ... }

=item before_all NAME => \%params, sub { ... }

=item before_all($NAME, \%PARAMS, \&CODE)

Specify a codeblock that should be run once, before all the test blocks run.

These ARE NOT inherited by nested describe blocks.

=item around_each NAME => sub { ... }

=item around_each NAME => \%params, sub { ... }

=item around_each($NAME, \%PARAMS, \&CODE)

Specify a codeblock that should wrap around each test block. These blocks are
run AFTER case blocks, but before test blocks.

    around_each wrapit => sub {
        my $cont = shift;

        local %ENV = ( ... );

        $cont->();

        ...
    };

The first argument to the codeblock will be a callback that MUST be called
somewhere inside the sub in order for nested items to run.

These ARE inherited by nested describe blocks.

=item around_case NAME => sub { ... }

=item around_case NAME => \%params, sub { ... }

=item around_case($NAME, \%PARAMS, \&CODE)

Same as C<around_each> except these run BEFORE case blocks.

These ARE NOT inherited by nested describe blocks.

=item around_all NAME => sub { ... }

=item around_all NAME => \%params, sub { ... }

=item around_all($NAME, \%PARAMS, \&CODE)

Same as C<around_each> except that it only runs once to wrap ALL test blocks.

These ARE NOT inherited by nested describe blocks.

=item after_each NAME => sub { ... }

=item after_each NAME => \%params, sub { ... }

=item after_each($NAME, \%PARAMS, \&CODE)

Same as C<before_each> except it runs right after each test block.

These ARE inherited by nested describe blocks.

=item after_case NAME => sub { ... }

=item after_case NAME => \%params, sub { ... }

=item after_case($NAME, \%PARAMS, \&CODE)

Same as C<after_each> except it runs right after the case block, and before the
test block.

These ARE NOT inherited by nested describe blocks.

=item after_all NAME => sub { ... }

=item after_all NAME => \%params, sub { ... }

=item after_all($NAME, \%PARAMS, \&CODE)

Same as C<before_all> except it runs after all test blocks have been run.

These ARE NOT inherited by nested describe blocks.

=back

=head2 SHORTCUTS

These are shortcuts. Each of these is the same as C<tests()> except some
parameters are added for you.

These are NOT exported by default/.

=over 4

=item mini NAME => sub { ... }

Same as:

    tests NAME => { flat => 1 }, sub { ... }

=item iso NAME => sub { ... }

Same as:

    tests NAME => { iso => 1 }, sub { ... }

=item miso NAME => sub { ... }

Same as:

    tests NAME => { mini => 1, iso => 1 }, sub { ... }

=item async NAME => sub { ... }

Same as:

    tests NAME => { async => 1 }, sub { ... }

B<Note:> This conflicts with the C<async()> exported from L<threads>. Don't
import both.

=item masync NAME => sub { ... }

Same as:

    tests NAME => { minit => 1, async => 1 }, sub { ... }

=back

=head2 CUSTOM ATTRIBUTE DEFAULTS

Sometimes you want to apply default attributes to all C<tests()> or C<case()>
blocks. This can be done, and is lexical to your describe or package root!

    use Test2::Bundle::Extended;
    use Test2::Tools::Spec ':ALL';

    # All 'tests' blocks after this declaration will have C<<iso => 1>> by default
    spec_defaults tests => (iso => 1);

    tests foo => sub { ... }; # isolated

    tests foo, {iso => 0}, sub { ... }; # Not isolated

    spec_defaults tests => (iso => 0); # Turn it off again

Defaults are inherited by nested describe blocks. You can also override the
defaults for the scope of the describe:

    spec_defaults tests => (iso => 1);

    describe foo => sub {
        spec_defaults tests => (async => 1); # Scoped to this describe and any child describes

        tests bar => sub { ... }; # both iso and async
    };

    tests baz => sub { ... }; # Just iso, no async.

You can apply defaults to any type of blocks:

    spec_defaults case => (iso => 1); # All cases are 'iso';

Defaults are not inherited when a builder's return is captured.

    spec_defaults tests => (iso => 1);

    # Note we are not calling this in void context, that is the key here.
    my $d = describe foo => {
        tests bar => sub { ... }; # Not iso
    };

=head1 EXECUTION ORDER

As each function is encountered it executes, just like any other function. The
C<describe()> function will immediately execute the codeblock it is given. All
other functions will stash their codeblocks to be run later. When
C<done_testing()> is run the workflow will be compiled, at which point all
other blocks will run.

Here is an overview of the order in which blocks get called once compiled (at
C<done_testing()>).

    before_all
        for-each-case {
            before_case
                case
            after_case

            # AND/OR nested describes
            before_each
                tests
            after_each
        }
    after_all

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

