#
# NetPacket::OFP - Decode and encode OpenFlow packets. 
#

package NetPacket::OFP;

#
# Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>.
#
# This package is free software and is provided "as is" without express 
# or implied warranty.  It may be used, redistributed and/or modified 
# under the terms of the Perl Artistic License (see
# http://www.perl.com/perl/misc/Artistic.html)
#

use strict;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
use NetPacket;

my $myclass;
BEGIN {
	$myclass = __PACKAGE__;
	$VERSION = "0.01";
}
sub Version () { "$myclass v$VERSION" }

BEGIN {
	@ISA = qw(Exporter NetPacket);

	@EXPORT = qw();

	@EXPORT_OK = qw(ofp_strip);

	%EXPORT_TAGS = (
		ALL         => [@EXPORT, @EXPORT_OK],
		strip       => [qw(ofp_strip)],
	);
}

#
# Decode the packet
#

sub decode {
	my $class = shift;
	my($pkt, $parent, @rest) = @_;
	my $self = {};

	# Class fields
	$self->{_parent} = $parent;
	$self->{_frame} = $pkt;

	$self->{version} = 1;
	$self->{type} = 0;
	$self->{length} = 0;
	$self->{xid} = 0;
	$self->{data} = '';

	# Decode OpenFlow packet
	if (defined($pkt)) {
		($self->{version}, $self->{type}, $self->{length},
		    $self->{xid}, $self->{data}) = unpack("CCnNa*", $pkt);
	}

	# Return a blessed object
	bless($self, $class);

	return ($self);
}

#
# Strip header from packet and return the data contained in it
#

undef &udp_strip;
*ofp_strip = \&strip;

sub strip {
	my ($pkt, @rest) = @_;
	my $ofp_obj = decode($pkt);
	return ($ofp_obj->data);
}   

#
# Encode a packet
#

sub encode {
	my $class = shift;
	my $self = shift;
	my $pkt = '';

	$self->{length} = 8;

	$pkt = pack("CCnN", $self->{version}, $self->{type},
	    $self->{length}, $self->{xid});

	if ($self->{version} == 1) {
		# PACKET_IN
		if ($self->{type} == 10) {
			$self->{length} += 10 + length($self->{data});
			$pkt = pack("CCnNNnnCCa*",
			    $self->{version}, $self->{type},
			    $self->{length}, $self->{xid}, $self->{buffer_id},
			    length($self->{data}),
			    $self->{port}, 0, 0, $self->{data});
		}
	}

	return ($pkt); 
}

#
# Module initialisation
#

1;

# autoloaded methods go after the END token (&& pod) below

__END__

=head1 NAME

C<NetPacket::OFP> - Assemble and disassemble OpenFlow packets.

=head1 SYNOPSIS

  use NetPacket::OFP;

  $ofp_obj = NetPacket::OFP->decode($raw_pkt);
  $ofp_pkt = NetPacket::OFP->encode($ofp_obj);
  $ofp_data = NetPacket::OFP::strip($raw_pkt);

=head1 DESCRIPTION

C<NetPacket::OFP> provides a set of routines for assembling and
disassembling packets using OpenFlow.

=head2 Methods

=over

=item C<NetPacket::OFP-E<gt>decode([RAW PACKET])>

Decode the raw packet data given and return an object containing
instance data.  This method will quite happily decode garbage input.
It is the responsibility of the programmer to ensure valid packet data
is passed to this method.

=item C<NetPacket::OFP-E<gt>encode($ofp_obj)>

Return a OFP packet encoded with the instance data specified.

=back

=head2 Functions

=over

=item C<NetPacket::OFP::strip([RAW PACKET])>

Return the encapsulated data (or payload) contained in the OpenFlow
packet.  This data is suitable to be used as input for other
C<NetPacket::*> modules.

This function is equivalent to creating an object using the
C<decode()> constructor and returning the C<data> field of that
object.

=back

=head2 Instance data

The instance data for the C<NetPacket::OFP> object consists of
the following fields.

=over

=item version

The OpenFlow version.

=item type

The message type.

=item length

The total message length.

=item xid

The transaction Id.

=item data

The encapsulated data (payload) for this packet.

=back

=head2 Exports

=over

=item default

none

=item exportable

ofp_strip

=item tags

The following tags group together related exportable items.

=over

=item C<:strip>

Import the strip function C<ofp_strip>.

=item C<:ALL>

All the above exportable items.

=back

=back

=head1 COPYRIGHT

  Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>

  This package is free software and is provided "as is" without express 
  or implied warranty.  It may be used, redistributed and/or modified 
  under the terms of the Perl Artistic License (see
  http://www.perl.com/perl/misc/Artistic.html)

=head1 AUTHOR

Reyk Floeter E<lt>reyk@openbsd.orgE<gt>

=cut

# any real autoloaded methods go after this line
