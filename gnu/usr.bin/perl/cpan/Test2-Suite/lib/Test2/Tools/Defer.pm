package Test2::Tools::Defer;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;

use Test2::Util qw/get_tid/;
use Test2::API qw{
    test2_add_callback_exit
    test2_pid test2_tid
};

our @EXPORT = qw/def do_def/;
use base 'Exporter';

my %TODO;

sub def {
    my ($func, @args) = @_;

    my @caller = caller(0);

    $TODO{$caller[0]} ||= [];
    push @{$TODO{$caller[0]}} => [$func, \@args, \@caller];
}

sub do_def {
    my $for = caller;
    my $tests = delete $TODO{$for} or croak "No tests to run!";

    for my $test (@$tests) {
        my ($func, $args, $caller) = @$test;

        my ($pkg, $file, $line) = @$caller;

        chomp(my $eval = <<"        EOT");
package $pkg;
# line $line "(eval in Test2::Tools::Defer) $file"
\&$func(\@\$args);
1;
        EOT

        eval $eval and next;
        chomp(my $error = $@);

        require Data::Dumper;
        chomp(my $td = Data::Dumper::Dumper($args));
        $td =~ s/^\$VAR1 =/\$args: /;
        die <<"        EOT";
Exception: $error
--eval--
$eval
--------
Tool:   $func
Caller: $caller->[0], $caller->[1], $caller->[2]
$td
        EOT
    }

    return;
}

sub _verify {
    my ($context, $exit, $new_exit) = @_;

    my $not_ok = 0;
    for my $pkg (keys %TODO) {
        my $tests = delete $TODO{$pkg};
        my $caller = $tests->[0]->[-1];
        print STDOUT "not ok - deferred tests were not run!\n" unless $not_ok++;
        print STDERR "# '$pkg' has deferred tests that were never run!\n";
        print STDERR "#   $caller->[1] at line $caller->[2]\n";
        $$new_exit ||= 255;
    }
}

test2_add_callback_exit(\&_verify);

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Defer - Write tests that get executed at a later time

=head1 DESCRIPTION

Sometimes you need to test things BEFORE loading the necessary functions. This
module lets you do that. You can write tests, and then have them run later,
after C<Test2> is loaded. You tell it what test function to run, and what
arguments to give it.  The function name and arguments will be stored to be
executed later. When ready, run C<do_def()> to kick them off once the functions
are defined.

=head1 SYNOPSIS

    use strict;
    use warnings;

    use Test2::Tools::Defer;

    BEGIN {
        def ok => (1, 'pass');
        def is => ('foo', 'foo', 'runs is');
        ...
    }

    use Test2::Tools::Basic;

    do_def(); # Run the tests

    # Declare some more tests to run later:
    def ok => (1, "another pass");
    ...

    do_def(); # run the new tests

    done_testing;

=head1 EXPORTS

=over 4

=item def function => @args;

This will store the function name, and the arguments to be run later. Note that
each package has a separate store of tests to run.

=item do_def()

This will run all the stored tests. It will also reset the list to be empty so
you can add more tests to run even later.

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
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

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
