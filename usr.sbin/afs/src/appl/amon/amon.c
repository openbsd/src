/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stdio.h> 
#include <X11/Intrinsic.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/XawInit.h>
#include <X11/Xaw/StripCharP.h>
#include <X11/Xmu/Converters.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>

#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/StripChart.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xmu/SysUtil.h>

#include <parse_bytes.h>

#include "appl_locl.h"

RCSID("$arla: amon.c,v 1.12 2003/01/17 03:23:32 lha Exp $");

#if 0
static XrmOptionDescRec options[] = {};
#endif

int debug = 0;
XtAppContext app_con;

#if 0
/*
 * Create a widget for doing the same thing as stripChartWidgetClass
 * but add two lines
 */

#ifndef XtNmaxScale
#define XtNmaxScale "XtNmaxScale"
#endif

/*
 * Class
 */
typedef struct {int dummy;} StripChartMinMaxClassPart;

typedef struct _StripChartMinMaxClassRec {
    CoreClassPart core_class;
    SimpleClassPart simple_class;
    StripChartClassPart strip_chart_class;
    StripChartMinMaxClassPart strip_minmax_class;
} StripChartMinMaxClassRec;

/*
 * Instance
 */

typedef struct {
    GC	sGC;		

    int min_value;
    int max_value;
} StripChartMinMaxRec_local;

typedef struct _StripChartMinMaxRec {
    CorePart core;
    SimplePart simple;
    StripChartPart strip_chart;
    StripChartMinMaxRec_local strip_minmax;
} StripChartMinMaxRec;

typedef struct _StripChartMinMaxRec *StripChartMinMaxWidget;


#define offset(field) XtOffsetOf(StripChartMinMaxRec, field)

static XtResource resources[] = {
    {XtNminScale, XtCScale, XtRInt, sizeof(int),
        offset(strip_minmax.min_value), XtRImmediate, (XtPointer) 0},
    {XtNmaxScale, XtCScale, XtRInt, sizeof(int),
        offset(strip_minmax.min_value), XtRImmediate, (XtPointer) 0}
};


/*
 * Prototypes
 */

static void Initialize (Widget greq, Widget gnew, ArgList args, 
			Cardinal *num_args);
static void Destroy (Widget widget);
static void SetPoints(Widget widget);
static void Redisplay(Widget w, XEvent *event, Region region);
static Boolean SetValues (Widget current, Widget request, Widget new, 
			  ArgList args, Cardinal *num_args);


/*
 * The class struct
 */


StripChartMinMaxClassRec stripChartMinMaxClassRec = {
    { /* core fields */
    /* superclass		*/	(WidgetClass) &stripChartClassRec,
    /* class_name		*/	"StripChartMinMax",
    /* size			*/	sizeof(StripChartMinMaxRec),
    /* class_initialize		*/	XawInitializeWidgetSet,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	XtExposeCompressMultiple |
	                                XtExposeGraphicsExposeMerged,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	SetPoints,
    /* expose			*/	Redisplay,
    /* set_values		*/	SetValues,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	NULL,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
    },
    { /* Simple class fields */
    /* change_sensitive		*/	XtInheritChangeSensitive
    }
};

WidgetClass stripChartMinMaxWidgetClass = 
     (WidgetClass) &stripChartMinMaxClassRec;


/*
 *
 *
 */

static void 
Initialize (Widget greq, Widget gnew, 
	    ArgList args, Cardinal *num_args)
{
    printf ("initialize\n");
#if 0
    (stripChartMinMaxClassRec.core_class.superclass->core_class.initialize) (greq, gnew, args, num_args);
#endif
}

static void 
Destroy (Widget widget)
{
    printf ("destroy\n");
#if 0
    (stripChartMinMaxClassRec.core_class.superclass->core_class.destroy) (widget);
#endif
}

static void
SetPoints(Widget widget)
{
    printf ("resize\n");
#if 0
    (stripChartMinMaxClassRec.core_class.superclass->core_class.resize) (widget);
#endif
}

static void 
Redisplay(Widget w, XEvent *event, Region region)
{
    printf ("expose\n");
#if 0
    (stripChartMinMaxClassRec.core_class.superclass->core_class.expose) (w, event, region);
#endif
}

static Boolean 
SetValues (Widget current, Widget request, Widget new, 
	   ArgList args, Cardinal *num_args)
{
    printf ("set_values\n");
#if 0
    return (stripChartMinMaxClassRec.core_class.superclass->core_class.set_values) (current, request, new, args, num_args);
#endif
}

#endif

static void
SetTitleOfLabel (Widget w_label, String string)
{
    Arg title_args[1];

    XtSetArg (title_args[0], XtNlabel, string);
    XtSetValues (w_label, title_args, ONE);
}

static void
SetNotPaned (Widget w)
{
    Arg paned_args[1];

    XtSetArg (paned_args[0], XtNshowGrip, FALSE);
    XtSetValues (w, paned_args, ONE);
}


/*
 * The program
 */

static void 
GetUsedBytes(Widget w, XtPointer closure, XtPointer call_data)
{
    int64_t max_bytes;
    int64_t used_bytes;
    static char str[100];
    char ub[100], mb[100];

    Widget label = (Widget) closure;

    double *bytesavg = (double *)call_data;
    int err;
	
    err = fs_getfilecachestats (&max_bytes, &used_bytes, NULL, 
				NULL, NULL, NULL);

    if (err) {
	warnx ("bytes: fs_getfilecachestats returned %d", err);
	*bytesavg = 1.0;
	return;
    }

    if (max_bytes == 0) {
	*bytesavg = 1.0;
	warnx ("bytes: will not divide with zero (used: %ld)", 
	       (long)used_bytes);
	return;
    }
	
    *bytesavg = (float) used_bytes / max_bytes;

    if (debug)
	warnx ("kbytes: max: %ld used: %ld usage: %f", 
	       (long)max_bytes, (long)used_bytes, *bytesavg);

    ub[0] = mb[0] = '\0';
    unparse_bytes_short ((long)used_bytes, ub, sizeof(ub));
    unparse_bytes_short ((long)max_bytes, mb, sizeof(mb));

    snprintf (str, sizeof(str), "(%s/%s)", ub, mb);
    SetTitleOfLabel (label, str);
}

static void 
GetUsedVnodes(Widget w, XtPointer closure, XtPointer call_data)
{
    int64_t max_vnodes;
    int64_t used_vnodes;
    static char str[100];

    Widget label = (Widget) closure;

    double *vnodeavg = (double *)call_data;
    int err;
	

    err = fs_getfilecachestats (NULL, NULL, NULL,
				&max_vnodes, &used_vnodes, NULL);

    if (err) {
	*vnodeavg = 1.0;
	warnx ("vnodes: fs_getfilecachestats returned %d", err);
	return;
    }

    if (max_vnodes == 0) {
	*vnodeavg = 1.0;
	warnx ("vnodes: will not divide with zero (used: %ld)",
	       (long)used_vnodes);
	return;
    }
	
    *vnodeavg = (float) used_vnodes / max_vnodes;

    if (debug)
	warnx ("vnode: max: %ld used: %ld usage: %f", 
	       (long)max_vnodes, (long)used_vnodes, *vnodeavg);

    snprintf (str, sizeof(str), "vnodes# (%ld/%ld)", 
	      (long)used_vnodes, (long)max_vnodes);
    SetTitleOfLabel (label, str);
}

#ifdef VIOC_AVIATOR
static void 
GetUsedWorkers(Widget w, XtPointer closure, XtPointer call_data)
{
    uint32_t max_workers;
    uint32_t used_workers;
    static char str[100];

    Widget label = (Widget) closure;
    
    double *workeravg = (double *)call_data;
    int err;
    
    err = fs_getaviatorstats (&max_workers, &used_workers);

    if (err) {
	*workeravg = 1.0;
	warnx ("workers: fs_getfilecachestats returned %d", err);
	return;
    }

    if (max_workers == 0) {
	*workeravg = 1.0;
	warnx ("workers: will not divide with zero (used: %d)", used_workers);
	return;
    }
	
    *workeravg = (float) used_workers / max_workers;

    if (debug)
	warnx ("workers: max: %d used: %d usage: %f", 
	       max_workers, used_workers, *workeravg);

    snprintf (str, sizeof(str), "workers (%d/%d)", used_workers, max_workers);
    SetTitleOfLabel (label, str);
}
#endif

static void
CreateMonitorBar (Widget frame, XtCallbackProc proc, String name)
{
    Widget box, bar, label;
    Arg	pretty_args[] = {
    	{XtNborderWidth, (XtArgVal)0},
    };

    box = XtCreateManagedWidget ("box", panedWidgetClass,
				 frame, (ArgList) NULL, ZERO);

    label = XtCreateManagedWidget ("label", labelWidgetClass, 
				   box, pretty_args,
				   XtNumber(pretty_args));
    
    SetTitleOfLabel (label, name);
    SetNotPaned (label);

    bar = XtCreateManagedWidget ("bar", stripChartWidgetClass,
				 box, NULL, ZERO);
    XtAddCallback (bar, XtNgetValue, proc, label);
    SetNotPaned (bar);
}



/*
 * Actions
 */

static void
quit(Widget widget, XEvent *event, String *params, Cardinal *num_parms)
{
    XtDestroyApplicationContext(app_con);
    exit(0);
}


/*
 *
 */

int main (int argc, char **argv)
{
    Widget top, frame;

    XtActionsRec Actions[] = {
	{ "quit",	quit}
	};

    if (debug)
	warnx ("has afs");

    if (!k_hasafs())
	errx (1, "no afs");

    if (debug)
	warnx ("init Xt");

    top = XtAppInitialize(&app_con, "amon", NULL, ZERO,
			  /* options, XtNumber(options), */
			  &argc, argv, NULL, NULL, (Cardinal) 0);

    if (argc != 1) {
	print_version(NULL);
	errx (1, "usage");
    }

    if (debug)
	warnx ("creating windows");

    XtAppAddActions (app_con, Actions, XtNumber(Actions));
    

    frame = XtCreateManagedWidget ("frame", panedWidgetClass,
				  top, (ArgList) NULL, ZERO);

    XtOverrideTranslations(frame, 
			   XtParseTranslationTable("<Key>q:quit()\n"));

    /* XXX */
    CreateMonitorBar (frame, GetUsedBytes,"          bytes (0/0)          ");
    CreateMonitorBar (frame, GetUsedVnodes, "vnode# (0/0)");
#ifdef VIOC_AVIATOR
    CreateMonitorBar (frame, GetUsedWorkers, "workers# (0/0)");
#endif

    XtRealizeWidget (top);

    if (debug)
	warnx ("X-loop");

    XtAppMainLoop(app_con);
    return 0;
}
