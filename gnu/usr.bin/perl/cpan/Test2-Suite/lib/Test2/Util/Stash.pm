package Test2::Util::Stash;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak/;
use B;

our @EXPORT_OK = qw{
    get_stash
    get_glob
    get_symbol
    parse_symbol
    purge_symbol
    slot_to_sig sig_to_slot
};
use base 'Exporter';

my %SIGMAP = (
    '&' => 'CODE',
    '$' => 'SCALAR',
    '%' => 'HASH',
    '@' => 'ARRAY',
);

my %SLOTMAP = reverse %SIGMAP;

sub slot_to_sig { $SLOTMAP{$_[0]} || croak "unsupported slot: '$_[0]'" }
sub sig_to_slot { $SIGMAP{$_[0]}  || croak "unsupported sigil: $_[0]"  }

sub get_stash {
    my $package = shift || caller;
    no strict 'refs';
    return \%{"${package}\::"};
}

sub get_glob {
    my $sym = _parse_symbol(scalar(caller), @_);
    no strict 'refs';
    no warnings 'once';
    return \*{"$sym->{package}\::$sym->{name}"};
}

sub parse_symbol { _parse_symbol(scalar(caller), @_) }

sub _parse_symbol {
    my ($caller, $symbol, $package) = @_;

    if (ref($symbol)) {
        my $pkg = $symbol->{package};

        croak "Symbol package ($pkg) and package argument ($package) do not match"
            if $pkg && $package && $pkg ne $package;

        $symbol->{package} ||= $caller;

        return $symbol;
    }

    utf8::downgrade($symbol) if $] == 5.010000; # prevent crash on 5.10.0
    my ($sig, $pkg, $name) = ($symbol =~ m/^(\W?)(.*::)?([^:]+)$/)
        or croak "Invalid symbol: '$symbol'";

    # Normalize package, '::' becomes 'main', 'Foo::' becomes 'Foo'
    $pkg = $pkg
        ? $pkg eq '::'
            ? 'main'
            : substr($pkg, 0, -2)
        : undef;

    croak "Symbol package ($pkg) and package argument ($package) do not match"
        if $pkg && $package && $pkg ne $package;

    $sig ||= '&';
    my $type = $SIGMAP{$sig} || croak "unsupported sigil: '$sig'";

    my $real_package = $package || $pkg || $caller;

    return {
        name    => $name,
        sigil   => $sig,
        type    => $type,
        symbol  => "${sig}${real_package}::${name}",
        package => $real_package,
    };
}

sub get_symbol {
    my $sym = _parse_symbol(scalar(caller), @_);

    my $name    = $sym->{name};
    my $type    = $sym->{type};
    my $package = $sym->{package};
    my $symbol  = $sym->{symbol};

    my $stash = get_stash($package);
    return undef unless exists $stash->{$name};

    my $glob = get_glob($sym);
    return *{$glob}{$type} if $type ne 'SCALAR' && defined(*{$glob}{$type});

    if ($] < 5.010) {
        return undef unless defined(*{$glob}{$type});

        {
            local ($@, $!);
            local $SIG{__WARN__} = sub { 1 };
            return *{$glob}{$type} if eval "package $package; my \$y = $symbol; 1";
        }

        return undef unless defined *{$glob}{$type};
        return *{$glob}{$type} if defined ${*{$glob}{$type}};
        return undef;
    }

    my $sv = B::svref_2object($glob)->SV;
    return *{$glob}{$type} if $sv->isa('B::SV');
    return undef unless $sv->isa('B::SPECIAL');
    return *{$glob}{$type} if $B::specialsv_name[$$sv] ne 'Nullsv';
    return undef;
}

sub purge_symbol {
    my $sym = _parse_symbol(scalar(caller), @_);

    local *GLOBCLONE = *{get_glob($sym)};
    delete get_stash($sym->{package})->{$sym->{name}};
    my $new_glob = get_glob($sym);

    for my $type (qw/CODE SCALAR HASH ARRAY FORMAT IO/) {
        next if $type eq $sym->{type};
        my $ref = get_symbol({type => $type, name => 'GLOBCLONE', sigil => $SLOTMAP{$type}}, __PACKAGE__);
        next unless $ref;
        *$new_glob = $ref;
    }

    return *GLOBCLONE{$sym->{type}};
}

1;

__END__


=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Stash - Utilities for manipulating stashes and globs.

=head1 DESCRIPTION

This is a collection of utilities for manipulating and inspecting package
stashes and globs.

=head1 EXPORTS

=over 4

=item $stash = get_stash($package)

Gets the package stash. This is the same as C<$stash = \%Package::Name::>.

=item $sym_spec = parse_symbol($symbol)

=item $sym_spec = parse_symbol($symbol, $package)

Parse a symbol name, and return a hashref with info about the symbol.

C<$symbol> can be a simple name, or a fully qualified symbol name. The sigil is
optional, and C<&> is assumed if none is provided. If C<$symbol> is fully qualified,
and C<$package> is also provided, then the package of the symbol must match the
C<$package>.

Returns a structure like this:

    return {
        name    => 'BAZ',
        sigil   => '$',
        type    => 'SCALAR',
        symbol  => '&Foo::Bar::BAZ',
        package => 'Foo::Bar',
    };

=item $glob_ref = get_glob($symbol)

=item $glob_ref = get_glob($symbol, $package)

Get a glob ref. Arguments are the same as for C<parse_symbol>.

=item $ref = get_symbol($symbol)

=item $ref = get_symbol($symbol, $package)

Get a reference to the symbol. Arguments are the same as for C<parse_symbol>.

=item $ref = purge_symbol($symbol)

=item $ref = purge_symbol($symbol, $package)

Completely remove the symbol from the package symbol table. Arguments are the
same as for C<parse_symbol>. A reference to the removed symbol is returned.

=item $sig = slot_to_sig($slot)

Convert a slot (like 'SCALAR') to a sigil (like '$').

=item $slot = sig_to_slot($sig)

Convert a sigil (like '$') to a slot (like 'SCALAR').

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
