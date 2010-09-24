package Cname;
our $Evil='A';

sub translator {
    my $str = shift;
    if ( $str eq 'EVIL' ) {
        # Returns A first time, AB second, ABC third ... A-ZA the 27th time.
        (my $c=substr("A".$Evil,-1))++;
        my $r=$Evil;
        $Evil.=$c;
        return $r;
    }
    if ( $str eq 'EMPTY-STR') {
       return "";
    }
    if ( $str eq 'NULL') {
        return "\0";
    }
    if ( $str eq 'LONG-STR') {
        return 'A' x 255;
    }
    # Should exceed limit for regex \N bytes in a sequence.  Anyway it will if
    # UCHAR_MAX is 255.
    if ( $str eq 'TOO-LONG-STR') {
       return 'A' x 256;
    }
    if ($str eq 'MALFORMED') {
        $str = "\xDF\xDFabc";
        utf8::upgrade($str);
         
        # Create a malformed in first and second characters.
        $str =~ s/^\C/A/;
        $str =~ s/^(\C\C)\C/$1A/;
    }
    return $str;
}

sub import {
    shift;
    $^H{charnames} = \&translator;
}
1;  
