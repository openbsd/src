dnl $OpenBSD: redef.m4,v 1.1 2001/09/19 19:15:08 espie Exp $
define(`mydefine',defn(`define'))dnl
mydefine(`mydefn',defn(`defn'))dnl
mydefine(`myundefine',mydefn(`undefine'))dnl
myundefine(`defn')dnl
myundefine(`define')dnl
myundefine(`undefine')dnl
mydefine(`mydef2',mydefn(`mydefine'))dnl
mydefine(`mydef', mydefn(`define'))dnl
myundefine(`mydefine')dnl
mydef2(`A',`B')dnl
mydef(`C',`D')dnl
A C
