/*
>Number:         4975
>Category:       chill
>Synopsis:       Segmentation fault of gdb 4.12.1
>Description:

    Problem: gdb 4.12.1 segment faults with following chill program.
*/

gdb_bug: MODULE

<> USE_SEIZE_FILE "pr-4975-grt.grt" <>
SEIZE is_channel_type;

  SYNMODE chan_type = POWERSET is_channel_type;
  SYN     hugo chan_type = [chan_1, chan_3];

DCL otto is_channel_type := chan_2;

x: PROC ();

  IF otto IN hugo THEN
    WRITETEXT (STDOUT, "otto IN hugo%/");
  ELSE
    WRITETEXT (STDOUT, "You loose%/");
  FI;
END x;

x ();

END gdb_bug;
/*
Compiled with:

   chill -S -fgrant-only pr-315-grt.ch
   chill -g -o pr-315 pr-315.ch

Run gdb with

   gdb pr-315 --readnow

will result in a sigsegv in file gdbtypes.c function force_to_range_type.
*/
