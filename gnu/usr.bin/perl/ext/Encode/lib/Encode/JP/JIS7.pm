package Encode::JP::JIS7;
use strict;

our $VERSION = do { my @r = (q$Revision: 1.8 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };

use Encode qw(:fallbacks);

for my $name ('7bit-jis', 'iso-2022-jp', 'iso-2022-jp-1'){
    my $h2z     = ($name eq '7bit-jis')    ? 0 : 1;
    my $jis0212 = ($name eq 'iso-2022-jp') ? 0 : 1;

    $Encode::Encoding{$name} =
        bless {
               Name      =>   $name,
               h2z       =>   $h2z,
               jis0212   =>   $jis0212,
              } => __PACKAGE__;
}

use base qw(Encode::Encoding);

# we override this to 1 so PerlIO works
sub needs_lines { 1 }

use Encode::CJKConstants qw(:all);

our $DEBUG = 0;

#
# decode is identical for all 2022 variants
#

sub decode($$;$)
{
    my ($obj, $str, $chk) = @_;
    my $residue = '';
    if ($chk){
	$str =~ s/([^\x00-\x7f].*)$//so;
	$1 and $residue = $1;
    }
    $residue .= jis_euc(\$str);
    $_[1] = $residue if $chk;
    return Encode::decode('euc-jp', $str, FB_PERLQQ);
}

#
# encode is different
#

sub encode($$;$)
{
    require Encode::JP::H2Z;
    my ($obj, $utf8, $chk) = @_;
    # empty the input string in the stack so perlio is ok
    $_[1] = '' if $chk;
    my ($h2z, $jis0212) = @$obj{qw(h2z jis0212)};
    my $octet = Encode::encode('euc-jp', $utf8, FB_PERLQQ) ;
    $h2z and &Encode::JP::H2Z::h2z(\$octet);
    euc_jis(\$octet, $jis0212);
    return $octet;
}


# JIS<->EUC

sub jis_euc {
    my $r_str = shift;
    $$r_str =~ s(
		 ($RE{JIS_0212}|$RE{JIS_0208}|$RE{ISO_ASC}|$RE{JIS_KANA})
		 ([^\e]*)
		 )
    {
	my ($esc, $chunk) = ($1, $2);
	if ($esc !~ /$RE{ISO_ASC}/o) {
	    $chunk =~ tr/\x21-\x7e/\xa1-\xfe/;
	    if ($esc =~ /$RE{JIS_KANA}/o) {
		$chunk =~ s/([\xa1-\xdf])/\x8e$1/og;
	    }
	    elsif ($esc =~ /$RE{JIS_0212}/o) {
		$chunk =~ s/([\xa1-\xfe][\xa1-\xfe])/\x8f$1/og;
	    }
	}
	$chunk;
    }geox;
    my ($residue) = ($$r_str =~ s/(\e.*)$//so);
    return $residue;
}

sub euc_jis{
    no warnings qw(uninitialized);
    my $r_str = shift;
    my $jis0212 = shift;
    $$r_str =~ s{
	((?:$RE{EUC_C})+|(?:$RE{EUC_KANA})+|(?:$RE{EUC_0212})+)
	}{
	    my $chunk = $1;
	    my $esc =
		( $chunk =~ tr/\x8E//d ) ? $ESC{KANA} :
		    ( $chunk =~ tr/\x8F//d ) ? $ESC{JIS_0212} :
			$ESC{JIS_0208};
	    if ($esc eq $ESC{JIS_0212} && !$jis0212){
		# fallback to '?'
		$chunk =~ tr/\xA1-\xFE/\x3F/;
	    }else{
		$chunk =~ tr/\xA1-\xFE/\x21-\x7E/;
	    }
	    $esc . $chunk . $ESC{ASC};
	}geox;
    $$r_str =~
	s/\Q$ESC{ASC}\E
	    (\Q$ESC{KANA}\E|\Q$ESC{JIS_0212}\E|\Q$ESC{JIS_0208}\E)/$1/gox;
    $$r_str;
}

1;
__END__


=head1 NAME

Encode::JP::JIS7 -- internally used by Encode::JP

=cut
