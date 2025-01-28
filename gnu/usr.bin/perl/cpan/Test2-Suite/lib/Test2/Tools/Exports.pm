package Test2::Tools::Exports;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/croak carp/;
use Test2::API qw/context/;
use Test2::Util::Stash qw/get_symbol/;

our @EXPORT = qw/imported_ok not_imported_ok/;
use base 'Exporter';

sub imported_ok {
    my $ctx     = context();
    my $caller  = caller;
    my @missing = grep { !get_symbol($_, $caller) } @_;

    my $name = "Imported symbol";
    $name .= "s" if @_ > 1;
    $name .= ": ";
    my $list = join(", ", @_);
    substr($list, 37, length($list) - 37, '...') if length($list) > 40;
    $name .= $list;

    $ctx->ok(!@missing, $name, [map { "'$_' was not imported." } @missing]);

    $ctx->release;

    return !@missing;
}

sub not_imported_ok {
    my $ctx    = context();
    my $caller = caller;
    my @found  = grep { get_symbol($_, $caller) } @_;

    my $name = "Did not imported symbol";
    $name .= "s" if @_ > 1;
    $name .= ": ";
    my $list = join(", ", @_);
    substr($list, 37, length($list) - 37, '...') if length($list) > 40;
    $name .= $list;

    $ctx->ok(!@found, $name, [map { "'$_' was imported." } @found]);

    $ctx->release;

    return !@found;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Tools::Exports - Tools for validating exporters.

=head1 DESCRIPTION

These are tools for checking that symbols have been imported into your
namespace.

=head1 SYNOPSIS

    use Test2::Tools::Exports

    use Data::Dumper;
    imported_ok qw/Dumper/;
    not_imported_ok qw/dumper/;

=head1 EXPORTS

All subs are exported by default.

=over 4

=item imported_ok(@SYMBOLS)

Check that the specified symbols exist in the current package. This will not
find inherited subs. This will only find symbols in the current package's symbol
table. This B<WILL NOT> confirm that the symbols were defined outside of the
package itself.

    imported_ok( '$scalar', '@array', '%hash', '&sub', 'also_a_sub' );

C<@SYMBOLS> can contain any number of symbols. Each item in the array must be a
string. The string should be the name of a symbol. If a sigil is present then
it will search for that specified type, if no sigil is specified it will be
used as a sub name.

=item not_imported_ok(@SYMBOLS)

Check that the specified symbols do not exist in the current package. This will
not find inherited subs. This will only look at symbols in the current package's
symbol table.

    not_imported_ok( '$scalar', '@array', '%hash', '&sub', 'also_a_sub' );

C<@SYMBOLS> can contain any number of symbols. Each item in the array must be a
string. The string should be the name of a symbol. If a sigil is present, then
it will search for that specified type. If no sigil is specified, it will be
used as a sub name.

=back

=head1 CAVEATS

Before Perl 5.10, it is very difficult to distinguish between a package scalar
that is undeclared vs declared and undefined. Currently C<imported_ok> and
C<not_imported_ok> cannot see package scalars declared using C<our $var> unless
the variable has been assigned a defined value.

This will pass on recent perls, but fail on perls older than 5.10:

    use Test2::Tools::Exports;

    our $foo;

    # Fails on perl onlder than 5.10
    imported_ok(qw/$foo/);

If C<$foo> is imported from another module, or imported using
C<use vars qw/$foo/;> then it will work on all supported perl versions.

    use Test2::Tools::Exports;

    use vars qw/$foo/;
    use Some::Module qw/$bar/;

    # Always works
    imported_ok(qw/$foo $bar/);

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
