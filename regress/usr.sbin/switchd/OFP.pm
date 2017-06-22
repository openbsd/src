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
# Get aligned packet size
#

sub ofp_align {
	my $matchlen = shift;

	return (($matchlen + (8 - 1)) & (~(8 - 1)));
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
	my $datalen = length($self->{data});

	if ($self->{length} == 0) {
		$self->{length} = 8 + $datalen;
	}

	if ($datalen == 0) {
		$pkt = pack('CCnN', $self->{version}, $self->{type},
		    $self->{length}, $self->{xid});
	} else {
		$pkt = pack('CCnNa*', $self->{version}, $self->{type},
		    $self->{length}, $self->{xid}, $self->{data});
	}

	return ($pkt);
}

#
# Table property Multipart reply
#

sub ofp_table_property {
	my $tp_type = shift;
	my $tp_payload = shift;
	my $tp_header;
	my ($datalen, $pad);

	# len = header + payload
	$datalen = 4 + length($tp_payload);

	$tp_header = pack('nna*',
	    $tp_type,			# type
	    $datalen,			# length
	    $tp_payload			# payload
	    );

	$pad = ofp_align($datalen) - $datalen;
	if ($pad > 0) {
		$tp_header .= pack("x[$pad]");
	}

	return ($tp_header);
}

sub ofp_table_features_reply {
	my $class = shift;
	my $self = shift;
	my $pkt = NetPacket::OFP->decode() or fatal($class, "new packet");
	my ($tf_header, $tf_payload);
	my ($tp_header, $tp_payload);
	my $mp_header;

	$tf_payload = '';

	#
	# instructions
	#
	$tp_payload = '';
	for (my $inst = main::OFP_INSTRUCTION_T_GOTO_TABLE();
	    $inst <= main::OFP_INSTRUCTION_T_METER(); $inst++) {
		$tp_payload .= pack('nn',
		    $inst,			# type
		    4				# length
		    );
	}

	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_INSTRUCTION(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_INSTRUCTION_MISS(),
		$tp_payload);

	#
	# Next tables
	#
	$tp_payload = '';

	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_NEXT_TABLES(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_NEXT_TABLES_MISS(),
		$tp_payload);

	#
	# Write / Apply actions
	#
	$tp_payload = pack('nn',
	    main::OFP_ACTION_OUTPUT(),		# type
	    4,					# length
	    );
	$tp_payload .= pack('nn',
	    main::OFP_ACTION_PUSH_VLAN(),	# type
	    4,					# length
	    );
	$tp_payload .= pack('nn',
	    main::OFP_ACTION_POP_VLAN(),	# type
	    4,					# length
	    );

	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_WRITE_ACTIONS(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_WRITE_ACTIONS_MISS(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_APPLY_ACTIONS(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_APPLY_ACTIONS_MISS(),
		$tp_payload);

	#
	# Match/Wildcards/Write set-field/Apply set-field
	#
	$tp_payload = pack('nCC',
	    main::OFP_OXM_C_OPENFLOW_BASIC(),	# class
	    main::OFP_XM_T_IN_PORT() << 1,	# type
	    4,					# length
	    );
	$tp_payload .= pack('nCC',
	    main::OFP_OXM_C_OPENFLOW_BASIC(),	# class
	    main::OFP_XM_T_ETH_TYPE() << 1,	# type
	    4,					# length
	    );
	$tp_payload .= pack('nCC',
	    main::OFP_OXM_C_OPENFLOW_BASIC(),	# class
	    main::OFP_XM_T_ETH_SRC() << 1,	# type
	    4,					# length
	    );
	$tp_payload .= pack('nCC',
	    main::OFP_OXM_C_OPENFLOW_BASIC(),	# class
	    main::OFP_XM_T_ETH_DST() << 1,	# type
	    4,					# length
	    );
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_MATCH(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_WILDCARDS(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_WRITE_SETFIELD(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_WRITE_SETFIELD_MISS(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_APPLY_SETFIELD(),
		$tp_payload);
	$tf_payload .=
	    ofp_table_property(main::OFP_TABLE_FEATPROP_APPLY_SETFIELD_MISS(),
		$tp_payload);

	#
	# Finish
	#
	$tf_header = pack('nCx[5]a[32]NNNNNN',
	    64 + length($tf_payload),	# length
	    0,				# tableid
	    'start',			# name
	    0x00000000, 0x00000000,	# metadata_match
	    0x00000000, 0x00000000,	# metadata_write
	    0x00000000,			# config
	    10000			# max_entries
	    );
	$tf_header .= $tf_payload;
	# XXX everything fits a single multipart reply, for now.

	$mp_header = pack('nnx[4]',
	    main::OFP_MP_T_TABLE_FEATURES(),	# multipart type
	    0					# multipart flags
	    );

	$mp_header .= $tf_header;

	$pkt->{version} = $self->{version};
	$pkt->{type} = main::OFP_T_MULTIPART_REPLY();
	$pkt->{xid} = $self->{xid}++;
	$pkt->{data} = $mp_header;
	$pkt = NetPacket::OFP->encode($pkt);

	main::ofp_output($self, $pkt);

	# Wait for new flow-mod
	main::ofp_input($self);
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
