#! /bin/sed -nf

# $OpenBSD: sierpinski.sed,v 1.1 2008/10/10 14:33:34 millert Exp $
# From http://sed.sourceforge.net/grabbag/scripts
# Public Domain

# Sierpinski triangle in 10 commands + 2 labels.
# Start with a line like this
# _______________________________X_____________________________

# Put an equal number of underscores on both sides.
s/^\(_*\).*/\1X\1/p

# Construct the last three lines of the triangle
:start
/^X/!s/_X_/X_X/gp
/^X/!s/_X_X_/X___X/gp
/^X/!s/_X___X_/X_X_X_X/gp
/^X/ d

# Now replace the consecutive X's with an X and many colons
# X_X_X_X --->
# X::::::
:loop
s/\(X:*\)_X/\1::/g
tloop

# And now create two new "seeds", one to the left and one to the right
# _X::::::_ --->
# X:::::::X --->
# X_______X

s/_X\(::*\)_/X:\1X/g
s/:/_/gp
bstart
