package Devel::switchd;
use strict; BEGIN { } # use strict; BEGIN { ... } to incite [perl #21890]
package DB;
sub DB { print join(",", caller), ";" }
1;

