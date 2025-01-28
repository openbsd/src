package Test2::Compare::Delta;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::Util::HashBase qw{verified id got chk children dne exception note};

use Test2::EventFacet::Info::Table;

use Test2::Util::Table();
use Test2::API qw/context/;

use Test2::Util::Ref qw/render_ref rtype/;
use Carp qw/croak/;

# 'CHECK' constant would not work, but I like exposing 'check()' to people
# using this class.
BEGIN {
    no warnings 'once';
    *check = \&chk;
    *set_check = \&set_chk;
}

my @COLUMN_ORDER = qw/PATH GLNs GOT OP CHECK CLNs/;
my %COLUMNS = (
    GOT   => {name => 'GOT',   value => sub { $_[0]->render_got },   no_collapse => 1},
    CHECK => {name => 'CHECK', value => sub { $_[0]->render_check }, no_collapse => 1},
    OP    => {name => 'OP',    value => sub { $_[0]->table_op }                      },
    PATH  => {name => 'PATH',  value => sub { $_[1] }                                },

    'GLNs' => {name => 'GLNs', alias => 'LNs', value => sub { $_[0]->table_got_lines }  },
    'CLNs' => {name => 'CLNs', alias => 'LNs', value => sub { $_[0]->table_check_lines }},
);
{
    my $i = 0;
    $COLUMNS{$_}->{id} = $i++ for @COLUMN_ORDER;
}

sub remove_column {
    my $class = shift;
    my $header = shift;
    @COLUMN_ORDER = grep { $_ ne $header } @COLUMN_ORDER;
    delete $COLUMNS{$header} ? 1 : 0;
}

sub add_column {
    my $class = shift;
    my $name = shift;

    croak "Column name is required"
        unless $name;

    croak "Column '$name' is already defined"
        if $COLUMNS{$name};

    my %params;
    if (@_ == 1) {
        %params = (value => @_, name => $name);
    }
    else {
        %params = (@_, name => $name);
    }

    my $value = $params{value};

    croak "You must specify a 'value' callback"
        unless $value;

    croak "'value' callback must be a CODE reference"
        unless rtype($value) eq 'CODE';

    if ($params{prefix}) {
        unshift @COLUMN_ORDER => $name;
    }
    else {
        push @COLUMN_ORDER => $name;
    }

    $COLUMNS{$name} = \%params;
}

sub set_column_alias {
    my ($class, $name, $alias) = @_;

    croak "Tried to alias a non-existent column"
        unless exists $COLUMNS{$name};

    croak "Missing alias" unless defined $alias;

    $COLUMNS{$name}->{alias} = $alias;
}

sub init {
    my $self = shift;

    croak "Cannot specify both 'check' and 'chk' as arguments"
        if exists($self->{check}) && exists($self->{+CHK});

    # Allow 'check' as an argument
    $self->{+CHK} ||= delete $self->{check}
        if exists $self->{check};
}

sub render_got {
    my $self = shift;

    my $exp = $self->{+EXCEPTION};
    if ($exp) {
        chomp($exp = "$exp");
        $exp =~ s/\n.*$//g;
        return "<EXCEPTION: $exp>";
    }

    my $dne = $self->{+DNE};
    return '<DOES NOT EXIST>' if $dne && $dne eq 'got';

    my $got = $self->{+GOT};
    return '<UNDEF>' unless defined $got;

    my $check = $self->{+CHK};
    my $stringify = defined( $check ) && $check->stringify_got;

    return render_ref($got) if ref $got && !$stringify;

    return "$got";
}

sub render_check {
    my $self = shift;

    my $dne = $self->{+DNE};
    return '<DOES NOT EXIST>' if $dne && $dne eq 'check';

    my $check = $self->{+CHK};
    return '<UNDEF>' unless defined $check;

    return $check->render;
}

sub _full_id {
    my ($type, $id) = @_;
    return "<$id>" if !$type || $type eq 'META';
    return $id     if $type eq 'SCALAR';
    return "{$id}" if $type eq 'HASH';
    return "{$id} <KEY>" if $type eq 'HASHKEY';
    return "[$id]" if $type eq 'ARRAY';
    return "$id()" if $type eq 'METHOD';
    return "$id" if $type eq 'DEREF';
    return "<$id>";
}

sub _arrow_id {
    my ($path, $type) = @_;
    return '' unless $path;

    return ' ' if !$type || $type eq 'META';    # Meta gets a space, not an arrow

    return '->' if $type eq 'METHOD';           # Method always needs an arrow
    return '->' if $type eq 'SCALAR';           # Scalar always needs an arrow
    return '->' if $type eq 'DEREF';            # deref always needs arrow
    return '->' if $path =~ m/(>|\(\))$/;       # Need an arrow after meta, or after a method
    return '->' if $path eq '$VAR';             # Need an arrow after the initial ref

    # Hash and array need an arrow unless they follow another hash/array
    return '->' if $type =~ m/^(HASH|ARRAY)$/ && $path !~ m/(\]|\})$/;

    # No arrow needed
    return '';
}

sub _join_id {
    my ($path, $parts) = @_;
    my ($type, $key) = @$parts;

    my $id   = _full_id($type, $key);
    my $join = _arrow_id($path, $type);

    return "${path}${join}${id}";
}

sub should_show {
    my $self = shift;
    return 1 unless $self->verified;
    defined( my $check = $self->check ) || return 0;
    return 0 unless $check->lines;
    my $file = $check->file || return 0;

    my $ctx = context();
    my $cfile = $ctx->trace->file;
    $ctx->release;
    return 0 unless $file eq $cfile;

    return 1;
}

sub filter_visible {
    my $self = shift;

    my @deltas;
    my @queue = (['', $self]);

    while (my $set = shift @queue) {
        my ($path, $delta) = @$set;

        push @deltas => [$path, $delta] if $delta->should_show;

        my $children = $delta->children || next;
        next unless @$children;

        my @new;
        for my $child (@$children) {
            my $cpath = _join_id($path, $child->id);
            push @new => [$cpath, $child];
        }
        unshift @queue => @new;
    }

    return \@deltas;
}

sub table_header { [map {$COLUMNS{$_}->{alias} || $_} @COLUMN_ORDER] }

sub table_op {
    my $self = shift;

    defined( my $check = $self->{+CHK} ) || return '!exists';

    return $check->operator($self->{+GOT})
        unless $self->{+DNE} && $self->{+DNE} eq 'got';

    return $check->operator();
}

sub table_check_lines {
    my $self = shift;

    defined( my $check = $self->{+CHK} ) || return '';
    my $lines = $check->lines || return '';

    return '' unless @$lines;

    return join ', ' => @$lines;
}

sub table_got_lines {
    my $self = shift;

    defined( my $check = $self->{+CHK} ) || return '';
    return '' if $self->{+DNE} && $self->{+DNE} eq 'got';

    my @lines = $check->got_lines($self->{+GOT});
    return '' unless @lines;

    return join ', ' => @lines;
}

sub table_rows {
    my $self = shift;

    my $deltas = $self->filter_visible;

    my @rows;
    for my $set (@$deltas) {
        my ($id, $d) = @$set;

        my @row;
        for my $col (@COLUMN_ORDER) {
            my $spec = $COLUMNS{$col};
            my $val = $spec->{value}->($d, $id);
            $val = '' unless defined $val;
            push @row => $val;
        }

        push @rows => \@row;
    }

    return \@rows;
}

sub table {
    my $self = shift;

    my @diag;
    my $header = $self->table_header;
    my $rows   = $self->table_rows;

    my $render_rows = [@$rows];
    my $max = exists $ENV{TS_MAX_DELTA} ? $ENV{TS_MAX_DELTA} : 25;
    if ($max && @$render_rows > $max) {
        @$render_rows = map { [@$_] } @{$render_rows}[0 .. ($max - 1)];
        @diag = (
            "************************************************************",
            sprintf("* Stopped after %-42.42s *", "$max differences."),
            "* Set the TS_MAX_DELTA environment var to raise the limit. *",
            "* Set it to 0 for no limit.                                *",
            "************************************************************",
        );
    }

    my @dne;
    for my $row (@$render_rows) {
        my $got = $row->[$COLUMNS{GOT}->{id}]   || '';
        my $chk = $row->[$COLUMNS{CHECK}->{id}] || '';
        if ($got eq '<DOES NOT EXIST>') {
            push @dne => "$row->[$COLUMNS{PATH}->{id}]:   DOES NOT EXIST";
        }
        elsif ($chk eq '<DOES NOT EXIST>') {
            push @dne => "$row->[$COLUMNS{PATH}->{id}]: SHOULD NOT EXIST";
        }
    }

    if (@dne) {
        unshift @dne => '==== Summary of missing/extra items ====';
        push    @dne => '== end summary of missing/extra items ==';
    }

    my $table_args = {
        header      => $header,
        collapse    => 1,
        sanitize    => 1,
        mark_tail   => 1,
        no_collapse => [grep { $COLUMNS{$COLUMN_ORDER[$_]}->{no_collapse} } 0 .. $#COLUMN_ORDER],
    };

    my $render = join "\n" => (
        Test2::Util::Table::table(%$table_args, rows => $render_rows),
        @dne,
        @diag,
    );

    my $table = Test2::EventFacet::Info::Table->new(
        %$table_args,
        rows      => $rows,
        as_string => $render,
    );

    return $table;
}

sub diag { shift->table }

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Delta - Representation of differences between nested data
structures.

=head1 DESCRIPTION

This is used by L<Test2::Compare>. When data structures are compared a
delta will be returned. Deltas are a tree data structure that represent all the
differences between two other data structures.

=head1 METHODS

=head2 CLASS METHODS

=over 4

=item $class->add_column($NAME => sub { ... })

=item $class->add_column($NAME, %PARAMS)

This can be used to add columns to the table that it produced when a comparison
fails. The first argument should always be the column name, which must be
unique.

The first form simply takes a coderef that produces the value that should be
displayed in the column for any given delta. The arguments passed into the sub
are the delta, and the row ID.

    Test2::Compare::Delta->add_column(
        Foo => sub {
            my ($delta, $id) = @_;
            return $delta->... ? 'foo' : 'bar'
        },
    );

The second form allows you some extra options. The C<'value'> key is required,
and must be a coderef. All other keys are optional.

    Test2::Compare::Delta->add_column(
        'Foo',    # column name
        value => sub { ... },    # how to get the cell value
        alias       => 'FOO',    # Display name (used in table header)
        no_collapse => $bool,    # Show column even if it has no values?
    );

=item $bool = $class->remove_column($NAME)

This will remove the specified column. This will return true if the column
existed and was removed. This will return false if the column did not exist. No
exceptions are thrown. If a missing column is a problem then you need to check
the return yourself.

=item $class->set_column_alias($NAME, $ALIAS)

This can be used to change the table header, overriding the default column
names with new ones.

=back

=head2 ATTRIBUTES

=over 4

=item $bool = $delta->verified

=item $delta->set_verified($bool)

This will be true if the delta itself matched, if the delta matched then the
problem is in the delta's children, not the delta itself.

=item $aref = $delta->id

=item $delta->set_id([$type, $name])

ID for the delta, used to produce the path into the data structure. An
example is C<< ['HASH' => 'foo'] >> which means the delta is in the path
C<< ...->{'foo'} >>. Valid types are C<HASH>, C<ARRAY>, C<SCALAR>, C<META>, and
C<METHOD>.

=item $val = $delta->got

=item $delta->set_got($val)

Deltas are produced by comparing a received data structure 'got' against a
check data structure 'check'. The 'got' attribute contains the value that was
received for comparison.

=item $check = $delta->chk

=item $check = $delta->check

=item $delta->set_chk($check)

=item $delta->set_check($check)

Deltas are produced by comparing a received data structure 'got' against a
check data structure 'check'. The 'check' attribute contains the value that was
expected in the comparison.

C<check> and C<chk> are aliases for the same attribute.

=item $aref = $delta->children

=item $delta->set_children([$delta1, $delta2, ...])

A Delta may have child deltas. If it does then this is an arrayref with those
children.

=item $dne = $delta->dne

=item $delta->set_dne($dne)

Sometimes a comparison results in one side or the other not existing at all, in
which case this is set to the name of the attribute that does not exist. This
can be set to 'got' or 'check'.

=item $e = $delta->exception

=item $delta->set_exception($e)

This will be set to the exception in cases where the comparison failed due to
an exception being thrown.

=back

=head2 OTHER

=over 4

=item $string = $delta->render_got

Renders the string that should be used in a table to represent the received
value in a comparison.

=item $string = $delta->render_check

Renders the string that should be used in a table to represent the expected
value in a comparison.

=item $bool = $delta->should_show

This will return true if the delta should be shown in the table. This is
normally true for any unverified delta. This will also be true for deltas that
contain extra useful debug information.

=item $aref = $delta->filter_visible

This will produce an arrayref of C<< [ $path => $delta ] >> for all deltas that
should be displayed in the table.

=item $aref = $delta->table_header

This returns an array ref of the headers for the table.

=item $string = $delta->table_op

This returns the operator that should be shown in the table.

=item $string = $delta->table_check_lines

This returns the defined lines (extra debug info) that should be displayed.

=item $string = $delta->table_got_lines

This returns the generated lines (extra debug info) that should be displayed.

=item $aref = $delta->table_rows

This returns an arrayref of table rows, each row is itself an arrayref.

=item @table_lines = $delta->table

Returns all the lines of the table that should be displayed.

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
