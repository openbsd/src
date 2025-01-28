package Test2::Manual::Tooling::Nesting;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Nesting - Tutorial for using other tools within your
own.

=head1 DESCRIPTION

Sometimes you find yourself writing the same test pattern over and over, in
such cases you may want to encapsulate the logic in a new test function that
calls several tools together. This sounds easy enough, but can cause headaches
if not done correctly.

=head1 NAIVE WAY

Lets say you find yourself writing the same test pattern over and over for multiple objects:

    my $obj1 = $class1->new;
    is($obj1->foo, 'foo', "got foo");
    is($obj1->bar, 'bar', "got bar");

    my $obj2 = $class1->new;
    is($obj2->foo, 'foo', "got foo");
    is($obj2->bar, 'bar', "got bar");

    ... 10x more times for classes 2-12

The naive way to do this is to write a C<check_class()> function like this:

    sub check_class {
        my $class = shift;
        my $obj = $class->new;
        is($obj->foo, 'foo', "got foo");
        is($obj->bar, 'bar', "got bar");
    }

    check_class($class1);
    check_class($class2);
    check_class($class3);
    ...

This will appear to work fine, and you might not notice any problems,
I<so long as the tests are passing.>

=head2 WHATS WRONG WITH IT?

The problems with the naive approach become obvious if things start to fail.
The diagnostics that tell you what file and line the failure occurred on will be
wrong. The failure will be reported to the line I<inside> C<check_class>, not
to the line where C<check_class()> was called. This is problem because it
leaves you with no idea which class is failing.

=head2 HOW TO FIX IT

Luckily this is extremely easy to fix. You need to acquire a context object at
the start of your function, and release it at the end... yes it is that simple.

    use Test2::API qw/context/;

    sub check_class {
        my $class = shift;

        my $ctx = context();

        my $obj = $class->new;
        is($obj->foo, 'foo', "got foo");
        is($obj->bar, 'bar', "got bar");

        $ctx->release;
    }

See, that was easy. With these 2 additional lines we know have proper file+line
reporting. The nested tools will find the context we acquired here, and know to
use it's file and line numbers.

=head3 THE OLD WAY (DO NOT DO THIS ANYMORE)

With L<Test::Builder> there was a global variables called
C<$Test::Builder::Level> which helped solve this problem:

    sub check_class {
        my $class = shift;

        local $Test::Builder::Level = $Test::Builder::Level + 1;

        my $obj = $class->new;
        is($obj->foo, 'foo', "got foo");
        is($obj->bar, 'bar', "got bar");
    }

This variable worked well enough (and will still work) but was not very
discoverable. Another problem with this variable is that it becomes cumbersome
if you have a more deeply nested code structure called the nested tools, you
might need to count stack frames, and hope they never change due to a third
party module. The context solution has no such caveats.

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
