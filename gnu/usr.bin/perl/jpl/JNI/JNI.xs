/*
 * Copyright 1997, O'Reilly & Associate, Inc.
 *
 * This package may be copied under the same terms as Perl itself.
 */

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include <stdio.h>
#include <jni.h>

#ifndef PERL_VERSION
#  include <patchlevel.h>
#  define PERL_REVISION		5
#  define PERL_VERSION		PATCHLEVEL
#  define PERL_SUBVERSION	SUBVERSION
#endif

#if PERL_REVISION == 5 && (PERL_VERSION < 4 || (PERL_VERSION == 4 && PERL_SUBVERSION <= 75))
#  define PL_na		na
#  define PL_sv_no	sv_no
#  define PL_sv_undef	sv_undef
#  define PL_dowarn	dowarn
#endif

#ifndef newSVpvn
#  define newSVpvn(a,b)	newSVpv(a,b)
#endif

#ifndef pTHX
#  define pTHX		void
#  define pTHX_
#  define aTHX
#  define aTHX_
#  define dTHX		extern int JNI___notused
#endif

#ifndef WIN32
#  include <dlfcn.h>
#endif

#ifdef EMBEDDEDPERL
extern JNIEnv* jplcurenv;
extern int jpldebug;
#else
JNIEnv* jplcurenv;
int jpldebug = 1;
#endif

#define SysRet jint

#ifdef WIN32
static void JNICALL call_my_exit(jint status)
{
    my_exit(status);
}
#else
static void call_my_exit(jint status)
{
    my_exit(status);
}
#endif

jvalue*
makeargs(char *sig, SV** svp, int items)
{
    jvalue* jv = (jvalue*)safemalloc(sizeof(jvalue) * items);
    int ix = 0;
    char *s = sig;
    JNIEnv* env = jplcurenv;
    char *start;
    STRLEN n_a;

    if (jpldebug)
	fprintf(stderr, "sig = %s, items = %d\n", sig, items);
    if (*s++ != '(')
	goto cleanup;

    while (items--) {
	SV *sv = *svp++;
	start = s;
	switch (*s++) {
	case 'Z':
	    jv[ix++].z = (jboolean)(SvIV(sv) != 0);
	    break;
	case 'B':
	    jv[ix++].b = (jbyte)SvIV(sv);
	    break;
	case 'C':
	    jv[ix++].c = (jchar)SvIV(sv);
	    break;
	case 'S':
	    jv[ix++].s = (jshort)SvIV(sv);
	    break;
	case 'I':
	    jv[ix++].i = (jint)SvIV(sv);
	    break;
	case 'J':
	    jv[ix++].j = (jlong)SvNV(sv);
	    break;
	case 'F':
	    jv[ix++].f = (jfloat)SvNV(sv);
	    break;
	case 'D':
	    jv[ix++].d = (jdouble)SvNV(sv);
	    break;
	case '[':
	    switch (*s++) {
	    case 'Z':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jboolean* buf = (jboolean*)malloc(len * sizeof(jboolean));
			int i;
			SV** esv;

			jbooleanArray ja = (*env)->NewBooleanArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jboolean)SvIV(*esv);
			(*env)->SetBooleanArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jboolean);

		    jbooleanArray ja = (*env)->NewBooleanArray(env, len);
		    (*env)->SetBooleanArrayRegion(env, ja, 0, len, (jboolean*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'B':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jbyte* buf = (jbyte*)malloc(len * sizeof(jbyte));
			int i;
			SV** esv;

			jbyteArray ja = (*env)->NewByteArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jbyte)SvIV(*esv);
			(*env)->SetByteArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jbyte);

		    jbyteArray ja = (*env)->NewByteArray(env, len);
		    (*env)->SetByteArrayRegion(env, ja, 0, len, (jbyte*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'C':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jchar* buf = (jchar*)malloc(len * sizeof(jchar));
			int i;
			SV** esv;

			jcharArray ja = (*env)->NewCharArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jchar)SvIV(*esv);
			(*env)->SetCharArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jchar);

		    jcharArray ja = (*env)->NewCharArray(env, len);
		    (*env)->SetCharArrayRegion(env, ja, 0, len, (jchar*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'S':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jshort* buf = (jshort*)malloc(len * sizeof(jshort));
			int i;
			SV** esv;

			jshortArray ja = (*env)->NewShortArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jshort)SvIV(*esv);
			(*env)->SetShortArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jshort);

		    jshortArray ja = (*env)->NewShortArray(env, len);
		    (*env)->SetShortArrayRegion(env, ja, 0, len, (jshort*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'I':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jint* buf = (jint*)malloc(len * sizeof(jint));
			int i;
			SV** esv;

			jintArray ja = (*env)->NewIntArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jint)SvIV(*esv);
			(*env)->SetIntArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jint);

		    jintArray ja = (*env)->NewIntArray(env, len);
		    (*env)->SetIntArrayRegion(env, ja, 0, len, (jint*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'J':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jlong* buf = (jlong*)malloc(len * sizeof(jlong));
			int i;
			SV** esv;

			jlongArray ja = (*env)->NewLongArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jlong)SvNV(*esv);
			(*env)->SetLongArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jlong);

		    jlongArray ja = (*env)->NewLongArray(env, len);
		    (*env)->SetLongArrayRegion(env, ja, 0, len, (jlong*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'F':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jfloat* buf = (jfloat*)malloc(len * sizeof(jfloat));
			int i;
			SV** esv;

			jfloatArray ja = (*env)->NewFloatArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jfloat)SvNV(*esv);
			(*env)->SetFloatArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jfloat);

		    jfloatArray ja = (*env)->NewFloatArray(env, len);
		    (*env)->SetFloatArrayRegion(env, ja, 0, len, (jfloat*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'D':
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			jdouble* buf = (jdouble*)malloc(len * sizeof(jdouble));
			int i;
			SV** esv;

			jdoubleArray ja = (*env)->NewDoubleArray(env, len);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jdouble)SvNV(*esv);
			(*env)->SetDoubleArrayRegion(env, ja, 0, len, buf);
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jdouble);

		    jdoubleArray ja = (*env)->NewDoubleArray(env, len);
		    (*env)->SetDoubleArrayRegion(env, ja, 0, len, (jdouble*)SvPV(sv,n_a));
		    jv[ix++].l = (jobject)ja;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    case 'L':
		while (*s != ';') s++;
		s++;
		if (strnEQ(start, "[Ljava/lang/String;", 19)) {
		    if (SvROK(sv)) {
			SV* rv = (SV*)SvRV(sv);
			if (SvOBJECT(rv))
			    jv[ix++].l = (jobject)(void*)SvIV(rv);
			else if (SvTYPE(rv) == SVt_PVAV) {
			    jsize len = av_len((AV*)rv) + 1;
			    int i;
			    SV** esv;
			    static jclass jcl = 0;
			    jobjectArray ja;

			    if (!jcl)
				jcl = (*env)->FindClass(env, "java/lang/String");
			    ja = (*env)->NewObjectArray(env, len, jcl, 0);
			    for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
				jobject str = (jobject)(*env)->NewStringUTF(env, SvPV(*esv,n_a));
				(*env)->SetObjectArrayElement(env, ja, i, str);
			    }
			    jv[ix++].l = (jobject)ja;
			}
			else
			    jv[ix++].l = (jobject)(void*)0;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		    break;
		}
		/* FALL THROUGH */
	    default:
		if (SvROK(sv)) {
		    SV* rv = (SV*)SvRV(sv);
		    if (SvOBJECT(rv))
			jv[ix++].l = (jobject)(void*)SvIV(rv);
		    else if (SvTYPE(rv) == SVt_PVAV) {
			jsize len = av_len((AV*)rv) + 1;
			int i;
			SV** esv;
		       static jclass jcl = 0;
			jobjectArray ja;

			if (!jcl)
			    jcl = (*env)->FindClass(env, "java/lang/Object");
			ja = (*env)->NewObjectArray(env, len, jcl, 0);
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
			    if (SvROK(*esv) && (rv = SvRV(*esv)) && SvOBJECT(rv)) {
				(*env)->SetObjectArrayElement(env, ja, i, (jobject)(void*)SvIV(rv));
			    }
			    else {
				jobject str = (jobject)(*env)->NewStringUTF(env, SvPV(*esv,n_a));
				(*env)->SetObjectArrayElement(env, ja, i, str);
			    }
			}
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else
		    jv[ix++].l = (jobject)(void*)0;
		break;
	    }
	    break;
	case 'L':
	    if (!SvROK(sv) || strnEQ(s, "java/lang/String;", 17)) {
		s += 17;
		jv[ix++].l = (jobject)(*env)->NewStringUTF(env, (char*) SvPV(sv,n_a));
		break;
	    }
	    while (*s != ';') s++;
	    s++;
	    if (SvROK(sv)) {
		SV* rv = SvRV(sv);
		jv[ix++].l = (jobject)(void*)SvIV(rv);
	    }
	    break;
	case ')':
	    croak("too many arguments, signature: %s", sig);
	    goto cleanup;
	default:
	    croak("panic: malformed signature: %s", s-1);
	    goto cleanup;
	}

    }
    if (*s != ')') {
	croak("not enough arguments, signature: %s", sig);
	goto cleanup;
    }
    return jv;

cleanup:
    safefree((char*)jv);
    return 0;
}

static int
not_here(char *s)
{
    croak("%s not implemented on this architecture", s);
    return -1;
}

static double
constant(char *name, int arg)
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	break;
    case 'C':
	break;
    case 'D':
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	break;
    case 'I':
	break;
    case 'J':
	if (strEQ(name, "JNI_ABORT"))
#ifdef JNI_ABORT
	    return JNI_ABORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_COMMIT"))
#ifdef JNI_COMMIT
	    return JNI_COMMIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_ERR"))
#ifdef JNI_ERR
	    return JNI_ERR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_FALSE"))
#ifdef JNI_FALSE
	    return JNI_FALSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_H"))
#ifdef JNI_H
#ifdef WIN32
	    return 1;
#else
	    return JNI_H;
#endif
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_OK"))
#ifdef JNI_OK
	    return JNI_OK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "JNI_TRUE"))
#ifdef JNI_TRUE
	    return JNI_TRUE;
#else
	    goto not_there;
#endif
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

#define FETCHENV jplcurenv
#define RESTOREENV jplcurenv = env

MODULE = JNI		PACKAGE = JNI		

PROTOTYPES: ENABLE

double
constant(name,arg)
	char *		name
	int		arg

jint
GetVersion()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
	    RETVAL = (*env)->GetVersion(env);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jclass
DefineClass(name, loader, buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jsize			buf_len_ = NO_INIT;
	const char *		name
	jobject			loader
	const jbyte *		buf
    CODE:
	{
#ifdef KAFFE
	    RETVAL = (*env)->DefineClass(env,  loader, buf, (jsize)buf_len_);
#else
	    RETVAL = (*env)->DefineClass(env,  name, loader, buf, (jsize)buf_len_); 
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jclass
FindClass(name)
	JNIEnv *		env = FETCHENV;
	const char *		name
    CODE:
	{
	    RETVAL = (*env)->FindClass(env,  name);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jclass
GetSuperclass(sub)
	JNIEnv *		env = FETCHENV;
	jclass			sub
    CODE:
	{
	    RETVAL = (*env)->GetSuperclass(env,  sub);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
IsAssignableFrom(sub, sup)
	JNIEnv *		env = FETCHENV;
	jclass			sub
	jclass			sup
    CODE:
	{
	    RETVAL = (*env)->IsAssignableFrom(env,  sub, sup);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

SysRet
Throw(obj)
	JNIEnv *		env = FETCHENV;
	jthrowable		obj
    CODE:
	{
	    RETVAL = (*env)->Throw(env,  obj);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL    

SysRet
ThrowNew(clazz, msg)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	const char *		msg
    CODE:
	{
	    RETVAL = (*env)->ThrowNew(env,  clazz, msg);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jthrowable
ExceptionOccurred()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
	    RETVAL = (*env)->ExceptionOccurred(env);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
ExceptionDescribe()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
	    (*env)->ExceptionDescribe(env);
	    RESTOREENV;
	}

void
ExceptionClear()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
	    (*env)->ExceptionClear(env);
	    RESTOREENV;
	}

void
FatalError(msg)
	JNIEnv *		env = FETCHENV;
	const char *		msg
    CODE:
	{
	    (*env)->FatalError(env,  msg);
	    RESTOREENV;
	}

jobject
NewGlobalRef(lobj)
	JNIEnv *		env = FETCHENV;
	jobject			lobj
    CODE:
	{
	    RETVAL = (*env)->NewGlobalRef(env, lobj);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
DeleteGlobalRef(gref)
	JNIEnv *		env = FETCHENV;
	jobject			gref
    CODE:
	{
	    (*env)->DeleteGlobalRef(env, gref);
	    RESTOREENV;
	}

void
DeleteLocalRef(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
	    (*env)->DeleteLocalRef(env,  obj);
	    RESTOREENV;
	}

jboolean
IsSameObject(obj1,obj2)
	JNIEnv *		env = FETCHENV;
	jobject			obj1
	jobject			obj2
    CODE:
	{
	    RETVAL = (*env)->IsSameObject(env, obj1,obj2);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
AllocObject(clazz)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
    CODE:
	{
	    RETVAL = (*env)->AllocObject(env, clazz);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
NewObject(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->NewObjectA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
NewObjectA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->NewObjectA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jclass
GetObjectClass(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
	    RETVAL = (*env)->GetObjectClass(env, obj);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
IsInstanceOf(obj,clazz)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
    CODE:
	{
	    RETVAL = (*env)->IsInstanceOf(env, obj,clazz);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jmethodID
GetMethodID(clazz,name,sig)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	const char *		name
	const char *		sig
    CODE:
	{
	    RETVAL = (*env)->GetMethodID(env, clazz,name,sig);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
CallObjectMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallObjectMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
CallObjectMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallObjectMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallBooleanMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallBooleanMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallBooleanMethodA(obj,methodID, args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallBooleanMethodA(env, obj,methodID, args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallByteMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallByteMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallByteMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallByteMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallCharMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallCharMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallCharMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallCharMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallShortMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallShortMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallShortMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallShortMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallIntMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallIntMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallIntMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallIntMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallLongMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallLongMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallLongMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallLongMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallFloatMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallFloatMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallFloatMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallFloatMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallDoubleMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallDoubleMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallDoubleMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallDoubleMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
CallVoidMethod(obj,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    (*env)->CallVoidMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}

void
CallVoidMethodA(obj,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    (*env)->CallVoidMethodA(env, obj,methodID,args);
	    RESTOREENV;
	}

jobject
CallNonvirtualObjectMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualObjectMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
CallNonvirtualObjectMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualObjectMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallNonvirtualBooleanMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualBooleanMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallNonvirtualBooleanMethodA(obj,clazz,methodID, args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualBooleanMethodA(env, obj,clazz,methodID, args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallNonvirtualByteMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualByteMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallNonvirtualByteMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualByteMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallNonvirtualCharMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualCharMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallNonvirtualCharMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualCharMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallNonvirtualShortMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualShortMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallNonvirtualShortMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualShortMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallNonvirtualIntMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualIntMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallNonvirtualIntMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualIntMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallNonvirtualLongMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualLongMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallNonvirtualLongMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualLongMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallNonvirtualFloatMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualFloatMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallNonvirtualFloatMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualFloatMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallNonvirtualDoubleMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallNonvirtualDoubleMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallNonvirtualDoubleMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallNonvirtualDoubleMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
CallNonvirtualVoidMethod(obj,clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    (*env)->CallNonvirtualVoidMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}

void
CallNonvirtualVoidMethodA(obj,clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    (*env)->CallNonvirtualVoidMethodA(env, obj,clazz,methodID,args);
	    RESTOREENV;
	}

jfieldID
GetFieldID(clazz,name,sig)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	const char *		name
	const char *		sig
    CODE:
	{
	    RETVAL = (*env)->GetFieldID(env, clazz,name,sig);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
GetObjectField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetObjectField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
GetBooleanField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetBooleanField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
GetByteField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetByteField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
GetCharField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetCharField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
GetShortField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetShortField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
GetIntField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetIntField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
GetLongField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetLongField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
GetFloatField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetFloatField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
GetDoubleField(obj,fieldID)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetDoubleField(env, obj,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
SetObjectField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jobject			val
    CODE:
	{
	    (*env)->SetObjectField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetBooleanField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jboolean		val
    CODE:
	{
	    (*env)->SetBooleanField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetByteField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jbyte			val
    CODE:
	{
	    (*env)->SetByteField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetCharField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jchar			val
    CODE:
	{
	    (*env)->SetCharField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetShortField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jshort			val
    CODE:
	{
	    (*env)->SetShortField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetIntField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jint			val
    CODE:
	{
	    (*env)->SetIntField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetLongField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jlong			val
    CODE:
	{
	    (*env)->SetLongField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetFloatField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jfloat			val
    CODE:
	{
	    (*env)->SetFloatField(env, obj,fieldID,val);
	    RESTOREENV;
	}

void
SetDoubleField(obj,fieldID,val)
	JNIEnv *		env = FETCHENV;
	jobject			obj
	jfieldID		fieldID
	char *			sig = 0;
	jdouble			val
    CODE:
	{
	    (*env)->SetDoubleField(env, obj,fieldID,val);
	    RESTOREENV;
	}

jmethodID
GetStaticMethodID(clazz,name,sig)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	const char *		name
	const char *		sig
    CODE:
	{
	    RETVAL = (*env)->GetStaticMethodID(env, clazz,name,sig);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
CallStaticObjectMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticObjectMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
CallStaticObjectMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticObjectMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallStaticBooleanMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticBooleanMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
CallStaticBooleanMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticBooleanMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallStaticByteMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticByteMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
CallStaticByteMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticByteMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallStaticCharMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticCharMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
CallStaticCharMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticCharMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallStaticShortMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticShortMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
CallStaticShortMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticShortMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallStaticIntMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticIntMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
CallStaticIntMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticIntMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallStaticLongMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticLongMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
CallStaticLongMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticLongMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallStaticFloatMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticFloatMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
CallStaticFloatMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticFloatMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallStaticDoubleMethod(clazz,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    RETVAL = (*env)->CallStaticDoubleMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
CallStaticDoubleMethodA(clazz,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    RETVAL = (*env)->CallStaticDoubleMethodA(env, clazz,methodID,args);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
CallStaticVoidMethod(cls,methodID,...)
	JNIEnv *		env = FETCHENV;
	jclass			cls
	jmethodID		methodID
	char *			sig = 0;
	int			argoff = $min_args;
    CODE:
	{
	    jvalue * args = makeargs(sig, &ST(argoff), items - argoff);
	    (*env)->CallStaticVoidMethodA(env, cls,methodID,args);
	    RESTOREENV;
	}

void
CallStaticVoidMethodA(cls,methodID,args)
	JNIEnv *		env = FETCHENV;
	jclass			cls
	jmethodID		methodID
	char *			sig = 0;
	jvalue *		args
    CODE:
	{
	    (*env)->CallStaticVoidMethodA(env, cls,methodID,args);
	    RESTOREENV;
	}

jfieldID
GetStaticFieldID(clazz,name,sig)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	const char *		name
	const char *		sig
    CODE:
	{
	    RETVAL = (*env)->GetStaticFieldID(env, clazz,name,sig);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
GetStaticObjectField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticObjectField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean
GetStaticBooleanField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticBooleanField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyte
GetStaticByteField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticByteField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jchar
GetStaticCharField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticCharField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshort
GetStaticShortField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticShortField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jint
GetStaticIntField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticIntField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlong
GetStaticLongField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticLongField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloat
GetStaticFloatField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticFloatField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdouble
GetStaticDoubleField(clazz,fieldID)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
    CODE:
	{
	    RETVAL = (*env)->GetStaticDoubleField(env, clazz,fieldID);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
SetStaticObjectField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jobject			value
    CODE:
	{
	  (*env)->SetStaticObjectField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticBooleanField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jboolean		value
    CODE:
	{
	  (*env)->SetStaticBooleanField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticByteField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jbyte			value
    CODE:
	{
	  (*env)->SetStaticByteField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticCharField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jchar			value
    CODE:
	{
	  (*env)->SetStaticCharField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticShortField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jshort			value
    CODE:
	{
	  (*env)->SetStaticShortField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticIntField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jint			value
    CODE:
	{
	  (*env)->SetStaticIntField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticLongField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jlong			value
    CODE:
	{
	  (*env)->SetStaticLongField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticFloatField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jfloat			value
    CODE:
	{
	  (*env)->SetStaticFloatField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

void
SetStaticDoubleField(clazz,fieldID,value)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	jfieldID		fieldID
	char *			sig = 0;
	jdouble			value
    CODE:
	{
	  (*env)->SetStaticDoubleField(env, clazz,fieldID,value);
	    RESTOREENV;
	}

jstring
NewString(unicode)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jsize			unicode_len_ = NO_INIT;
	const jchar *		unicode
    CODE:
	{
	    RETVAL = (*env)->NewString(env, unicode, unicode_len_);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jsize
GetStringLength(str)
	JNIEnv *		env = FETCHENV;
	jstring			str
    CODE:
	{
	    RETVAL = (*env)->GetStringLength(env, str);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

const jchar *
GetStringChars(str)
	JNIEnv *		env = FETCHENV;
	jstring			str
	jboolean		isCopy = NO_INIT;
	jsize 			RETVAL_len_ = NO_INIT;
    CODE:
	{
	    RETVAL = (*env)->GetStringChars(env, str,&isCopy);
	    RETVAL_len_ = (*env)->GetStringLength(env, str);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL
    CLEANUP:
	    (*env)->ReleaseStringChars(env, str,RETVAL);

jstring
NewStringUTF(utf)
	JNIEnv *		env = FETCHENV;
	const char *		utf
    CODE:
	{
	    RETVAL = (*env)->NewStringUTF(env, utf);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jsize
GetStringUTFLength(str)
	JNIEnv *		env = FETCHENV;
	jstring			str
    CODE:
	{
	    RETVAL = (*env)->GetStringUTFLength(env, str);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

const char *
GetStringUTFChars(str)
	JNIEnv *		env = FETCHENV;
	jstring			str
	jboolean		isCopy = NO_INIT;
    CODE:
	{
	    RETVAL = (*env)->GetStringUTFChars(env, str,&isCopy);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL
    CLEANUP:
	(*env)->ReleaseStringUTFChars(env, str, RETVAL);


jsize
GetArrayLength(array)
	JNIEnv *		env = FETCHENV;
	jarray			array
    CODE:
	{
	    RETVAL = (*env)->GetArrayLength(env, array);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobjectArray
NewObjectArray(len,clazz,init)
	JNIEnv *		env = FETCHENV;
	jsize			len
	jclass			clazz
	jobject			init
    CODE:
	{
	    RETVAL = (*env)->NewObjectArray(env, len,clazz,init);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jobject
GetObjectArrayElement(array,index)
	JNIEnv *		env = FETCHENV;
	jobjectArray		array
	jsize			index
    CODE:
	{
	    RETVAL = (*env)->GetObjectArrayElement(env, array,index);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
SetObjectArrayElement(array,index,val)
	JNIEnv *		env = FETCHENV;
	jobjectArray		array
	jsize			index
	jobject			val
    CODE:
	{
	    (*env)->SetObjectArrayElement(env, array,index,val);
	    RESTOREENV;
	}

jbooleanArray
NewBooleanArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewBooleanArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jbyteArray
NewByteArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewByteArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jcharArray
NewCharArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewCharArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jshortArray
NewShortArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewShortArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jintArray
NewIntArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewIntArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jlongArray
NewLongArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewLongArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jfloatArray
NewFloatArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewFloatArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jdoubleArray
NewDoubleArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
	    RETVAL = (*env)->NewDoubleArray(env, len);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jboolean *
GetBooleanArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jbooleanArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetBooleanArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jboolean* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jboolean))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseBooleanArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jbyte *
GetByteArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jbyteArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetByteArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jbyte* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jbyte))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseByteArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jchar *
GetCharArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jcharArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetCharArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jchar* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jchar))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseCharArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jshort *
GetShortArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jshortArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetShortArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jshort* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jshort))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseShortArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jint *
GetIntArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jintArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetIntArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jint* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jint))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseIntArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jlong *
GetLongArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jlongArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetLongArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jlong* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSViv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jlong))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseLongArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jfloat *
GetFloatArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jfloatArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetFloatArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jfloat* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSVnv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jfloat))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseFloatArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

jdouble *
GetDoubleArrayElements(array)
	JNIEnv *		env = FETCHENV;
	jsize			RETVAL_len_ = NO_INIT;
	jdoubleArray		array
	jboolean		isCopy = NO_INIT;
    PPCODE:
	{
	    RETVAL = (*env)->GetDoubleArrayElements(env, array,&isCopy);
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
	    if (GIMME == G_ARRAY) {
		int i;
		jdouble* r = RETVAL;
		EXTEND(sp, RETVAL_len_);
		for (i = RETVAL_len_; i; --i) {
		    PUSHs(sv_2mortal(newSVnv(*r++)));
		}
	    }
	    else {
		if (RETVAL_len_) {
		    PUSHs(sv_2mortal(newSVpvn((char*)RETVAL,
			(STRLEN)RETVAL_len_ * sizeof(jdouble))));
		}
		else
		    PUSHs(&PL_sv_no);
	    }
	    (*env)->ReleaseDoubleArrayElements(env, array,RETVAL,JNI_ABORT);
	    RESTOREENV;
	}

void
GetBooleanArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jbooleanArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jboolean *		buf = (jboolean*)sv_grow(ST(3),len * sizeof(jboolean)+1);
    CODE:
	{
	    (*env)->GetBooleanArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jboolean));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetByteArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jbyteArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jbyte *			buf = (jbyte*)sv_grow(ST(3),len * sizeof(jbyte)+1);
    CODE:
	{
	    (*env)->GetByteArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jbyte));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetCharArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jcharArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jchar *			buf = (jchar*)sv_grow(ST(3),len * sizeof(jchar)+1);
    CODE:
	{
	    (*env)->GetCharArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jchar));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetShortArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jshortArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jshort *		buf = (jshort*)sv_grow(ST(3),len * sizeof(jshort)+1);
    CODE:
	{
	    (*env)->GetShortArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jshort));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetIntArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jintArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jint *			buf = (jint*)sv_grow(ST(3),len * sizeof(jint)+1);
    CODE:
	{
	    (*env)->GetIntArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jint));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetLongArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jlongArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jlong *			buf = (jlong*)sv_grow(ST(3),len * sizeof(jlong)+1);
    CODE:
	{
	    (*env)->GetLongArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jlong));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetFloatArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jfloatArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jfloat *		buf = (jfloat*)sv_grow(ST(3),len * sizeof(jfloat)+1);
    CODE:
	{
	    (*env)->GetFloatArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jfloat));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
GetDoubleArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	jdoubleArray		array
	jsize			start
	jsize			len
	STRLEN			tmplen = len * sizeof(jboolean) + 1;
	char *			tmpbuf = (char*)sv_pvn_force(ST(3), &tmplen);
	jdouble *		buf = (jdouble*)sv_grow(ST(3),len * sizeof(jdouble)+1);
    CODE:
	{
	    (*env)->GetDoubleArrayRegion(env, array,start,len,buf);
	    SvCUR_set(ST(3), len * sizeof(jdouble));
	    *SvEND(ST(3)) = '\0';
	    RESTOREENV;
	}

void
SetBooleanArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jbooleanArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jboolean *		buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetBooleanArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetByteArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jbyteArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jbyte *			buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetByteArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetCharArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jcharArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jchar *			buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetCharArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetShortArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jshortArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jshort *		buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetShortArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetIntArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jintArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jint *			buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetIntArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetLongArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jlongArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jlong *			buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetLongArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetFloatArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jfloatArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jfloat *		buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetFloatArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

void
SetDoubleArrayRegion(array,start,len,buf)
	JNIEnv *		env = FETCHENV;
	STRLEN			tmplen = NO_INIT;
	jdoubleArray		array
	jsize			start
	jsize			len
	jsize			buf_len_ = NO_INIT;
	jdouble *		buf
    CODE:
	{
	    if (buf_len_ < len)
		croak("string is too short");
	    else if (buf_len_ > len && PL_dowarn)
		warn("string is too long");
	    (*env)->SetDoubleArrayRegion(env, array,start,len,buf);
	    RESTOREENV;
	}

SysRet
RegisterNatives(clazz,methods,nMethods)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
	JNINativeMethod *	methods
	jint			nMethods
    CODE:
	{
	    RETVAL = (*env)->RegisterNatives(env, clazz,methods,nMethods);
	}

SysRet
UnregisterNatives(clazz)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
    CODE:
	{
	    RETVAL = (*env)->UnregisterNatives(env, clazz);
	}
    OUTPUT:
	RETVAL  
   
SysRet
MonitorEnter(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
	    RETVAL = (*env)->MonitorEnter(env, obj);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

SysRet
MonitorExit(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
	    RETVAL = (*env)->MonitorExit(env, obj);
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

JavaVM *
GetJavaVM(...)
	JNIEnv *		env = FETCHENV;
    CODE:
	{
#ifdef JPL_DEBUG
	    jpldebug = 1;
#else
	    jpldebug = 0;
#endif
	    if (env) {	/* We're embedded. */
		if ((*env)->GetJavaVM(env, &RETVAL) < 0)
		    RETVAL = 0;
	    }
	    else {	/* We're embedding. */
#ifdef KAFFE
                JavaVMInitArgs vm_args;
#else
                JDK1_1InitArgs vm_args;
#endif
		char *lib;
		if (jpldebug) {
		    fprintf(stderr, "We're embedding Java in Perl.\n");
		}

		if (items--) {
  		    ++mark;
		    lib = SvPV(*mark, PL_na);
		}
		else
		    lib = 0;
		if (jpldebug) {
		    fprintf(stderr, "lib is %s.\n", lib);
		}
#ifdef WIN32
        if (LoadLibrary("jvm.dll")) {
            if (!LoadLibrary("javai.dll")) {
                warn("Can't load javai.dll");
            }
        } else {
            if (lib && !LoadLibrary(lib))
                croak("Can't load javai.dll"); 
        }
#else
		if (jpldebug) {
		    fprintf(stderr, "Opening Java shared library.\n");
                }
#ifdef KAFFE
		if (!dlopen("libkaffevm.so", RTLD_LAZY|RTLD_GLOBAL)) {
#else
		if (!dlopen("libjava.so", RTLD_LAZY|RTLD_GLOBAL)) {
#endif
		    if (lib && !dlopen(lib, RTLD_LAZY|RTLD_GLOBAL))
			croak("Can't load Java shared library.");
		}
#endif
               /* Kaffe seems to get very upset if vm_args.version isn't set */
#ifdef KAFFE
		vm_args.version = JNI_VERSION_1_1;
#endif
		JNI_GetDefaultJavaVMInitArgs(&vm_args);
		vm_args.exit = &call_my_exit;
		if (jpldebug) {
            fprintf(stderr, "items = %d\n", items);
            fprintf(stderr, "mark = %s\n", SvPV(*mark, PL_na));
        }
		while (items > 1) {
		  char *s;
		    ++mark;
		    s = SvPV(*mark,PL_na);
		    ++mark;
		    if (jpldebug) {
                fprintf(stderr, "*s = %s\n", s);
                fprintf(stderr, "val = %s\n", SvPV(*mark, PL_na));
            }
		    items -= 2;
		    if (strEQ(s, "checkSource"))
			vm_args.checkSource = (jint)SvIV(*mark);
		    else if (strEQ(s, "nativeStackSize"))
			vm_args.nativeStackSize = (jint)SvIV(*mark);
		    else if (strEQ(s, "javaStackSize"))
			vm_args.javaStackSize = (jint)SvIV(*mark);
		    else if (strEQ(s, "minHeapSize"))
			vm_args.minHeapSize = (jint)SvIV(*mark);
		    else if (strEQ(s, "maxHeapSize"))
			vm_args.maxHeapSize = (jint)SvIV(*mark);
		    else if (strEQ(s, "verifyMode"))
			vm_args.verifyMode = (jint)SvIV(*mark);
		    else if (strEQ(s, "classpath"))
			vm_args.classpath = savepv(SvPV(*mark,PL_na));
		    else if (strEQ(s, "enableClassGC"))
			vm_args.enableClassGC = (jint)SvIV(*mark);
		    else if (strEQ(s, "enableVerboseGC"))
			vm_args.enableVerboseGC = (jint)SvIV(*mark);
		    else if (strEQ(s, "disableAsyncGC"))
			vm_args.disableAsyncGC = (jint)SvIV(*mark);
#ifdef KAFFE
		    else if (strEQ(s, "libraryhome"))
			vm_args.libraryhome = savepv(SvPV(*mark,PL_na));
		    else if (strEQ(s, "classhome"))
			vm_args.classhome = savepv(SvPV(*mark,PL_na));
		    else if (strEQ(s, "enableVerboseJIT"))
			vm_args.enableVerboseJIT = (jint)SvIV(*mark); 
		    else if (strEQ(s, "enableVerboseClassloading"))
			vm_args.enableVerboseClassloading = (jint)SvIV(*mark); 
		    else if (strEQ(s, "enableVerboseCall"))
			vm_args.enableVerboseCall = (jint)SvIV(*mark); 
		    else if (strEQ(s, "allocHeapSize"))
			vm_args.allocHeapSize = (jint)SvIV(*mark); 
#else
		    else if (strEQ(s, "verbose"))
			vm_args.verbose = (jint)SvIV(*mark); 
		    else if (strEQ(s, "debugging"))
			vm_args.debugging = (jboolean)SvIV(*mark);
		    else if (strEQ(s, "debugPort"))
			vm_args.debugPort = (jint)SvIV(*mark); 
#endif
		    else
			croak("unrecognized option: %s", s);
		}

		if (jpldebug) {
		    fprintf(stderr, "Creating Java VM...\n");
		    fprintf(stderr, "Working CLASSPATH: %s\n", 
			vm_args.classpath);
		}
		if (JNI_CreateJavaVM(&RETVAL, &jplcurenv, &vm_args) < 0) {
                  croak("Unable to create instance of JVM");
                }
		if (jpldebug) {
		    fprintf(stderr, "Created Java VM.\n");
		}

	    }
	}

