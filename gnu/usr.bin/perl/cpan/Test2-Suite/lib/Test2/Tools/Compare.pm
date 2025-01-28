package Test2::Tools::Compare;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;
use Scalar::Util qw/reftype/;

use Test2::API qw/context/;
use Test2::Util::Ref qw/rtype/;
use Test2::Util qw/pkg_to_file/;

use Test2::Compare qw{
    compare
    get_build push_build pop_build build
    strict_convert relaxed_convert
};

use Test2::Compare::Array();
use Test2::Compare::Bag();
use Test2::Compare::Bool();
use Test2::Compare::Custom();
use Test2::Compare::Event();
use Test2::Compare::Float();
use Test2::Compare::Hash();
use Test2::Compare::Isa();
use Test2::Compare::Meta();
use Test2::Compare::Number();
use Test2::Compare::Object();
use Test2::Compare::OrderedSubset();
use Test2::Compare::Pattern();
use Test2::Compare::Ref();
use Test2::Compare::DeepRef();
use Test2::Compare::Regex();
use Test2::Compare::Scalar();
use Test2::Compare::Set();
use Test2::Compare::String();
use Test2::Compare::Undef();
use Test2::Compare::Wildcard();

%Carp::Internal = (
    %Carp::Internal,
    'Test2::Tools::Compare'         => 1,
    'Test2::Compare::Array'         => 1,
    'Test2::Compare::Bag'           => 1,
    'Test2::Compare::Bool'          => 1,
    'Test2::Compare::Custom'        => 1,
    'Test2::Compare::Event'         => 1,
    'Test2::Compare::Float'         => 1,
    'Test2::Compare::Hash'          => 1,
    'Test2::Compare::Isa'           => 1,
    'Test2::Compare::Meta'          => 1,
    'Test2::Compare::Number'        => 1,
    'Test2::Compare::Object'        => 1,
    'Test2::Compare::Pattern'       => 1,
    'Test2::Compare::Ref'           => 1,
    'Test2::Compare::Regex'         => 1,
    'Test2::Compare::Scalar'        => 1,
    'Test2::Compare::Set'           => 1,
    'Test2::Compare::String'        => 1,
    'Test2::Compare::Undef'         => 1,
    'Test2::Compare::Wildcard'      => 1,
    'Test2::Compare::OrderedSubset' => 1,
);

our @EXPORT = qw/is like/;
our @EXPORT_OK = qw{
    is like isnt unlike
    match mismatch validator
    hash array bag object meta meta_check number float rounded within string subset bool check_isa
    number_lt number_le number_ge number_gt
    in_set not_in_set check_set
    item field call call_list call_hash prop check all_items all_keys all_vals all_values
    etc end filter_items
    T F D DF E DNE FDNE U L
    event fail_events
    exact_ref
};
use base 'Exporter';

my $_autodump = sub {
    my ($ctx, $got) = @_;

    my $module = $ENV{'T2_AUTO_DUMP'} or return;
    $module = 'Data::Dumper' if $module eq '1';

    my $file = pkg_to_file($module);
    eval { require $file };

    if (not $module->can('Dump')) {
        require Data::Dumper;
        $module = 'Data::Dumper';
    }

    my $deparse = $Data::Dumper::Deparse;
    $deparse = !!$ENV{'T2_AUTO_DEPARSE'} if exists $ENV{'T2_AUTO_DEPARSE'};
    local $Data::Dumper::Deparse = $deparse;

    $ctx->diag($module->Dump([$got], ['GOT']));
};

sub is($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

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
                "The old behavior was to default to etc() when inside is().",
                "The old behavior was a bug.",
                "The new behavior is to default to end().",
                "This test will soon start to fail with the following diagnostics:",
                $delta->diag->as_string,
                "",
            );
        }
        else {
            $ctx->fail($name, $delta->diag, @diag);
            $ctx->$_autodump($got);
        }
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

    my $delta = compare($got, $exp, \&strict_convert);

    if ($delta) {
        $ctx->ok(1, $name);
    }
    else {
        $ctx->ok(0, $name, ["Comparison matched (it should not).", @diag]);
        $ctx->$_autodump($got);
    }

    $ctx->release;
    return $delta ? 1 : 0;
}

sub like($$;$@) {
    my ($got, $exp, $name, @diag) = @_;
    my $ctx = context();

    my $delta = compare($got, $exp, \&relaxed_convert);

    if ($delta) {
        $ctx->fail($name, $delta->diag, @diag);
        $ctx->$_autodump($got);
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

    my $delta = compare($got, $exp, \&relaxed_convert);

    if ($delta) {
        $ctx->ok(1, $name);
    }
    else {
        $ctx->ok(0, $name, ["Comparison matched (it should not).", @diag]);
        $ctx->$_autodump($got);
    }

    $ctx->release;
    return $delta ? 1 : 0;
}

sub meta(&)       { build('Test2::Compare::Meta',          @_) }
sub meta_check(&) { build('Test2::Compare::Meta',          @_) }
sub hash(&)       { build('Test2::Compare::Hash',          @_) }
sub array(&)      { build('Test2::Compare::Array',         @_) }
sub bag(&)        { build('Test2::Compare::Bag',           @_) }
sub object(&)     { build('Test2::Compare::Object',        @_) }
sub subset(&)     { build('Test2::Compare::OrderedSubset', @_) }

sub U() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { defined $_ ? 0 : 1 }, name => 'UNDEFINED', operator => '!DEFINED()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub D() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { defined $_ ? 1 : 0 }, name => 'DEFINED', operator => 'DEFINED()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub DF() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { defined $_ && ( ! ref $_ && ! $_ ) ? 1 : 0 }, name => 'DEFINED BUT FALSE', operator => 'DEFINED() && FALSE()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub DNE() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { my %p = @_; $p{exists} ? 0 : 1 }, name => '<DOES NOT EXIST>', operator => '!exists',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub E() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { my %p = @_; $p{exists} ? 1 : 0 }, name => '<DOES EXIST>', operator => '!exists',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub F() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { my %p = @_; $p{got} ? 0 : $p{exists} }, name => 'FALSE', operator => 'FALSE()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub FDNE() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub {
            my %p = @_;
            return 1 unless $p{exists};
            return $p{got} ? 0 : 1;
        },
        name => 'FALSE', operator => 'FALSE() || !exists',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub T() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub {
            my %p = @_;
            return 0 unless $p{exists};
            return $p{got} ? 1 : 0;
        },
        name => 'TRUE', operator => 'TRUE()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub L() {
    my @caller = caller;
    Test2::Compare::Custom->new(
        code => sub { defined $_ && length $_ ? 1 : 0 }, name => 'LENGTH', operator => 'DEFINED() && LENGTH()',
        file => $caller[1],
        lines => [$caller[2]],
    );
}

sub exact_ref($) {
    my @caller = caller;
    return Test2::Compare::Ref->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $_[0],
    );
}

sub match($) {
    my @caller = caller;
    return Test2::Compare::Pattern->new(
        file    => $caller[1],
        lines   => [$caller[2]],
        pattern => $_[0],
    );
}

sub mismatch($) {
    my @caller = caller;
    return Test2::Compare::Pattern->new(
        file    => $caller[1],
        lines   => [$caller[2]],
        negate  => 1,
        pattern => $_[0],
    );
}

sub validator {
    my $code = pop;
    my $cname = pop;
    my $op = pop;

    my @caller = caller;
    return Test2::Compare::Custom->new(
        file     => $caller[1],
        lines    => [$caller[2]],
        code     => $code,
        name     => $cname,
        operator => $op,
    );
}

sub number($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Number->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        @args,
    );
}

sub number_lt($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Number->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        mode  => '<',
        @args,
    );
}

sub number_le($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Number->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        mode  => '<=',
        @args,
    );
}

sub number_ge($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Number->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        mode  => '>=',
        @args,
    );
}

sub number_gt($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Number->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        mode  => '>',
        @args,
    );
}

sub float($;@) {
    my ($num, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Float->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $num,
        @args,
    );
}

sub rounded($$) {
    my ($num, $precision) = @_;
    my @caller = caller;
    return Test2::Compare::Float->new(
        file      => $caller[1],
        lines     => [$caller[2]],
        input     => $num,
        precision => $precision,
    );
}

sub within($;$) {
    my ($num, $tolerance) = @_;
    my @caller = caller;
    return Test2::Compare::Float->new(
        file      => $caller[1],
        lines     => [$caller[2]],
        input     => $num,
        defined $tolerance ? ( tolerance => $tolerance ) : (),
    );
}

sub bool($;@) {
    my ($bool, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Bool->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $bool,
        @args,
    );
}

sub string($;@) {
    my ($str, @args) = @_;
    my @caller = caller;
    return Test2::Compare::String->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $str,
        @args,
    );
}

sub check_isa($;@) {
    my ($class_name, @args) = @_;
    my @caller = caller;
    return Test2::Compare::Isa->new(
        file  => $caller[1],
        lines => [$caller[2]],
        input => $class_name,
        @args,
    );
}

sub filter_items(&) {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support filters"
        unless $build->can('add_filter');

    croak "'filter_items' should only ever be called in void context"
        if defined wantarray;

    $build->add_filter(@_);
}

sub all_items {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support all-items"
        unless $build->can('add_for_each');

    croak "'all_items' should only ever be called in void context"
        if defined wantarray;

    $build->add_for_each(@_);
}

sub all_keys {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support all-keys"
        unless $build->can('add_for_each_key');

    croak "'all_keys' should only ever be called in void context"
        if defined wantarray;

    $build->add_for_each_key(@_);
}

*all_vals = *all_values;
sub all_values {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support all-values"
        unless $build->can('add_for_each_val');

    croak "'all_values' should only ever be called in void context"
        if defined wantarray;

    $build->add_for_each_val(@_);
}


sub end() {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support 'ending'"
        unless $build->can('ending');

    croak "'end' should only ever be called in void context"
        if defined wantarray;

    $build->set_ending(1);
}

sub etc() {
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support 'ending'"
        unless $build->can('ending');

    croak "'etc' should only ever be called in void context"
        if defined wantarray;

    $build->set_ending(0);
}

my $_call = sub {
    my ($name, $expect, $context, $func_name) = @_;
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support method calls"
        unless $build->can('add_call');

    croak "'$func_name' should only ever be called in void context"
        if defined wantarray;

    my @caller = caller;
    $build->add_call(
        $name,
        Test2::Compare::Wildcard->new(
            expect => $expect,
            file   => $caller[1],
            lines  => [$caller[2]],
        ),
        undef,
        $context,
    );
};

sub call($$)      { $_call->(@_,'scalar','call') }
sub call_list($$) { $_call->(@_,'list','call_list') }
sub call_hash($$) { $_call->(@_,'hash','call_hash') }

sub prop($$) {
    my ($name, $expect) = @_;
    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support meta-checks"
        unless $build->can('add_prop');

    croak "'prop' should only ever be called in void context"
        if defined wantarray;

    my @caller = caller;
    $build->add_prop(
        $name,
        Test2::Compare::Wildcard->new(
            expect => $expect,
            file   => $caller[1],
            lines  => [$caller[2]],
        ),
    );
}

sub item($;$) {
    my @args   = @_;
    my $expect = pop @args;

    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support array item checks"
        unless $build->can('add_item');

    croak "'item' should only ever be called in void context"
        if defined wantarray;

    my @caller = caller;
    push @args => Test2::Compare::Wildcard->new(
        expect => $expect,
        file   => $caller[1],
        lines  => [$caller[2]],
    );

    $build->add_item(@args);
}

sub field($$) {
    my ($name, $expect) = @_;

    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' does not support hash field checks"
        unless $build->can('add_field');

    croak "'field' should only ever be called in void context"
        if defined wantarray;

    my @caller = caller;
    $build->add_field(
        $name,
        Test2::Compare::Wildcard->new(
            expect => $expect,
            file   => $caller[1],
            lines  => [$caller[2]],
        ),
    );
}

sub check($) {
    my ($check) = @_;

    defined( my $build = get_build() ) or croak "No current build!";

    croak "'$build' is not a check-set"
        unless $build->can('add_check');

    croak "'check' should only ever be called in void context"
        if defined wantarray;

    my @caller = caller;
    my $wc = Test2::Compare::Wildcard->new(
        expect => $check,
        file   => $caller[1],
        lines  => [$caller[2]],
    );

    $build->add_check($wc);
}

sub check_set  { return _build_set('all'  => @_) }
sub in_set     { return _build_set('any'  => @_) }
sub not_in_set { return _build_set('none' => @_) }

sub _build_set {
    my $redux = shift;
    my ($builder) = @_;
    my $btype = reftype($builder) || '';

    my $set;
    if ($btype eq 'CODE') {
        $set = build('Test2::Compare::Set', $builder);
        $set->set_builder($builder);
    }
    else {
        $set = Test2::Compare::Set->new(checks => [@_]);
    }

    $set->set_reduction($redux);
    return $set;
}

sub fail_events($;$) {
    my $event = &event(@_);

    my $diag = event('Diag');

    return ($event, $diag) if defined wantarray;

    defined( my $build = get_build() ) or croak "No current build!";
    $build->add_item($event);
    $build->add_item($diag);
}

sub event($;$) {
    my ($intype, $spec) = @_;

    my @caller = caller;

    croak "type is required" unless $intype;

    my $type;
    if ($intype =~ m/^\+(.*)$/) {
        $type = $1;
    }
    else {
        $type = "Test2::Event::$intype";
    }

    my $event;
    if (!$spec) {
        $event = Test2::Compare::Event->new(
            etype  => $intype,
            file   => $caller[1],
            lines  => [$caller[2]],
            ending => 0,
        );
    }
    elsif (!ref $spec) {
        croak "'$spec' is not a valid event specification";
    }
    elsif (reftype($spec) eq 'CODE') {
        $event = build('Test2::Compare::Event', $spec);
        $event->set_etype($intype);
        $event->set_builder($spec);
        $event->set_ending(0) unless defined $event->ending;
    }
    else {
        my $refcheck = Test2::Compare::Hash->new(
            inref => $spec,
            file  => $caller[1],
            lines => [$caller[2]],
        );
        $event = Test2::Compare::Event->new(
            refcheck => $refcheck,
            file     => $caller[1],
            lines    => [$caller[2]],
            etype    => $intype,
            ending   => 0,
        );
    }

    $event->add_prop('blessed' => $type);

    return $event if defined wantarray;

    defined( my $build = get_build() ) or croak "No current build!";
    $build->add_item($event);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Compare - Tools for comparing deep data structures.

=head1 DESCRIPTION

L<Test::More> had C<is_deeply()>. This library is the L<Test2> version that can
be used to compare data structures, but goes a step further in that it provides
tools for building a data structure specification against which you can verify
your data. There are both 'strict' and 'relaxed' versions of the tools.

=head1 SYNOPSIS

    use Test2::Tools::Compare;

    # Hash for demonstration purposes
    my $some_hash = {a => 1, b => 2, c => 3};

    # Strict checking, everything must match
    is(
        $some_hash,
        {a => 1, b => 2, c => 3},
        "The hash we got matches our expectations"
    );

    # Relaxed Checking, only fields we care about are checked, and we can use a
    # regex to approximate a field.
    like(
        $some_hash,
        {a => 1, b => qr/\A[0-9]+\z/},
        "'a' is 1, 'b' is an integer, we don't care about 'c'."
    );

=head2 ADVANCED

Declarative hash, array, and objects builders are available that allow you to
generate specifications. These are more verbose than simply providing a hash,
but have the advantage that every component you specify has a line number
associated. This is helpful for debugging as the failure output will tell you
not only which fields was incorrect, but also the line on which you declared
the field.

    use Test2::Tools::Compare qw{
        is like isnt unlike
        match mismatch validator
        hash array bag object meta number float rounded within string subset bool
        in_set not_in_set check_set
        item field call call_list call_hash prop check all_items all_keys all_vals all_values
        etc end filter_items
        T F D DF E DNE FDNE U L
        event fail_events
        exact_ref
    };

    is(
        $some_hash,
        hash {
            field a => 1;
            field b => 2;
            field c => 3;
        },
        "Hash matches spec"
    );

=head1 COMPARISON TOOLS

=over 4

=item $bool = is($got, $expect)

=item $bool = is($got, $expect, $name)

=item $bool = is($got, $expect, $name, @diag)

C<$got> is the data structure you want to check. C<$expect> is what you want
C<$got> to look like. C<$name> is an optional name for the test. C<@diag> is
optional diagnostics messages that will be printed to STDERR in event of
failure, they will not be displayed when the comparison is successful. The
boolean true/false result of the comparison is returned.

This is the strict checker. The strict checker requires a perfect match between
C<$got> and C<$expect>. All hash fields must be specified, all array items must
be present, etc. All non-scalar/hash/array/regex references must be identical
(same memory address). Scalar, hash and array references will be traversed and
compared. Regex references will be compared to see if they have the same
pattern.

    is(
        $some_hash,
        {a => 1, b => 2, c => 3},
        "The hash we got matches our expectations"
    );

The only exception to strictness is when it is given an C<$expect> object that
was built from a specification, in which case the specification determines the
strictness. Strictness only applies to literal values/references that are
provided and converted to a specification for you.

    is(
        $some_hash,
        hash {    # Note: the hash function is not exported by default
            field a => 1;
            field b => match(qr/\A[0-9]+\z/);    # Note: The match function is not exported by default
            # Don't care about other fields.
        },
        "The hash comparison is not strict"
    );

This works for both deep and shallow structures. For instance you can use this
to compare two strings:

    is('foo', 'foo', "strings match");

B<Note>: This is not the tool to use if you want to check if two references are
the same exact reference, use C<ref_is()> from the
L<Test2::Tools::Ref> plugin instead. I<Most> of the time this will
work as well, however there are problems if your reference contains a cycle and
refers back to itself at some point. If this happens, an exception will be
thrown to break an otherwise infinite recursion.

B<Note>: Non-reference values will be compared as strings using C<eq>, so that
means strings '2.0' and '2' will not match, but numeric 2.0 and 2 will, since
they are both stringified to '2'.

=item $bool = isnt($got, $expect)

=item $bool = isnt($got, $expect, $name)

=item $bool = isnt($got, $expect, $name, @diag)

Opposite of C<is()>. Does all the same checks, but passes when there is a
mismatch.

=item $bool = like($got, $expect)

=item $bool = like($got, $expect, $name)

=item $bool = like($got, $expect, $name, @diag)

C<$got> is the data structure you want to check. C<$expect> is what you want
C<$got> to look like. C<$name> is an optional name for the test. C<@diag> is
optional diagnostics messages that will be printed to STDERR in event of
failure, they will not be displayed when the comparison is successful. The
boolean true/false result of the comparison is returned.

This is the relaxed checker. This will ignore hash keys or array indexes that
you do not actually specify in your C<$expect> structure. In addition regex and
sub references will be used as validators. If you provide a regex using
C<qr/.../>, the regex itself will be used to validate the corresponding value
in the C<$got> structure. The same is true for coderefs, the value is passed in
as the first argument (and in C<$_>) and the sub should return a boolean value.
In this tool regexes will stringify the thing they are checking.

    like(
        $some_hash,
        {a => 1, b => qr/\A[0-9]+\z/},
        "'a' is 1, 'b' is an integer, we don't care about other fields"
    );

This works for both deep and shallow structures. For instance you can use this
to compare two strings:

    like('foo bar', qr/^foo/, "string matches the pattern");

=item $bool = unlike($got, $expect)

=item $bool = unlike($got, $expect, $name)

=item $bool = unlike($got, $expect, $name, @diag)

Opposite of C<like()>. Does all the same checks, but passes when there is a
mismatch.

=back

The C<is()>, C<isnt()>, C<like()>, and C<unlike()> functions can be made
to dump C<$got> using L<Data::Dumper> when tests fail by setting the
C<T2_AUTO_DUMP> environment variable to "1". (Alternatively, C<T2_AUTO_DUMP>
can be set to the name of a Perl module providing a compatible C<Dump()>
method.) The C<T2_AUTO_DEPARSE> environment variable can be used to
enable Data::Dumper's deparsing of coderefs.

=head2 QUICK CHECKS

B<Note: None of these are exported by default. You need to request them.>

Quick checks are a way to quickly generate a common value specification. These
can be used in structures passed into C<is> and C<like> through the C<$expect>
argument.

Example:

    is($foo, T(), '$foo has a true value');

=over 4

=item $check = T()

This verifies that the value in the corresponding C<$got> structure is
true, any true value will do.

    is($foo, T(), '$foo has a true value');

    is(
        { a => 'xxx' },
        { a => T() },
        "The 'a' key is true"
    );

=item $check = F()

This verifies that the value in the corresponding C<$got> structure is
false, any false value will do, B<but the value must exist>.

    is($foo, F(), '$foo has a false value');

    is(
        { a => 0 },
        { a => F() },
        "The 'a' key is false"
    );

It is important to note that a nonexistent value does not count as false. This
check will generate a failing test result:

    is(
        { a => 1 },
        { a => 1, b => F() },
        "The 'b' key is false"
    );

This will produce the following output:

    not ok 1 - The b key is false
    # Failed test "The 'b' key is false"
    # at some_file.t line 10.
    # +------+------------------+-------+---------+
    # | PATH | GOT              | OP    | CHECK   |
    # +------+------------------+-------+---------+
    # | {b}  | <DOES NOT EXIST> | FALSE | FALSE() |
    # +------+------------------+-------+---------+

In Perl, you can have behavior that is different for a missing key vs. a false
key, so it was decided not to count a completely absent value as false.
See the C<DNE()> shortcut below for checking that a field is missing.

If you want to check for false and/or DNE use the C<FDNE()> check.

=item $check = D()

This is to verify that the value in the C<$got> structure is defined. Any value
other than C<undef> will pass.

This will pass:

    is('foo', D(), 'foo is defined');

This will fail:

    is(undef, D(), 'foo is defined');

=item $check = U()

This is to verify that the value in the C<$got> structure is undefined.

This will pass:

    is(undef, U(), 'not defined');

This will fail:

    is('foo', U(), 'not defined');

=item $check = DF()

This is to verify that the value in the C<$got> structure is defined but false.
Any false value other than C<undef> will pass.

This will pass:

    is(0, DF(), 'foo is defined but false');

These will fail:

    is(undef, DF(), 'foo is defined but false');
    is(1, DF(), 'foo is defined but false');

=item $check = E()

This can be used to check that a value exists. This is useful to check that an
array has more values, or to check that a key exists in a hash, even if the
value is undefined.

These pass:

    is(['a', 'b', undef], ['a', 'b', E()], "There is a third item in the array");
    is({a => 1, b => 2}, {a => 1, b => E()}, "The 'b' key exists in the hash");

These will fail:

    is(['a', 'b'], ['a', 'b', E()], "Third item exists");
    is({a => 1}, {a => 1, b => E()}, "'b' key exists");

=item $check = DNE()

This can be used to check that no value exists. This is useful to check the end
bound of an array, or to check that a key does not exist in a hash.

These pass:

    is(['a', 'b'], ['a', 'b', DNE()], "There is no third item in the array");
    is({a => 1}, {a => 1, b => DNE()}, "The 'b' key does not exist in the hash");

These will fail:

    is(['a', 'b', 'c'], ['a', 'b', DNE()], "No third item");
    is({a => 1, b => 2}, {a => 1, b => DNE()}, "No 'b' key");

=item $check = FDNE()

This is a combination of C<F()> and C<DNE()>. This will pass for a false value,
or a nonexistent value.

=item $check = L()

This is to verify that the value in the C<$got> structure is defined and
has length.  Any value other than C<undef> or the empty string will pass
(including references).

These will pass:

    is('foo', L(), 'value is defined and has length');
    is([],    L(), 'value is defined and has length');

These will fail:

    is(undef, L(), 'value is defined and has length');
    is('',    L(), 'value is defined and has length');

=back

=head2 VALUE SPECIFICATIONS

B<Note: None of these are exported by default. You need to request them.>

=over 4

=item $check = string "..."

Verify that the value matches the given string using the C<eq> operator.

=item $check = !string "..."

Verify that the value does not match the given string using the C<ne> operator.

=item $check = number ...;

Verify that the value matches the given number using the C<==> operator.

=item $check = !number ...;

Verify that the value does not match the given number using the C<!=> operator.

=item $check = number_lt ...;

=item $check = number_le ...;

=item $check = number_ge ...;

=item $check = number_gt ...;

Verify that the value is less than, less than or equal to, greater than or
equal to, or greater than the given number.

=item $check = float ...;

Verify that the value is approximately equal to the given number.

If a 'precision' parameter is specified, both operands will be
rounded to 'precision' number of fractional decimal digits and
compared with C<eq>.

  is($near_val, float($val, precision => 4), "Near 4 decimal digits");

Otherwise, the check will be made within a range of +/- 'tolerance',
with a default 'tolerance' of 1e-08.

  is( $near_val, float($val, tolerance => 0.01), "Almost there...");

See also C<within> and C<rounded>.

=item $check = !float ...;

Verify that the value is not approximately equal to the given number.

If a 'precision' parameter is specified, both operands will be
rounded to 'precision' number of fractional decimal digits and
compared with C<eq>.

Otherwise, the check will be made within a range of +/- 'tolerance',
with a default 'tolerance' of 1e-08.

See also C<!within> and C<!rounded>.

=item $check = within($num, $tolerance);

Verify that the value approximately matches the given number,
within a range of +/- C<$tolerance>.  Compared using the C<==> operator.

C<$tolerance> is optional and defaults to 1e-08.

=item $check = !within($num, $tolerance);

Verify that the value does not approximately match the given number within a range of +/- C<$tolerance>.  Compared using the C<!=> operator.

C<$tolerance> is optional and defaults to 1e-08.

=item $check = rounded($num, $precision);

Verify that the value approximately matches the given number, when both are rounded to C<$precision> number of fractional digits. Compared using the C<eq> operator.

=item $check = !rounded($num, $precision);

Verify that the value does not approximately match the given number, when both are rounded to C<$precision> number of fractional digits. Compared using the C<ne> operator.

=item $check = bool ...;

Verify the value has the same boolean value as the given argument (XNOR).

=item $check = !bool ...;

Verify the value has a different boolean value from the given argument (XOR).

=item $check = check_isa ...;

Verify the value is an instance of the given class name.

=item $check = !check_isa ...;

Verify the value is not an instance of the given class name.

=item $check = match qr/.../

=item $check = !mismatch qr/.../

Verify that the value matches the regex pattern. This form of pattern check
will B<NOT> stringify references being checked.

B<Note:> C<!mismatch()> is documented for completion, please do not use it.

=item $check = !match qr/.../

=item $check = mismatch qr/.../

Verify that the value does not match the regex pattern. This form of pattern
check will B<NOT> stringify references being checked.

B<Note:> C<mismatch()> was created before overloading of C<!> for C<match()>
was a thing.

=item $check = validator(sub{ ... })

=item $check = validator($NAME => sub{ ... })

=item $check = validator($OP, $NAME, sub{ ... })

The coderef is the only required argument. The coderef should check that the
value is what you expect and return a boolean true or false. Optionally,
you can specify a name and operator that are used in diagnostics. They are also
provided to the sub itself as named parameters.

Check the value using this sub. The sub gets the value in C<$_>, and it
receives the value and several other items as named parameters.

    my $check = validator(sub {
        my %params = @_;

        # These both work:
        my $got = $_;
        my $got = $params{got};

        # Check if a value exists at all
        my $exists = $params{exists}

        # What $OP (if any) did we specify when creating the validator
        my $operator = $params{operator};

        # What name (if any) did we specify when creating the validator
        my $name = $params{name};

        ...

        return $bool;
    }

=item $check = exact_ref($ref)

Check that the value is exactly the same reference as the one provided.

=back

=head2 SET BUILDERS

B<Note: None of these are exported by default. You need to request them.>

=over 4

=item my $check = check_set($check1, $check2, ...)

Check that the value matches ALL of the specified checks.

=item my $check = in_set($check1, $check2, ...)

Check that the value matches ONE OR MORE of the specified checks.

=item not_in_set($check1, $check2, ...)

Check that the value DOES NOT match ANY of the specified checks.

=item check $thing

Check that the value matches the specified thing.

=back

=head2 HASH BUILDER

B<Note: None of these are exported by default. You need to request them.>

    $check = hash {
        field foo => 1;
        field bar => 2;

        # Ensure the 'baz' keys does not even exist in the hash.
        field baz => DNE();

        # Ensure the key exists, but is set to undef
        field bat => undef;

        # Any check can be used
        field boo => $check;

        # Set checks that apply to all keys or values. Can be done multiple
        # times, and each call can define multiple checks, all will be run.
        all_vals match qr/a/, match qr/b/;    # All values must have an 'a' and a 'b'
        all_keys match qr/x/;                 # All keys must have an 'x'

        ...

        end(); # optional, enforces that no other keys are present.
    };

=over 4

=item $check = hash { ... }

This is used to define a hash check.

=item field $NAME => $VAL

=item field $NAME => $CHECK

Specify a field check. This will check the hash key specified by C<$NAME> and
ensure it matches the value in C<$VAL>. You can put any valid check in C<$VAL>,
such as the result of another call to C<array { ... }>, C<DNE()>, etc.

B<Note:> This function can only be used inside a hash builder sub, and must be
called in void context.

=item all_keys($CHECK1, $CHECK2, ...)

Add checks that apply to all keys. You can put this anywhere in the hash
block, and can call it any number of times with any number of arguments.

=item all_vals($CHECK1, $CHECK2, ...)

=item all_values($CHECK1, $CHECK2, ...)

Add checks that apply to all values. You can put this anywhere in the hash
block, and can call it any number of times with any number of arguments.

=item end()

Enforce that no keys are found in the hash other than those specified. This is
essentially the C<use strict> of a hash check. This can be used anywhere in the
hash builder, though typically it is placed at the end.

=item etc()

Ignore any extra keys found in the hash. This is the opposite of C<end()>.
This can be used anywhere in the hash builder, though typically it is placed at
the end.

=item DNE()

This is a handy check that can be used with C<field()> to ensure that a field
(D)oes (N)ot (E)xist.

    field foo => DNE();

=back

=head2 ARRAY BUILDER

B<Note: None of these are exported by default. You need to request them.>

    $check = array {
        # Uses the next index, in this case index 0;
        item 'a';

        # Gets index 1 automatically
        item 'b';

        # Specify the index
        item 2 => 'c';

        # We skipped index 3, which means we don't care what it is.
        item 4 => 'e';

        # Gets index 5.
        item 'f';

        # Remove any REMAINING items that contain 0-9.
        filter_items { grep {!m/[0-9]/} @_ };

        # Set checks that apply to all items. Can be done multiple times, and
        # each call can define multiple checks, all will be run.
        all_items match qr/a/, match qr/b/;
        all_items match qr/x/;

        # Of the remaining items (after the filter is applied) the next one
        # (which is now index 6) should be 'g'.
        item 6 => 'g';

        item 7 => DNE; # Ensure index 7 does not exist.

        end(); # Ensure no other indexes exist.
    };

=over 4

=item $check = array { ... }

=item item $VAL

=item item $CHECK

=item item $IDX, $VAL

=item item $IDX, $CHECK

Add an expected item to the array. If C<$IDX> is not specified it will
automatically calculate it based on the last item added. You can skip indexes,
which means you do not want them to be checked.

You can provide any value to check in C<$VAL>, or you can provide any valid
check object.

B<Note:> Items MUST be added in order.

B<Note:> This function can only be used inside an array, bag or subset
builder sub, and must be called in void context.

=item filter_items { my @remaining = @_; ...; return @filtered }

This function adds a filter, all items remaining in the array from the point
the filter is reached will be passed into the filter sub as arguments, the sub
should return only the items that should be checked.

B<Note:> This function can only be used inside an array builder sub, and must
be called in void context.

=item all_items($CHECK1, $CHECK2, ...)

Add checks that apply to all items. You can put this anywhere in the array
block, and can call it any number of times with any number of arguments.

=item end()

Enforce that there are no indexes after the last one specified. This will not
force checking of skipped indexes.

=item etc()

Ignore any extra items found in the array. This is the opposite of C<end()>.
This can be used anywhere in the array builder, though typically it is placed
at the end.

=item DNE()

This is a handy check that can be used with C<item()> to ensure that an index
(D)oes (N)ot (E)xist.

    item 5 => DNE();

=back

=head2 BAG BUILDER

B<Note: None of these are exported by default. You need to request them.>

    $check = bag {
        item 'a';
        item 'b';

        end(); # Ensure no other elements exist.
    };

A bag is like an array, but we don't care about the order of the
items. In the example, C<$check> would match both C<['a','b']> and
C<['b','a']>.

=over 4

=item $check = bag { ... }

=item item $VAL

=item item $CHECK

Add an expected item to the bag.

You can provide any value to check in C<$VAL>, or you can provide any valid
check object.

B<Note:> This function can only be used inside an array, bag or subset
builder sub, and must be called in void context.

=item all_items($CHECK1, $CHECK2, ...)

Add checks that apply to all items. You can put this anywhere in the bag
block, and can call it any number of times with any number of arguments.

=item end()

Enforce that there are no more items after the last one specified.

=item etc()

Ignore any extra items found in the array. This is the opposite of C<end()>.
This can be used anywhere in the bag builder, though typically it is placed
at the end.

=back

=head2 ORDERED SUBSET BUILDER

B<Note: None of these are exported by default. You need to request them.>

    $check = subset {
        item 'a';
        item 'b';
        item 'c';

        # Doesn't matter if the array has 'd', the check will skip past any
        # unknown items until it finds the next one in our subset.

        item 'e';
        item 'f';
    };

=over 4

=item $check = subset { ... }

=item item $VAL

=item item $CHECK

Add an expected item to the subset.

You can provide any value to check in C<$VAL>, or you can provide any valid
check object.

B<Note:> Items MUST be added in order.

B<Note:> This function can only be used inside an array, bag or subset
builder sub, and must be called in void context.

=back

=head2 META BUILDER

B<Note: None of these are exported by default. You need to request them.>

    my $check = meta {
        prop blessed => 'My::Module'; # Ensure value is blessed as our package
        prop reftype => 'HASH';       # Ensure value is a blessed hash
        prop isa     => 'My::Base';   # Ensure value is an instance of our class
        prop size    => 4;            # Check the number of hash keys
        prop this    => ...;          # Check the item itself
    };

=over 4

=item meta { ... }

=item meta_check { ... }

Build a meta check. If you are using L<Moose> then the C<meta()> function would
conflict with the one exported by L<Moose>, in such cases C<meta_check()> is
available. Neither is exported by default.

=item prop $NAME => $VAL

=item prop $NAME => $CHECK

Check the property specified by C<$name> against the value or check.

Valid properties are:

=over 4

=item 'blessed'

What package (if any) the thing is blessed as.

=item 'reftype'

Reference type (if any) the thing is.

=item 'isa'

What class the thing is an instance of.

=item 'this'

The thing itself.

=item 'size'

For array references this returns the number of elements. For hashes this
returns the number of keys. For everything else this returns undef.

=back

=back

=head2 OBJECT BUILDER

B<Note: None of these are exported by default. You need to request them.>

    my $check = object {
        call foo => 1; # Call the 'foo' method, check the result.

        # Call the specified sub-ref as a method on the object, check the
        # result. This is useful for wrapping methods that return multiple
        # values.
        call sub { [ shift->get_list ] } => [...];

        # This can be used to ensure a method does not exist.
        call nope => DNE();

        # Check the hash key 'foo' of the underlying reference, this only works
        # on blessed hashes.
        field foo => 1;

        # Check the value of index 4 on the underlying reference, this only
        # works on blessed arrays.
        item 4 => 'foo';

        # Check the meta-property 'blessed' of the object.
        prop blessed => 'My::Module';

        # Check if the object is an instance of the specified class.
        prop isa => 'My::Base';

        # Ensure only the specified hash keys or array indexes are present in
        # the underlying hash. Has no effect on meta-property checks or method
        # checks.
        end();
    };

=over 4

=item $check = object { ... }

Specify an object check for use in comparisons.

=item call $METHOD_NAME => $RESULT

=item call $METHOD_NAME => $CHECK

=item call [$METHOD_NAME, @METHOD_ARGS] => $RESULT

=item call [$METHOD_NAME, @METHOD_ARGS] => $CHECK

=item call sub { ... }, $RESULT

=item call sub { ... }, $CHECK

Call the specified method (or coderef) and verify the result. If you
pass an arrayref, the first element must be the method name, the
others are the arguments it will be called with.

The coderef form is useful if you need to do something more complex.

    my $ref = sub {
      local $SOME::GLOBAL::THING = 3;
      return [shift->get_values_for('thing')];
    };

    call $ref => ...;

=item call_list $METHOD_NAME => $RESULT

=item call_list $METHOD_NAME => $CHECK

=item call_list [$METHOD_NAME, @METHOD_ARGS] => $RESULT

=item call_list [$METHOD_NAME, @METHOD_ARGS] => $CHECK

=item call_list sub { ... }, $RESULT

=item call_list sub { ... }, $CHECK

Same as C<call>, but the method is invoked in list context, and the
result is always an arrayref.

    call_list get_items => [ ... ];

=item call_hash $METHOD_NAME => $RESULT

=item call_hash $METHOD_NAME => $CHECK

=item call_hash [$METHOD_NAME, @METHOD_ARGS] => $RESULT

=item call_hash [$METHOD_NAME, @METHOD_ARGS] => $CHECK

=item call_hash sub { ... }, $RESULT

=item call_hash sub { ... }, $CHECK

Same as C<call>, but the method is invoked in list context, and the
result is always a hashref. This will warn if the method returns an
odd number of values.

    call_hash get_items => { ... };

=item field $NAME => $VAL

Works just like it does for hash checks.

=item item $VAL

=item item $IDX, $VAL

Works just like it does for array checks.

=item prop $NAME => $VAL

=item prop $NAME => $CHECK

Check the property specified by C<$name> against the value or check.

Valid properties are:

=over 4

=item 'blessed'

What package (if any) the thing is blessed as.

=item 'reftype'

Reference type (if any) the thing is.

=item 'isa'

What class the thing is an instance of.

=item 'this'

The thing itself.

=item 'size'

For array references this returns the number of elements. For hashes this
returns the number of keys. For everything else this returns undef.

=back

=item DNE()

Can be used with C<item>, or C<field> to ensure the hash field or array index
does not exist. Can also be used with C<call> to ensure a method does not
exist.

=item end()

Turn on strict array/hash checking, ensuring that no extra keys/indexes
are present.

=item etc()

Ignore any extra items found in the hash/array. This is the opposite of
C<end()>.  This can be used anywhere in the builder, though typically it is
placed at the end.

=back

=head2 EVENT BUILDERS

B<Note: None of these are exported by default. You need to request them.>

Check that we got an event of a specified type:

    my $check = event 'Ok';

Check for details about the event:

    my $check = event Ok => sub {
        # Check for a failure
        call pass => 0;

        # Effective pass after TODO/SKIP are accounted for.
        call effective_pass => 1;

        # Check the diagnostics
        call diag => [ match qr/Failed test foo/ ];

        # Check the file the event reports to
        prop file => 'foo.t';

        # Check the line number the event reports to
        prop line => '42';

        # You can check the todo/skip values as well:
        prop skip => 'broken';
        prop todo => 'fixme';

        # Thread-id and process-id where event was generated
        prop tid => 123;
        prop pid => 123;
    };

You can also provide a fully qualified event package with the '+' prefix:

    my $check = event '+My::Event' => sub { ... }

You can also provide a hashref instead of a sub to directly check hash values
of the event:

    my $check = event Ok => { pass => 1, ... };

=head3 USE IN OTHER BUILDERS

You can use these all in other builders, simply use them in void context to
have their value(s) appended to the build.

    my $check = array {
        event Ok => { ... };
        event Note => { ... };

        fail_events Ok => { pass => 0 };
        # Get a Diag for free.
    };

=head3 SPECIFICS

=over 4

=item $check = event $TYPE;

=item $check = event $TYPE => sub { ... };

=item $check = event $TYPE => { ... };

This works just like an object builder. In addition to supporting everything
the object check supports, you also have to specify the event type, and many
extra meta-properties are available.

Extra properties are:

=over 4

=item 'file'

File name to which the event reports (for use in diagnostics).

=item 'line'

Line number to which the event reports (for use in diagnostics).

=item 'package'

Package to which the event reports (for use in diagnostics).

=item 'subname'

Sub that was called to generate the event (example: C<ok()>).

=item 'skip'

Set to the skip value if the result was generated by skipping tests.

=item 'todo'

Set to the todo value if TODO was set when the event was generated.

=item 'trace'

The C<at file foo.t line 42> string that will be used in diagnostics.

=item 'tid'

Thread ID in which the event was generated.

=item 'pid'

Process ID in which the event was generated.

=back

B<NOTE>: Event checks have an implicit C<etc()> added. This means you need to
use C<end()> if you want to fail on unexpected hash keys or array indexes. This
implicit C<etc()> extends to all forms, including builder, hashref, and no
argument.

=item @checks = fail_events $TYPE;

=item @checks = fail_events $TYPE => sub { ... };

=item @checks = fail_events $TYPE => { ... };

Just like C<event()> documented above. The difference is that this produces two
events, the one you specify, and a C<Diag> after it. There are no extra checks
in the Diag.

Use this to validate a simple failure where you do not want to be bothered with
the default diagnostics. It only adds a single Diag check, so if your failure
has custom diagnostics you will need to add checks for them.

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
