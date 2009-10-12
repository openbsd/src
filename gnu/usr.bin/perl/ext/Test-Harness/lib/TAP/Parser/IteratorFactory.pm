package TAP::Parser::IteratorFactory;

use strict;
use vars qw($VERSION @ISA);

use TAP::Object                    ();
use TAP::Parser::Iterator::Array   ();
use TAP::Parser::Iterator::Stream  ();
use TAP::Parser::Iterator::Process ();

@ISA = qw(TAP::Object);

=head1 NAME

TAP::Parser::IteratorFactory - Internal TAP::Parser Iterator

=head1 VERSION

Version 3.17

=cut

$VERSION = '3.17';

=head1 SYNOPSIS

  use TAP::Parser::IteratorFactory;
  my $factory = TAP::Parser::IteratorFactory->new;
  my $iter = $factory->make_iterator(\*TEST);
  my $iter = $factory->make_iterator(\@array);
  my $iter = $factory->make_iterator(\%hash);

  my $line = $iter->next;

=head1 DESCRIPTION

This is a factory class for simple iterator wrappers for arrays, filehandles,
and hashes.  Unless you're subclassing, you probably won't need to use this
module directly.

=head1 METHODS

=head2 Class Methods

=head3 C<new>

Creates a new factory class.
I<Note:> You currently don't need to instantiate a factory in order to use it.

=head3 C<make_iterator>

Create an iterator.  The type of iterator created depends on the arguments to
the constructor:

  my $iter = TAP::Parser::Iterator->make_iterator( $filehandle );

Creates a I<stream> iterator (see L</make_stream_iterator>).

  my $iter = TAP::Parser::Iterator->make_iterator( $array_reference );

Creates an I<array> iterator (see L</make_array_iterator>).

  my $iter = TAP::Parser::Iterator->make_iterator( $hash_reference );

Creates a I<process> iterator (see L</make_process_iterator>).

=cut

sub make_iterator {
    my ( $proto, $thing ) = @_;

    my $ref = ref $thing;
    if ( $ref eq 'GLOB' || $ref eq 'IO::Handle' ) {
        return $proto->make_stream_iterator($thing);
    }
    elsif ( $ref eq 'ARRAY' ) {
        return $proto->make_array_iterator($thing);
    }
    elsif ( $ref eq 'HASH' ) {
        return $proto->make_process_iterator($thing);
    }
    else {
        die "Can't iterate with a $ref";
    }
}

=head3 C<make_stream_iterator>

Make a new stream iterator and return it.  Passes through any arguments given.
Defaults to a L<TAP::Parser::Iterator::Stream>.

=head3 C<make_array_iterator>

Make a new array iterator and return it.  Passes through any arguments given.
Defaults to a L<TAP::Parser::Iterator::Array>.

=head3 C<make_process_iterator>

Make a new process iterator and return it.  Passes through any arguments given.
Defaults to a L<TAP::Parser::Iterator::Process>.

=cut

sub make_stream_iterator {
    my $proto = shift;
    TAP::Parser::Iterator::Stream->new(@_);
}

sub make_array_iterator {
    my $proto = shift;
    TAP::Parser::Iterator::Array->new(@_);
}

sub make_process_iterator {
    my $proto = shift;
    TAP::Parser::Iterator::Process->new(@_);
}

1;

=head1 SUBCLASSING

Please see L<TAP::Parser/SUBCLASSING> for a subclassing overview.

There are a few things to bear in mind when creating your own
C<ResultFactory>:

=over 4

=item 1

The factory itself is never instantiated (this I<may> change in the future).
This means that C<_initialize> is never called.

=back

=head2 Example

  package MyIteratorFactory;

  use strict;
  use vars '@ISA';

  use MyStreamIterator;
  use TAP::Parser::IteratorFactory;

  @ISA = qw( TAP::Parser::IteratorFactory );

  # override stream iterator
  sub make_stream_iterator {
    my $proto = shift;
    MyStreamIterator->new(@_);
  }

  1;

=head1 ATTRIBUTION

Originally ripped off from L<Test::Harness>.

=head1 SEE ALSO

L<TAP::Object>,
L<TAP::Parser>,
L<TAP::Parser::Iterator>,
L<TAP::Parser::Iterator::Array>,
L<TAP::Parser::Iterator::Stream>,
L<TAP::Parser::Iterator::Process>,

=cut

