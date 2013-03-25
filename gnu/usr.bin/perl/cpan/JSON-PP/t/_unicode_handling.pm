#package utf8;
package _unicode_handling;

# this is a dummy pragma for 5.005.

    if ($] < 5.006) {
        $INC{'utf8.pm'} = './utf8.pm';

        eval q|
            sub utf8::import {  }
            sub utf8::unimport {  }
        |;

        $JSON::PP::can_handle_UTF16_and_utf8 = 0;
    }
    else {
        $JSON::PP::can_handle_UTF16_and_utf8 = 1;

        if ($] > 5.007 and $] < 5.008003) {
#            $JSON::can_handle_UTF16_and_utf8 = 0;
        }

    }




1;
