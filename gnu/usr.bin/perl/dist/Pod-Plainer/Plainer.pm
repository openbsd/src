package Pod::Plainer;
use 5.006;
use strict;
use warnings;
use if $] >= 5.011, 'deprecate';
use Pod::Parser;
our @ISA = qw(Pod::Parser);
our $VERSION = '1.02';

our %E = qw( < lt > gt );
 
sub escape_ltgt {
    (undef, my $text) = @_;
    $text =~ s/([<>])/E<$E{$1}>/g;
    $text 
} 

sub simple_delimiters {
    (undef, my $seq) = @_;
    $seq -> left_delimiter( '<' ); 
    $seq -> right_delimiter( '>' );  
    $seq;
}

sub textblock {
    my($parser,$text,$line) = @_;
    print {$parser->output_handle()}
	$parser->parse_text(
	    { -expand_text => q(escape_ltgt),
	      -expand_seq => q(simple_delimiters) },
	    $text, $line ) -> raw_text(); 
}

1;

__END__

=head1 NAME

Pod::Plainer - Perl extension for converting Pod to old-style Pod.

=head1 SYNOPSIS

  use Pod::Plainer;

  my $parser = Pod::Plainer -> new ();
  $parser -> parse_from_filehandle(\*STDIN);

=head1 DESCRIPTION

Pod::Plainer uses Pod::Parser which takes Pod with the (new)
'CE<lt>E<lt> .. E<gt>E<gt>' constructs
and returns the old(er) style with just 'CE<lt>E<gt>';
'<' and '>' are replaced by 'EE<lt>ltE<gt>' and 'EE<lt>gtE<gt>'.

This can be used to pre-process Pod before using tools which do not
recognise the new style Pods.

=head2 METHODS

=over 

=item escape_ltgt

Replace '<' and '>' by 'EE<lt>ltE<gt>' and 'EE<lt>gtE<gt>'.

=item simple_delimiters

Replace delimiters by 'E<lt>' and 'E<gt>'.

=item textblock

Redefine C<textblock> from L<Pod::Parser> to use C<escape_ltgt>
and C<simple_delimiters>.

=back

=head2 EXPORT

None by default.

=head1 AUTHOR

Robin Barker, rmbarker@cpan.org

=head1 SEE ALSO

See L<Pod::Parser>.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009 by Robin Barker

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.10.1 or,
at your option, any later version of Perl 5 you may have available.

=cut

$Id: Plainer.pm 253 2010-02-11 16:28:10Z rmb1 $
