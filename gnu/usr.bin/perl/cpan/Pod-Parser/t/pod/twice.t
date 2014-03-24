use strict;
use Test;
use File::Spec;

BEGIN { plan tests => 1 }

use Pod::Parser;
use Carp;
$SIG{__DIE__} = \&Carp::confess;

eval {require IO::String;};
skip($@ ? 'no IO::String' : '', sub {
  {
    my $pod_string = 'some I<silly> text';
    my $handle = IO::String->new( \$pod_string );
    my $parser = Pod::Parser->new();
    $parser->parse_from_file( $0, $handle );
  }
  # free the reference
  {
    my $parser = Pod::Parser->new();
    $parser->parse_from_file( $0, File::Spec->devnull );
  }
  1;
});

exit 0;

__END__

=head1 EXAMPLE

This test makes sure the parse_from_file is re-entrant

=cut

