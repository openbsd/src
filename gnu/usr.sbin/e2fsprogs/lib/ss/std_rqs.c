/* ./std_rqs.c - automatically generated from ./std_rqs.ct */
#include <ss/ss.h>

#ifndef __STDC__
#define const
#endif

static char const * const ssu00001[] = {
".",
    (char const *)0
};
extern void ss_self_identify __SS_PROTO;
static char const * const ssu00002[] = {
"help",
    (char const *)0
};
extern void ss_help __SS_PROTO;
static char const * const ssu00003[] = {
"list_help",
    "lh",
    (char const *)0
};
extern void ss_unimplemented __SS_PROTO;
static char const * const ssu00004[] = {
"list_requests",
    "lr",
    "?",
    (char const *)0
};
extern void ss_list_requests __SS_PROTO;
static char const * const ssu00005[] = {
"quit",
    "q",
    (char const *)0
};
extern void ss_quit __SS_PROTO;
static char const * const ssu00006[] = {
"abbrev",
    "ab",
    (char const *)0
};
extern void ss_unimplemented __SS_PROTO;
static char const * const ssu00007[] = {
"execute",
    "e",
    (char const *)0
};
extern void ss_unimplemented __SS_PROTO;
static char const * const ssu00008[] = {
"?",
    (char const *)0
};
extern void ss_unimplemented __SS_PROTO;
static char const * const ssu00009[] = {
"subsystem_name",
    (char const *)0
};
extern void ss_subsystem_name __SS_PROTO;
static char const * const ssu00010[] = {
"subsystem_version",
    (char const *)0
};
extern void ss_subsystem_version __SS_PROTO;
static ss_request_entry ssu00011[] = {
    { ssu00001,
      ss_self_identify,
      "Identify the subsystem.",
      3 },
    { ssu00002,
      ss_help,
      "Display info on command or topic.",
      0 },
    { ssu00003,
      ss_unimplemented,
      "List topics for which help is available.",
      3 },
    { ssu00004,
      ss_list_requests,
      "List available commands.",
      0 },
    { ssu00005,
      ss_quit,
      "Leave the subsystem.",
      0 },
    { ssu00006,
      ss_unimplemented,
      "Enable/disable abbreviation processing of request lines.",
      3 },
    { ssu00007,
      ss_unimplemented,
      "Execute a UNIX command line.",
      3 },
    { ssu00008,
      ss_unimplemented,
      "Produce a list of the most commonly used requests.",
      3 },
    { ssu00009,
      ss_subsystem_name,
      "Return the name of this subsystem.",
      1 },
    { ssu00010,
      ss_subsystem_version,
      "Return the version of this subsystem.",
      1 },
    { 0, 0, 0, 0 }
};

ss_request_table ss_std_requests = { 2, ssu00011 };
