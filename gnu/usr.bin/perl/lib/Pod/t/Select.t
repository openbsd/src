#!perl
use warnings;
use strict;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
};

use Test::More tests => 9;

require_ok( 'Pod::Select' );

my $fake_out = tie *FAKEOUT, 'CatchOut';

my $p_s = Pod::Select->new;
isa_ok( $p_s, 'Pod::Select' );

my $pod = << 'EO_NAME';
=head1 NAME

Select.t - Tests for Pod::Select.

EO_NAME

$p_s->select( 'NAME' );
$p_s->parse_from_file( $0, \*FAKEOUT );
is( $$fake_out, $pod, 'select( NAME )' );

$pod .= << 'EO_SYNOPSIS';
=head1 SYNOPSIS

This program just tests the basics of the Pod::Select module.

EO_SYNOPSIS

$$fake_out = '';
$p_s->select( 'NAME', 'SYNOPSIS' );
$p_s->parse_from_file( $0, \*FAKEOUT );
is( $$fake_out, $pod, 'select( NAME, SYNOPSIS )' );

$pod .= << 'EO_AUTHOR';
=head1 AUTHOR

Abe Timmerman <abe@ztreet.demon.nl>

EO_AUTHOR

$$fake_out = '';
$p_s->add_selection( 'AUTHOR' );
$p_s->parse_from_file( $0, \*FAKEOUT );
is( $$fake_out, $pod, 'add_selection( AUTHOR )' );

my $head1 = $p_s->curr_headings(1);
is( $head1, 'AUTHOR', 'curr_headings()' );

$pod = << 'EO_DESCRIPTION';
=head2 subsection

a sub-section can be specified

EO_DESCRIPTION

$$fake_out = '';
$p_s->select( 'DESCRIPTION/subsection' );
$p_s->parse_from_file( $0, \*FAKEOUT );
is( $$fake_out, $pod, 'select( DESCRIPTION/subsection )' );


ok( $p_s->match_section( 'DESCRIPTION', 'subsection' ), 
    'match_section( DESCRIPTION, subsection )' );

$pod = << 'EO_DESCRIPTION';
=head1 DESCRIPTION

I'll go by the POD in Pod::Select.

EO_DESCRIPTION

$$fake_out = '';
$p_s->select( 'DESCRIPTION/!.+' );
$p_s->parse_from_file( $0, \*FAKEOUT );
is( $$fake_out, $pod, 'select( DESCRIPTION/!.+ )' );


package CatchOut;
sub TIEHANDLE { bless \( my $self ), shift }
sub PRINT     { my $self = shift; $$self .= $_[0] }

__END__

=head1 NAME

Select.t - Tests for Pod::Select.

=head1 SYNOPSIS

This program just tests the basics of the Pod::Select module.

=head1 DESCRIPTION

I'll go by the POD in Pod::Select.

=head2 selection + add_selection

Pull out the specified sections

=head2 subsection

a sub-section can be specified

=head1 AUTHOR

Abe Timmerman <abe@ztreet.demon.nl>

=cut
