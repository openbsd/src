package BaseIncExtender;

BEGIN { ::ok( $INC[-1] ne '.', 'no trailing dot in @INC during module load from base' ) }

use lib 't/lib/blahblah';

1;
