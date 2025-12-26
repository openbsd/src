#
#  Copyright (c) 1995-2000, Raphael Manfredi
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

package STDump;
use strict;
use warnings;
use Carp;
use Exporter;
*import = \&Exporter::import;

our @EXPORT = qw(stdump);

my %dump = (
    'SCALAR'        => \&dump_scalar,
    'LVALUE'        => \&dump_scalar,
    'ARRAY'         => \&dump_array,
    'HASH'          => \&dump_hash,
    'REF'           => \&dump_ref,
);

# Given an object, dump its transitive data closure
sub stdump {
    my ($object) = @_;
    croak "Not a reference!" unless ref($object);
    my $ctx = {
        dumped => {},
        object => {},
        count => 0,
        dump => '',
    };
    recursive_dump($object, 1, $ctx);
    return $ctx->{dump};
}

# This is the root recursive dumping routine that may indirectly be
# called by one of the routine it calls...
# The link parameter is set to false when the reference passed to
# the routine is an internal temporary variable, implying the object's
# address is not to be dumped in the %dumped table since it's not a
# user-visible object.
sub recursive_dump {
    my ($object, $link, $ctx) = @_;

    # Get something like SCALAR(0x...) or TYPE=SCALAR(0x...).
    # Then extract the bless, ref and address parts of that string.

    my $what = "$object";               # Stringify
    my ($bless, $ref, $addr) = $what =~ /^(\w+)=(\w+)\((0x.*)\)$/;
    ($ref, $addr) = $what =~ /^(\w+)\((0x.*)\)$/ unless $bless;

    # Special case for references to references. When stringified,
    # they appear as being scalars. However, ref() correctly pinpoints
    # them as being references indirections. And that's it.

    $ref = 'REF' if ref($object) eq 'REF';

    # Make sure the object has not been already dumped before.
    # We don't want to duplicate data. Retrieval will know how to
    # relink from the previously seen object.

    if ($link && $ctx->{dumped}{$addr}++) {
        my $num = $ctx->{object}{$addr};
        $ctx->{dump} .= "OBJECT #$num seen\n";
        return;
    }

    my $objcount = $ctx->{count}++;
    $ctx->{object}{$addr} = $objcount;

    # Call the appropriate dumping routine based on the reference type.
    # If the referenced was blessed, we bless it once the object is dumped.
    # The retrieval code will perform the same on the last object retrieved.

    croak "Unknown simple type '$ref'" unless defined $dump{$ref};

    $dump{$ref}->($object, $ctx);    # Dump object
    $ctx->{dump} .= "BLESS $bless\n" if $bless;  # Mark it as blessed, if necessary

    $ctx->{dump} .= "OBJECT $objcount\n";
}

# Dump single scalar
sub dump_scalar {
    my ($sref, $ctx) = @_;
    my $scalar = $$sref;
    unless (defined $scalar) {
        $ctx->{dump} .= "UNDEF\n";
        return;
    }
    my $len = length($scalar);
    $ctx->{dump} .= "SCALAR len=$len $scalar\n";
}

# Dump array
sub dump_array {
    my ($aref, $ctx) = @_;
    my $items = 0 + @{$aref};
    $ctx->{dump} .= "ARRAY items=$items\n";
    foreach my $item (@{$aref}) {
        unless (defined $item) {
            $ctx->{dump} .= 'ITEM_UNDEF' . "\n";
            next;
        }
        $ctx->{dump} .= 'ITEM ';
        recursive_dump(\$item, 1, $ctx);
    }
}

# Dump hash table
sub dump_hash {
    my ($href, $ctx) = @_;
    my $items = scalar(keys %{$href});
    $ctx->{dump} .= "HASH items=$items\n";
    foreach my $key (sort keys %{$href}) {
        $ctx->{dump} .= 'KEY ';
        recursive_dump(\$key, undef, $ctx);
        unless (defined $href->{$key}) {
            $ctx->{dump} .= 'VALUE_UNDEF' . "\n";
            next;
        }
        $ctx->{dump} .= 'VALUE ';
        recursive_dump(\$href->{$key}, 1, $ctx);
    }
}

# Dump reference to reference
sub dump_ref {
    my ($rref, $ctx) = @_;
    my $deref = $$rref;                 # Follow reference to reference
    $ctx->{dump} .= 'REF ';
    recursive_dump($deref, 1, $ctx);         # $dref is a reference
}

1;
