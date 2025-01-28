package Test2::Tools::ClassicCompare;
use strict;
use warnings;

our $VERSION = '0.000162';

our @EXPORT = qw/is is_deeply isnt like unlike cmp_ok/;
use base 'Exporter';

use Carp qw/carp/;
use Scalar::Util qw/reftype/;

use Test2::API qw/context/;
use Test2::Compare qw/compare strict_convert/;
use Test2::Util::Ref qw/rtype render_ref/;
use Test2::Util::Table qw/table/;

use Test2::Compare::Array();
use Test2::Compare::Bag();
use Test2::Compare::Custom();
use Test2::Compare::Event();
use Test2::Compare::Hash();
use Test2::Compare::Meta();
use Test2::Compare::Number();
use Test2::Compare::Object();
use Test2::Compare::OrderedSubset();
use Test2::Compare::Pattern();
use Test2::Compare::Ref();
use Test2::Compare::Regex();
use Test2::Compare::Scalar();
use Test2::Compare::Set();
use Test2::Compare::String();
use Test2::Compare::Undef();
use Test2::Compare::Wildcard();

sub is($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my @caller = caller;

    my $delta = compare($got, $exp, \&is_convert);

    if ($delta) {
        $ctx->fail($name, $delta->diag, @diag);
    }
    else {
        $ctx->ok(1, $name);
    }

    $ctx->release;
    return !$delta;
}

sub isnt($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my @caller = caller;

    my $delta = compare($got, $exp, \&isnt_convert);

    if ($delta) {
        $ctx->fail($name, $delta->diag, @diag);
    }
    else {
        $ctx->ok(1, $name);
    }

    $ctx->release;
    return !$delta;
}

sub is_convert {
    my ($thing) = @_;
    return Test2::Compare::Undef->new()
        unless defined $thing;
    return Test2::Compare::String->new(input => $thing);
}

sub isnt_convert {
    my ($thing) = @_;
    return Test2::Compare::Undef->new()
        unless defined $thing;
    my $str = Test2::Compare::String->new(input => $thing, negate => 1);
}

sub like($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my $delta = compare($got, $exp, \&like_convert);

    if ($delta) {
        $ctx->fail($name, $delta->diag, @diag);
    }
    else {
        $ctx->ok(1, $name);
    }

    $ctx->release;
    return !$delta;
}

sub unlike($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my $delta = compare($got, $exp, \&unlike_convert);

    if ($delta) {
        $ctx->fail($name, $delta->diag, @diag);
    }
    else {
        $ctx->ok(1, $name);
    }

    $ctx->release;
    return !$delta;
}

sub like_convert {
    my ($thing) = @_;
    return Test2::Compare::Pattern->new(
        pattern       => $thing,
        stringify_got => 1,
    );
}

sub unlike_convert {
    my ($thing) = @_;
    return Test2::Compare::Pattern->new(
        negate        => 1,
        stringify_got => 1,
        pattern       => $thing,
    );
}

sub is_deeply($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my @caller = caller;

    my $delta = compare($got, $exp, \&strict_convert);

    if ($delta) {
        # Temporary thing.
        my $count = 0;
        my $implicit = 0;
        my @deltas = ($delta);
        while (my $d = shift @deltas) {
            my $add = $d->children;
            push @deltas => @$add if $add && @$add;
            next if $d->verified;
            $count++;
            $implicit++ if $d->note && $d->note eq 'implicit end';
        }

        if ($implicit == $count) {
            $ctx->ok(1, $name);
            my $meth = $ENV{AUTHOR_TESTING} ? 'throw' : 'alert';
            my $type = $delta->render_check;
            $ctx->$meth(
                join "\n",
                "!!! NOTICE OF BEHAVIOR CHANGE !!!",
                "This test uses at least 1 $type check without using end() or etc().",
                "The existing behavior is to default to etc() when inside is_deeply().",
                "The new behavior is to default to end().",
                "This test will soon start to fail with the following diagnostics:",
                $delta->diag->as_string,
                "",
            );
        }
        else {
            $ctx->fail($name, $delta->diag, @diag);
        }
    }
    else {
        $ctx->ok(1, $name);
    }

    $ctx->release;
    return !$delta;
}

our %OPS = (
    '=='  => 'num',
    '!='  => 'num',
    '>='  => 'num',
    '<='  => 'num',
    '>'   => 'num',
    '<'   => 'num',
    '<=>' => 'num',

    'eq'  => 'str',
    'ne'  => 'str',
    'gt'  => 'str',
    'lt'  => 'str',
    'ge'  => 'str',
    'le'  => 'str',
    'cmp' => 'str',
    '!~'  => 'str',
    '=~'  => 'str',

    '&&'  => 'logic',
    '||'  => 'logic',
    'xor' => 'logic',
    'or'  => 'logic',
    'and' => 'logic',
    '//'  => 'logic',

    '&' => 'bitwise',
    '|' => 'bitwise',

    '~~' => 'match',
);
sub cmp_ok($$$;$@) {
    my ($got, $op, $exp, $name, @diag) = @_;

    my $ctx = context();

    # Warnings and syntax errors should report to the cmp_ok call, not the test
    # context. They may not be the same.
    my ($pkg, $file, $line) = caller;

    my $type = $OPS{$op};
    if (!$type) {
        carp "operator '$op' is not supported (you can add it to %Test2::Tools::ClassicCompare::OPS)";
        $type = 'unsupported';
    }

    local ($@, $!, $SIG{__DIE__});

    my $test;
    my $lived = eval <<"    EOT";
#line $line "(eval in cmp_ok) $file"
\$test = (\$got $op \$exp);
1;
    EOT
    my $error = $@;
    $ctx->send_event('Exception', error => $error) unless $lived;

    if ($test && $lived) {
        $ctx->ok(1, $name);
        $ctx->release;
        return 1;
    }

    # Ugh, it failed. Do roughly the same thing Test::More did to try and show
    # diagnostics, but make it better by showing both the overloaded and
    # unoverloaded form if overloading is in play. Also unoverload numbers,
    # Test::More only unoverloaded strings.

    my ($display_got, $display_exp);
    if($type eq 'str') {
        $display_got = defined($got) ? "$got" : undef;
        $display_exp = defined($exp) ? "$exp" : undef;
    }
    elsif($type eq 'num') {
        $display_got = defined($got) ? $got + 0 : undef;
        $display_exp = defined($exp) ? $exp + 0 : undef;
    }
    else { # Well, we did what we could.
        $display_got = $got;
        $display_exp = $exp;
    }

    my $got_ref = ref($got) ? render_ref($got) : $got;
    my $exp_ref = ref($exp) ? render_ref($exp) : $exp;

    my @table;
    my $show_both = (
        (defined($got) && $got_ref ne "$display_got")
        ||
        (defined($exp) && $exp_ref ne "$display_exp")
    );

    if ($show_both) {
        @table = table(
            header => ['TYPE', 'GOT', 'OP', 'CHECK'],
            rows   => [
                [$type, $display_got, $op, $lived ? $display_exp : '<EXCEPTION>'],
                ['orig', $got_ref, '', $exp_ref],
            ],
        );
    }
    else {
        @table = table(
            header => ['GOT', 'OP', 'CHECK'],
            rows   => [[$display_got, $op, $lived ? $display_exp : '<EXCEPTION>']],
        );
    }

    $ctx->ok(0, $name, [join("\n", @table), @diag]);
    $ctx->release;
    return 0;
}


1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::ClassicCompare - Classic (Test::More style) comparison tools.

=head1 DESCRIPTION

This provides comparison functions that behave like they did in L<Test::More>,
unlike the L<Test2::Tools::Compare> plugin which has modified them.

=head1 SYNOPSIS

    use Test2::Tools::ClassicCompare qw/is is_deeply isnt like unlike cmp_ok/;

    is($got, $expect, "These are the same when stringified");
    isnt($got, $unexpected, "These are not the same when stringified");

    like($got, qr/.../, "'got' matches the pattern");
    unlike($got, qr/.../, "'got' does not match the pattern");

    is_deeply($got, $expect, "These structures are same when checked deeply");

    cmp_ok($GOT, $OP, $WANT, 'Compare these items using the specified operatr');

=head1 EXPORTS

=over 4

=item $bool = is($got, $expect)

=item $bool = is($got, $expect, $name)

=item $bool = is($got, $expect, $name, @diag)

This does a string comparison of the two arguments. If the two arguments are the
same after stringification the test passes. The test will also pass if both
arguments are undef.

The test C<$name> is optional.

The test C<@diag> is optional, it is extra diagnostics messages that will be
displayed if the test fails. The diagnostics are ignored if the test passes.

It is important to note that this tool considers C<"1"> and C<"1.0"> to not be
equal as it uses a string comparison.

See L<Test2::Tools::Compare> if you want an C<is()> function that tries
to be smarter for you.

=item $bool = isnt($got, $dont_expect)

=item $bool = isnt($got, $dont_expect, $name)

=item $bool = isnt($got, $dont_expect, $name, @diag)

This is the inverse of C<is()>, it passes when the strings are not the same.

=item $bool = like($got, $pattern)

=item $bool = like($got, $pattern, $name)

=item $bool = like($got, $pattern, $name, @diag)

Check if C<$got> matches the specified pattern. Will fail if it does not match.

The test C<$name> is optional.

The test C<@diag> is optional. It contains extra diagnostics messages that will
be displayed if the test fails. The diagnostics are ignored if the test passes.

=item $bool = unlike($got, $pattern)

=item $bool = unlike($got, $pattern, $name)

=item $bool = unlike($got, $pattern, $name, @diag)

This is the inverse of C<like()>. This will fail if C<$got> matches
C<$pattern>.

=item $bool = is_deeply($got, $expect)

=item $bool = is_deeply($got, $expect, $name)

=item $bool = is_deeply($got, $expect, $name, @diag)

This does a deep check, comparing the structures in C<$got> with those in
C<$expect>. It will recurse into hashrefs, arrayrefs, and scalar refs. All
other values will be stringified and compared as strings. It is important to
note that this tool considers C<"1"> and C<"1.0"> to not be equal as it uses a
string comparison.

This is the same as C<Test2::Tools::Compare::is()>.

=item cmp_ok($got, $op, $expect)

=item cmp_ok($got, $op, $expect, $name)

=item cmp_ok($got, $op, $expect, $name, @diag)

Compare C<$got> to C<$expect> using the operator specified in C<$op>. This is
effectively an C<eval "\$got $op \$expect"> with some other stuff to make it
more sane. This is useful for comparing numbers, overloaded objects, etc.

B<Overloading Note:> Your input is passed as-is to the comparison.
If the comparison fails between two overloaded objects, the diagnostics will
try to show you the overload form that was used in comparisons. It is possible
that the diagnostics will be wrong, though attempts have been made to improve
them since L<Test::More>.

B<Exceptions:> If the comparison results in an exception then the test will
fail and the exception will be shown.

C<cmp_ok()> has an internal list of operators it supports. If you provide an
unsupported operator it will issue a warning. You can add operators to the
C<%Test2::Tools::ClassicCompare::OPS> hash, the key should be the operator, and
the value should either be 'str' for string comparison operators, 'num' for
numeric operators, or any other true value for other operators.

Supported operators:

=over 4

=item ==  (num)

=item !=  (num)

=item >=  (num)

=item <=  (num)

=item >   (num)

=item <   (num)

=item <=> (num)

=item eq  (str)

=item ne  (str)

=item gt  (str)

=item lt  (str)

=item ge  (str)

=item le  (str)

=item cmp (str)

=item !~  (str)

=item =~  (str)

=item &&

=item ||

=item xor

=item or

=item and

=item //

=item &

=item |

=item ~~

=back

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
