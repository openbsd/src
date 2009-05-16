package TieOut;
# $Id: TieOut.pm,v 1.1 2009/05/16 21:42:58 simon Exp $

sub TIEHANDLE {
    my $scalar = '';
    bless( \$scalar, $_[0] );
}

sub PRINT {
    my $self = shift;
    $$self .= join( '', @_ );
}

sub PRINTF {
    my $self = shift;
    my $fmt  = shift;
    $$self .= sprintf $fmt, @_;
}

sub FILENO { }

sub read {
    my $self = shift;
    my $data = $$self;
    $$self = '';
    return $data;
}

1;
