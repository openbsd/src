package Test2::Util::Ref;
use strict;
use warnings;

our $VERSION = '0.000162';

use Scalar::Util qw/reftype blessed refaddr/;

our @EXPORT_OK = qw/rtype render_ref/;
use base 'Exporter';

sub rtype {
    my ($thing) = @_;
    return '' unless defined $thing;

    my $rf = ref $thing;
    my $rt = reftype $thing;

    return '' unless $rf || $rt;
    return 'REGEXP' if $rf =~ m/Regex/i;
    return 'REGEXP' if $rt =~ m/Regex/i;
    return $rt || '';
}

sub render_ref {
    my ($in) = @_;

    return 'undef' unless defined($in);

    my $type = rtype($in);
    return "$in" unless $type;

    # Look past overloading
    my $class = blessed($in) || '';

    my $it = sprintf('0x%x', refaddr($in));
    my $ref = "$type($it)";

    return $ref unless $class;

    my $out = "$class=$ref";
    if ($class =~ m/bool/i) {
        my $bool = $in ? 'TRUE' : 'FALSE';
        return "<$bool: $out>";
    }
    return $out;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Ref - Tools for inspecting or manipulating references.

=head1 DESCRIPTION

These are used by L<Test2::Tools> to inspect, render, or manipulate references.

=head1 EXPORTS

All exports are optional. You must specify subs to import.

=over 4

=item $type = rtype($ref)

A normalization between C<Scalar::Util::reftype()> and C<ref()>.

Always returns a string.

Returns C<'REGEXP'> for regex types

Returns C<''> for non-refs

Otherwise returns what C<Scalar::Util::reftype()> returns.

=item $addr_str = render_ref($ref)

Always returns a string. For unblessed references this returns something like
C<"SCALAR(0x...)">. For blessed references it returns
C<"My::Thing=SCALAR(0x...)">. The only difference between this and C<$add_str =
"$thing"> is that it ignores any overloading to ensure it is always the ref
address.

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

=item Kent Fredric E<lt>kentnl@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
