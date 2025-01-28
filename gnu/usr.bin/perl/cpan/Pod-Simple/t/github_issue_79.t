use strict;
use warnings;

use Test::More;

{
package DumpAsXML::Enh;

use Pod::Simple::DumpAsXML ();
our @ISA = qw(Pod::Simple::DumpAsXML);

sub new {
    my ( $class ) = @_;
    my $self = $class->SUPER::new();
    $self->code_handler( sub { pop( @_ )->_handle_line( 'code', @_ ); } );
    $self->cut_handler( sub { pop( @_ )->_handle_line( 'cut', @_ ); } );
    $self->pod_handler( sub { pop( @_ )->_handle_line( 'pod', @_ ); } );
    $self->whiteline_handler( sub { pop( @_ )->_handle_line( 'white', @_ ); } );
    return $self;
};

sub _handle_line {
    my ( $self, $elem, $text, $line ) = @_;
    my $fh = $self->{ output_fh };
    print { $fh } '  ' x $self->{ indent }, "<$elem start_line=\"$line\"/>\n";
};

}

my $output = '';
my $parser = DumpAsXML::Enh->new();
$parser->output_string( \$output );

my $input = [
    '=head1 DESCRIPTION',
    '',
    '    Verbatim paragraph.',
    '',
    '=cut',
];
my $expected_output = join "\n",
    '<Document start_line="1">',
    '  <head1 start_line="1">',
    '    DESCRIPTION',
    '  </head1>',
    '  <VerbatimFormatted start_line="3" xml:space="preserve">',
    '        Verbatim paragraph.',
    '  </VerbatimFormatted>',
    '  <cut start_line="5"/>',
    '</Document>',
    '',
;

$parser->parse_lines( @$input, undef );

is($output, $expected_output);

done_testing;
