package Test::Harness::Iterator;

use strict;
use vars qw($VERSION);
$VERSION = 0.01;


=head1 NAME

Test::Harness::Iterator - Internal Test::Harness Iterator

=head1 SYNOPSIS

  use Test::Harness::Iterator;
  use Test::Harness::Iterator;
  my $it = Test::Harness::Iterator->new(\*TEST);
  my $it = Test::Harness::Iterator->new(\@array);

  my $line = $it->next;


=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

This is a simple iterator wrapper for arrays and filehandles.

=cut

sub new {
    my($proto, $thing) = @_;

    my $self = {};
    if( ref $thing eq 'GLOB' ) {
        bless $self, 'Test::Harness::Iterator::FH';
        $self->{fh} = $thing;
    }
    elsif( ref $thing eq 'ARRAY' ) {
        bless $self, 'Test::Harness::Iterator::ARRAY';
        $self->{idx}   = 0;
        $self->{array} = $thing;
    }
    else {
        warn "Can't iterate with a ", ref $thing;
    }

    return $self;
}

package Test::Harness::Iterator::FH;
sub next {
    my $fh = $_[0]->{fh};
    return scalar <$fh>;
}


package Test::Harness::Iterator::ARRAY;
sub next {
    my $self = shift;
    return $self->{array}->[$self->{idx}++];
}
