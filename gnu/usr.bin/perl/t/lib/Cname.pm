package Cname;
our $Evil='A';

sub translator {
    my $str = shift;
    if ( $str eq 'EVIL' ) {
        (my $c=substr("A".$Evil,-1))++;
        my $r=$Evil;
        $Evil.=$c;
        return $r;
    }
    if ( $str eq 'EMPTY-STR') {
       return "";
    }
    return $str;
}

sub import {
    shift;
    $^H{charnames} = \&translator;
}
1;  
