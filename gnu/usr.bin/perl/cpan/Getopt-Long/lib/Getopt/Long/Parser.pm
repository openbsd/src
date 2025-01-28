#! perl

# Parser.pm -- Getopt::Long object oriented interface
# Author          : Johan Vromans
# Created On      : Thu Nov  9 10:37:00 2023
# Last Modified On: Sat Nov 11 17:48:49 2023
# Update Count    : 13
# Status          : Released

package Getopt::Long::Parser;

our $VERSION = 2.57;

=head1 NAME

Getopt::Long::Parser - Getopt::Long object oriented interface

=head1 SYNOPSIS

    use Getopt::Long::Parser;
    my $p = Getopt::Long::Parser->new;
    $p->configure( ...configuration options... );
    if ( $p->getoptions( ...options descriptions... ) ) ...
    if ( $p->getoptionsfromarray( \@array, ...options descriptions... ) ) ...

Configuration options can be passed to the constructor:

    my $p = Getopt::Long::Parser->new
             config => [...configuration options...];

=head1 DESCRIPTION

Getopt::Long::Parser is an object oriented interface to
L<Getopt::Long>. See its documentation for configuration and use.

Note that Getopt::Long and Getopt::Long::Parser are not object
oriented. Getopt::Long::Parser emulates an object oriented interface,
which should be okay for most purposes.

=head1 CONSTRUCTOR

    my $p = Getopt::Long::Parser->new( %options );

The constructor takes an optional hash with parameters.

=over 4

=item config

An array reference with configuration settings.
See L<Getopt::Long/"Configuring Getopt::Long"> for all possible settings.

=back

=cut

# Getopt::Long has a stub for Getopt::Long::Parser::new.
use Getopt::Long ();
no warnings 'redefine';

sub new {
    my $that = shift;
    my $class = ref($that) || $that;
    my %atts = @_;

    # Register the callers package.
    my $self = { caller_pkg => (caller)[0] };

    bless ($self, $class);

    my $default_config = Getopt::Long::_default_config();

    # Process config attributes.
    if ( defined $atts{config} ) {
	my $save = Getopt::Long::Configure ($default_config, @{$atts{config}});
	$self->{settings} = Getopt::Long::Configure ($save);
	delete ($atts{config});
    }
    # Else use default config.
    else {
	$self->{settings} = $default_config;
    }

    if ( %atts ) {		# Oops
	die(__PACKAGE__.": unhandled attributes: ".
	    join(" ", sort(keys(%atts)))."\n");
    }

    $self;
}

use warnings 'redefine';

=head1 METHODS

In the examples, $p is assumed to be the result of a call to the constructor.

=head2 configure

    $p->configure( %settings );

Update the current config settings.
See L<Getopt::Long/"Configuring Getopt::Long"> for all possible settings.

=cut

sub configure {
    my ($self) = shift;

    # Restore settings, merge new settings in.
    my $save = Getopt::Long::Configure ($self->{settings}, @_);

    # Restore orig config and save the new config.
    $self->{settings} = Getopt::Long::Configure ($save);
}

=head2 getoptionsfromarray

    $res = $p->getoptionsfromarray( $aref, @opts );

=head2 getoptions

    $res = $p->getoptions( @opts );

The same as getoptionsfromarray( \@ARGV, @opts ).

=cut

sub getoptions {
    my ($self) = shift;

    return $self->getoptionsfromarray(\@ARGV, @_);
}

sub getoptionsfromarray {
    my ($self) = shift;

    # Restore config settings.
    my $save = Getopt::Long::Configure ($self->{settings});

    # Call main routine.
    my $ret = 0;
    $Getopt::Long::caller = $self->{caller_pkg};

    eval {
	# Locally set exception handler to default, otherwise it will
	# be called implicitly here, and again explicitly when we try
	# to deliver the messages.
	local ($SIG{__DIE__}) = 'DEFAULT';
	$ret = Getopt::Long::GetOptionsFromArray (@_);
    };

    # Restore saved settings.
    Getopt::Long::Configure ($save);

    # Handle errors and return value.
    die ($@) if $@;
    return $ret;
}

=head1 SEE ALSO

L<Getopt::Long>

=head1 AUTHOR

Johan Vromans <jvromans@squirrel.nl>

=head1 COPYRIGHT AND DISCLAIMER

This program is Copyright 1990,2015,2023 by Johan Vromans.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl.

=cut

1;
