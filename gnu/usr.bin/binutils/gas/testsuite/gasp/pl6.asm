	.ALTERNATE
! test of expression operator
define 	MACRO 	val, string
	SDATA 	val
	SDATA	string
	ENDM
	define "1","99%of100"	! notice % within string
	define 	%1 + 2, "=3"


	define 	% 1 + 2 %3+4

	define	%3*4-2  <=10>

	define	%3*4-2  5

	define	%1 + 2,<is equal to %1 + 2, right?>

	! has no effect

	end
