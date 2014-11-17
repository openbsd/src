#---------------------------------------------------------------------
package Tie::CPHash;
#
# Copyright 1997 Christopher J. Madsen
#
# Author: Christopher J. Madsen <cjm@pobox.com>
# Created: 08 Nov 1997
# $Revision$  $Date$
#
# This program is free software; you can redistribute it and/or modify
# it under the same terms as Perl itself.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either the
# GNU General Public License or the Artistic License for more details.
#
# Case preserving but case insensitive hash
#---------------------------------------------------------------------

require 5.000;
use strict;
use warnings;
use vars qw(@ISA $VERSION);

@ISA = qw();

#=====================================================================
# Package Global Variables:

$VERSION = '1.02';

#=====================================================================
# Tied Methods:
#---------------------------------------------------------------------
# TIEHASH classname
#      The method invoked by the command `tie %hash, classname'.
#      Associates a new hash instance with the specified class.

sub TIEHASH
{
    bless {}, $_[0];
} # end TIEHASH

#---------------------------------------------------------------------
# STORE this, key, value
#      Store datum *value* into *key* for the tied hash *this*.

sub STORE
{
    $_[0]->{lc $_[1]} = [ $_[1], $_[2] ];
} # end STORE

#---------------------------------------------------------------------
# FETCH this, key
#      Retrieve the datum in *key* for the tied hash *this*.

sub FETCH
{
    my $v = $_[0]->{lc $_[1]};
    ($v ? $v->[1] : undef);
} # end FETCH

#---------------------------------------------------------------------
# FIRSTKEY this
#      Return the (key, value) pair for the first key in the hash.

sub FIRSTKEY
{
    my $a = scalar keys %{$_[0]};
    &NEXTKEY;
} # end FIRSTKEY

#---------------------------------------------------------------------
# NEXTKEY this, lastkey
#      Return the next (key, value) pair for the hash.

sub NEXTKEY
{
    my $v = (each %{$_[0]})[1];
    ($v ? $v->[0] : undef );
} # end NEXTKEY

#---------------------------------------------------------------------
# SCALAR this
#     Return bucket usage information for the hash (0 if empty).

sub SCALAR
{
    scalar %{$_[0]};
} # end SCALAR

#---------------------------------------------------------------------
# EXISTS this, key
#     Verify that *key* exists with the tied hash *this*.

sub EXISTS
{
    exists $_[0]->{lc $_[1]};
} # end EXISTS

#---------------------------------------------------------------------
# DELETE this, key
#     Delete the key *key* from the tied hash *this*.
#     Returns the old value, or undef if it didn't exist.

sub DELETE
{
    my $v = delete $_[0]->{lc $_[1]};
    ($v ? $v->[1] : undef);
} # end DELETE

#---------------------------------------------------------------------
# CLEAR this
#     Clear all values from the tied hash *this*.

sub CLEAR
{
    %{$_[0]} = ();
} # end CLEAR

#=====================================================================
# Other Methods:
#---------------------------------------------------------------------
# Return the case of KEY.

sub key
{
    my $v = $_[0]->{lc $_[1]};
    ($v ? $v->[0] : undef);
}

#=====================================================================
# Package Return Value:

1;

__END__

=head1 NAME

Tie::CPHash - Case preserving but case insensitive hash table

=head1 SYNOPSIS

    require Tie::CPHash;
    tie %cphash, 'Tie::CPHash';

    $cphash{'Hello World'} = 'Hi there!';
    printf("The key `%s' was used to store `%s'.\n",
           tied(%cphash)->key('HELLO WORLD'),
           $cphash{'HELLO world'});

=head1 DESCRIPTION

The B<Tie::CPHash> module provides a hash table that is case
preserving but case insensitive.  This means that

    $cphash{KEY}    $cphash{key}
    $cphash{Key}    $cphash{keY}

all refer to the same entry.  Also, the hash remembers which form of
the key was last used to store the entry.  The C<keys> and C<each>
functions will return the key that was used to set the value.

An example should make this clear:

    tie %h, 'Tie::CPHash';
    $h{Hello} = 'World';
    print $h{HELLO};            # Prints 'World'
    print keys(%h);             # Prints 'Hello'
    $h{HELLO} = 'WORLD';
    print $h{hello};            # Prints 'WORLD'
    print keys(%h);             # Prints 'HELLO'

The additional C<key> method lets you fetch the case of a specific key:

    # When run after the previous example, this prints 'HELLO':
    print tied(%h)->key('Hello');

(The C<tied> function returns the object that C<%h> is tied to.)

If you need a case insensitive hash, but don't need to preserve case,
just use C<$hash{lc $key}> instead of C<$hash{$key}>.  This has a lot
less overhead than B<Tie::CPHash>.

=head1 AUTHOR

Christopher J. Madsen E<lt>F<cjm@pobox.com>E<gt>

=cut

# Local Variables:
# tmtrack-file-task: "Tie::CPHash.pm"
# End:
