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

#if PERL_REVISION == 5 && (PERL_VERSION < 4 || \
			   (PERL_VERSION == 4 && PERL_SUBVERSION <= 75))
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

#ifdef WIN32
			jbooleanArray ja = env->NewBooleanArray(len);
#else
			jbooleanArray ja = (*env)->NewBooleanArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jboolean)SvIV(*esv);
#ifdef WIN32
			env->SetBooleanArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetBooleanArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jboolean);

#ifdef WIN32
		    jbooleanArray ja = env->NewBooleanArray(len);
#else
		    jbooleanArray ja = (*env)->NewBooleanArray(env, len);
#endif
#ifdef WIN32
		    env->SetBooleanArrayRegion(ja, 0, len, (jboolean*)SvPV(sv,n_a));
#else
		    (*env)->SetBooleanArrayRegion(env, ja, 0, len, (jboolean*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jbyteArray ja = env->NewByteArray(len);
#else
			jbyteArray ja = (*env)->NewByteArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jbyte)SvIV(*esv);
#ifdef WIN32
			env->SetByteArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetByteArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jbyte);

#ifdef WIN32
		    jbyteArray ja = env->NewByteArray(len);
#else
		    jbyteArray ja = (*env)->NewByteArray(env, len);
#endif
#ifdef WIN32
		    env->SetByteArrayRegion(ja, 0, len, (jbyte*)SvPV(sv,n_a));
#else
		    (*env)->SetByteArrayRegion(env, ja, 0, len, (jbyte*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jcharArray ja = env->NewCharArray(len);
#else
			jcharArray ja = (*env)->NewCharArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jchar)SvIV(*esv);
#ifdef WIN32
			env->SetCharArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetCharArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jchar);

#ifdef WIN32
		    jcharArray ja = env->NewCharArray(len);
#else
		    jcharArray ja = (*env)->NewCharArray(env, len);
#endif
#ifdef WIN32
		    env->SetCharArrayRegion(ja, 0, len, (jchar*)SvPV(sv,n_a));
#else
		    (*env)->SetCharArrayRegion(env, ja, 0, len, (jchar*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jshortArray ja = env->NewShortArray(len);
#else
			jshortArray ja = (*env)->NewShortArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jshort)SvIV(*esv);
#ifdef WIN32
			env->SetShortArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetShortArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jshort);

#ifdef WIN32
		    jshortArray ja = env->NewShortArray(len);
#else
		    jshortArray ja = (*env)->NewShortArray(env, len);
#endif
#ifdef WIN32
		    env->SetShortArrayRegion(ja, 0, len, (jshort*)SvPV(sv,n_a));
#else
		    (*env)->SetShortArrayRegion(env, ja, 0, len, (jshort*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jintArray ja = env->NewIntArray(len);
#else
			jintArray ja = (*env)->NewIntArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jint)SvIV(*esv);
#ifdef WIN32
			env->SetIntArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetIntArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jint);

#ifdef WIN32
		    jintArray ja = env->NewIntArray(len);
#else
		    jintArray ja = (*env)->NewIntArray(env, len);
#endif
#ifdef WIN32
		    env->SetIntArrayRegion(ja, 0, len, (jint*)SvPV(sv,n_a));
#else
		    (*env)->SetIntArrayRegion(env, ja, 0, len, (jint*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jlongArray ja = env->NewLongArray(len);
#else
			jlongArray ja = (*env)->NewLongArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jlong)SvNV(*esv);
#ifdef WIN32
			env->SetLongArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetLongArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jlong);

#ifdef WIN32
		    jlongArray ja = env->NewLongArray(len);
#else
		    jlongArray ja = (*env)->NewLongArray(env, len);
#endif
#ifdef WIN32
		    env->SetLongArrayRegion(ja, 0, len, (jlong*)SvPV(sv,n_a));
#else
		    (*env)->SetLongArrayRegion(env, ja, 0, len, (jlong*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jfloatArray ja = env->NewFloatArray(len);
#else
			jfloatArray ja = (*env)->NewFloatArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jfloat)SvNV(*esv);
#ifdef WIN32
			env->SetFloatArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetFloatArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jfloat);

#ifdef WIN32
		    jfloatArray ja = env->NewFloatArray(len);
#else
		    jfloatArray ja = (*env)->NewFloatArray(env, len);
#endif
#ifdef WIN32
		    env->SetFloatArrayRegion(ja, 0, len, (jfloat*)SvPV(sv,n_a));
#else
		    (*env)->SetFloatArrayRegion(env, ja, 0, len, (jfloat*)SvPV(sv,n_a));
#endif
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

#ifdef WIN32
			jdoubleArray ja = env->NewDoubleArray(len);
#else
			jdoubleArray ja = (*env)->NewDoubleArray(env, len);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++)
			    buf[i] = (jdouble)SvNV(*esv);
#ifdef WIN32
			env->SetDoubleArrayRegion(ja, 0, len, buf);
#else
			(*env)->SetDoubleArrayRegion(env, ja, 0, len, buf);
#endif
			free((void*)buf);
			jv[ix++].l = (jobject)ja;
		    }
		    else
			jv[ix++].l = (jobject)(void*)0;
		}
		else if (SvPOK(sv)) {
		    jsize len = sv_len(sv) / sizeof(jdouble);

#ifdef WIN32
		    jdoubleArray ja = env->NewDoubleArray(len);
#else
		    jdoubleArray ja = (*env)->NewDoubleArray(env, len);
#endif
#ifdef WIN32
		    env->SetDoubleArrayRegion(ja, 0, len, (jdouble*)SvPV(sv,n_a));
#else
		    (*env)->SetDoubleArrayRegion(env, ja, 0, len, (jdouble*)SvPV(sv,n_a));
#endif
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
#ifdef WIN32
				jcl = env->FindClass("java/lang/String");
#else
				jcl = (*env)->FindClass(env, "java/lang/String");
#endif
#ifdef WIN32
			    ja = env->NewObjectArray(len, jcl, 0);
#else
			    ja = (*env)->NewObjectArray(env, len, jcl, 0);
#endif
			    for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
#ifdef WIN32
				jobject str = (jobject)env->NewStringUTF(SvPV(*esv,n_a));
#else
				jobject str = (jobject)(*env)->NewStringUTF(env, SvPV(*esv,n_a));
#endif
#ifdef WIN32
				env->SetObjectArrayElement(ja, i, str);
#else
				(*env)->SetObjectArrayElement(env, ja, i, str);
#endif
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
#ifdef WIN32
			    jcl = env->FindClass("java/lang/Object");
#else
			    jcl = (*env)->FindClass(env, "java/lang/Object");
#endif
#ifdef WIN32
			ja = env->NewObjectArray(len, jcl, 0);
#else
			ja = (*env)->NewObjectArray(env, len, jcl, 0);
#endif
			for (esv = AvARRAY((AV*)rv), i = 0; i < len; esv++, i++) {
			    if (SvROK(*esv) && (rv = SvRV(*esv)) && SvOBJECT(rv)) {
#ifdef WIN32
				env->SetObjectArrayElement(ja, i, (jobject)(void*)SvIV(rv));
#else
				(*env)->SetObjectArrayElement(env, ja, i, (jobject)(void*)SvIV(rv));
#endif
			    }
			    else {
#ifdef WIN32
				jobject str = (jobject)env->NewStringUTF(SvPV(*esv,n_a));
#else
				jobject str = (jobject)(*env)->NewStringUTF(env, SvPV(*esv,n_a));
#endif
#ifdef WIN32
				env->SetObjectArrayElement(ja, i, str);
#else
				(*env)->SetObjectArrayElement(env, ja, i, str);
#endif
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
#ifdef WIN32
		jv[ix++].l = (jobject)env->NewStringUTF((char*) SvPV(sv,n_a));
#else
		jv[ix++].l = (jobject)(*env)->NewStringUTF(env, (char*) SvPV(sv,n_a));
#endif
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
#ifdef WIN32
	    RETVAL = env->GetVersion();
#else
	    RETVAL = (*env)->GetVersion(env);
#endif
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
#ifdef WIN32
	    RETVAL = env->DefineClass( loader, buf, (jsize)buf_len_);
#else
	    RETVAL = (*env)->DefineClass(env,  loader, buf, (jsize)buf_len_);
#endif
#else
#ifdef WIN32
	    RETVAL = env->DefineClass( name, loader, buf, (jsize)buf_len_); 
#else
	    RETVAL = (*env)->DefineClass(env,  name, loader, buf, (jsize)buf_len_); 
#endif
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
#ifdef WIN32
	    RETVAL = env->FindClass( name);
#else
	    RETVAL = (*env)->FindClass(env,  name);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetSuperclass( sub);
#else
	    RETVAL = (*env)->GetSuperclass(env,  sub);
#endif
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
#ifdef WIN32
	    RETVAL = env->IsAssignableFrom( sub, sup);
#else
	    RETVAL = (*env)->IsAssignableFrom(env,  sub, sup);
#endif
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
#ifdef WIN32
	    RETVAL = env->Throw( obj);
#else
	    RETVAL = (*env)->Throw(env,  obj);
#endif
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
#ifdef WIN32
	    RETVAL = env->ThrowNew( clazz, msg);
#else
	    RETVAL = (*env)->ThrowNew(env,  clazz, msg);
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

jthrowable
ExceptionOccurred()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->ExceptionOccurred();
#else
	    RETVAL = (*env)->ExceptionOccurred(env);
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

void
ExceptionDescribe()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
#ifdef WIN32
	    env->ExceptionDescribe();
#else
	    (*env)->ExceptionDescribe(env);
#endif
	    RESTOREENV;
	}

void
ExceptionClear()
	JNIEnv *		env = FETCHENV;
    CODE:
	{
#ifdef WIN32
	    env->ExceptionClear();
#else
	    (*env)->ExceptionClear(env);
#endif
	    RESTOREENV;
	}

void
FatalError(msg)
	JNIEnv *		env = FETCHENV;
	const char *		msg
    CODE:
	{
#ifdef WIN32
	    env->FatalError( msg);
#else
	    (*env)->FatalError(env,  msg);
#endif
	    RESTOREENV;
	}

jobject
NewGlobalRef(lobj)
	JNIEnv *		env = FETCHENV;
	jobject			lobj
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->NewGlobalRef(lobj);
#else
	    RETVAL = (*env)->NewGlobalRef(env, lobj);
#endif
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
#ifdef WIN32
	    env->DeleteGlobalRef(gref);
#else
	    (*env)->DeleteGlobalRef(env, gref);
#endif
	    RESTOREENV;
	}

void
DeleteLocalRef(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
#ifdef WIN32
	    env->DeleteLocalRef( obj);
#else
	    (*env)->DeleteLocalRef(env,  obj);
#endif
	    RESTOREENV;
	}

jboolean
IsSameObject(obj1,obj2)
	JNIEnv *		env = FETCHENV;
	jobject			obj1
	jobject			obj2
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->IsSameObject(obj1,obj2);
#else
	    RETVAL = (*env)->IsSameObject(env, obj1,obj2);
#endif
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
#ifdef WIN32
	    RETVAL = env->AllocObject(clazz);
#else
	    RETVAL = (*env)->AllocObject(env, clazz);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewObjectA(clazz,methodID,args);
#else
	    RETVAL = (*env)->NewObjectA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewObjectA(clazz,methodID,args);
#else
	    RETVAL = (*env)->NewObjectA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetObjectClass(obj);
#else
	    RETVAL = (*env)->GetObjectClass(env, obj);
#endif
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
#ifdef WIN32
	    RETVAL = env->IsInstanceOf(obj,clazz);
#else
	    RETVAL = (*env)->IsInstanceOf(env, obj,clazz);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetMethodID(clazz,name,sig);
#else
	    RETVAL = (*env)->GetMethodID(env, clazz,name,sig);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallObjectMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallObjectMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallObjectMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallObjectMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallBooleanMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallBooleanMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallBooleanMethodA(obj,methodID, args);
#else
	    RETVAL = (*env)->CallBooleanMethodA(env, obj,methodID, args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallByteMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallByteMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallByteMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallByteMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallCharMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallCharMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallCharMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallCharMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallShortMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallShortMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallShortMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallShortMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallIntMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallIntMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallIntMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallIntMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallLongMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallLongMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallLongMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallLongMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallFloatMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallFloatMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallFloatMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallFloatMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallDoubleMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallDoubleMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallDoubleMethodA(obj,methodID,args);
#else
	    RETVAL = (*env)->CallDoubleMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallVoidMethodA(obj,methodID,args);
#else
	    (*env)->CallVoidMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallVoidMethodA(obj,methodID,args);
#else
	    (*env)->CallVoidMethodA(env, obj,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualObjectMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualObjectMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualObjectMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualObjectMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualBooleanMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualBooleanMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualBooleanMethodA(obj,clazz,methodID, args);
#else
	    RETVAL = (*env)->CallNonvirtualBooleanMethodA(env, obj,clazz,methodID, args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualByteMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualByteMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualByteMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualByteMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualCharMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualCharMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualCharMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualCharMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualShortMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualShortMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualShortMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualShortMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualIntMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualIntMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualIntMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualIntMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualLongMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualLongMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualLongMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualLongMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualFloatMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualFloatMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualFloatMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualFloatMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualDoubleMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualDoubleMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallNonvirtualDoubleMethodA(obj,clazz,methodID,args);
#else
	    RETVAL = (*env)->CallNonvirtualDoubleMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallNonvirtualVoidMethodA(obj,clazz,methodID,args);
#else
	    (*env)->CallNonvirtualVoidMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallNonvirtualVoidMethodA(obj,clazz,methodID,args);
#else
	    (*env)->CallNonvirtualVoidMethodA(env, obj,clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetFieldID(clazz,name,sig);
#else
	    RETVAL = (*env)->GetFieldID(env, clazz,name,sig);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetObjectField(obj,fieldID);
#else
	    RETVAL = (*env)->GetObjectField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetBooleanField(obj,fieldID);
#else
	    RETVAL = (*env)->GetBooleanField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetByteField(obj,fieldID);
#else
	    RETVAL = (*env)->GetByteField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetCharField(obj,fieldID);
#else
	    RETVAL = (*env)->GetCharField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetShortField(obj,fieldID);
#else
	    RETVAL = (*env)->GetShortField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetIntField(obj,fieldID);
#else
	    RETVAL = (*env)->GetIntField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetLongField(obj,fieldID);
#else
	    RETVAL = (*env)->GetLongField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetFloatField(obj,fieldID);
#else
	    RETVAL = (*env)->GetFloatField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetDoubleField(obj,fieldID);
#else
	    RETVAL = (*env)->GetDoubleField(env, obj,fieldID);
#endif
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
#ifdef WIN32
	    env->SetObjectField(obj,fieldID,val);
#else
	    (*env)->SetObjectField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetBooleanField(obj,fieldID,val);
#else
	    (*env)->SetBooleanField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetByteField(obj,fieldID,val);
#else
	    (*env)->SetByteField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetCharField(obj,fieldID,val);
#else
	    (*env)->SetCharField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetShortField(obj,fieldID,val);
#else
	    (*env)->SetShortField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetIntField(obj,fieldID,val);
#else
	    (*env)->SetIntField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetLongField(obj,fieldID,val);
#else
	    (*env)->SetLongField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetFloatField(obj,fieldID,val);
#else
	    (*env)->SetFloatField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    env->SetDoubleField(obj,fieldID,val);
#else
	    (*env)->SetDoubleField(env, obj,fieldID,val);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticMethodID(clazz,name,sig);
#else
	    RETVAL = (*env)->GetStaticMethodID(env, clazz,name,sig);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticObjectMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticObjectMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticObjectMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticObjectMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticBooleanMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticBooleanMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticBooleanMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticBooleanMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticByteMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticByteMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticByteMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticByteMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticCharMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticCharMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticCharMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticCharMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticShortMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticShortMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticShortMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticShortMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticIntMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticIntMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticIntMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticIntMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticLongMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticLongMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticLongMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticLongMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticFloatMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticFloatMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticFloatMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticFloatMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticDoubleMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticDoubleMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->CallStaticDoubleMethodA(clazz,methodID,args);
#else
	    RETVAL = (*env)->CallStaticDoubleMethodA(env, clazz,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallStaticVoidMethodA(cls,methodID,args);
#else
	    (*env)->CallStaticVoidMethodA(env, cls,methodID,args);
#endif
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
#ifdef WIN32
	    env->CallStaticVoidMethodA(cls,methodID,args);
#else
	    (*env)->CallStaticVoidMethodA(env, cls,methodID,args);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticFieldID(clazz,name,sig);
#else
	    RETVAL = (*env)->GetStaticFieldID(env, clazz,name,sig);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticObjectField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticObjectField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticBooleanField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticBooleanField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticByteField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticByteField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticCharField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticCharField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticShortField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticShortField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticIntField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticIntField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticLongField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticLongField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticFloatField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticFloatField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStaticDoubleField(clazz,fieldID);
#else
	    RETVAL = (*env)->GetStaticDoubleField(env, clazz,fieldID);
#endif
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
#ifdef WIN32
	  env->SetStaticObjectField(clazz,fieldID,value);
#else
	  (*env)->SetStaticObjectField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticBooleanField(clazz,fieldID,value);
#else
	  (*env)->SetStaticBooleanField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticByteField(clazz,fieldID,value);
#else
	  (*env)->SetStaticByteField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticCharField(clazz,fieldID,value);
#else
	  (*env)->SetStaticCharField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticShortField(clazz,fieldID,value);
#else
	  (*env)->SetStaticShortField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticIntField(clazz,fieldID,value);
#else
	  (*env)->SetStaticIntField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticLongField(clazz,fieldID,value);
#else
	  (*env)->SetStaticLongField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticFloatField(clazz,fieldID,value);
#else
	  (*env)->SetStaticFloatField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	  env->SetStaticDoubleField(clazz,fieldID,value);
#else
	  (*env)->SetStaticDoubleField(env, clazz,fieldID,value);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewString(unicode, unicode_len_);
#else
	    RETVAL = (*env)->NewString(env, unicode, unicode_len_);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStringLength(str);
#else
	    RETVAL = (*env)->GetStringLength(env, str);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStringChars(str,&isCopy);
#else
	    RETVAL = (*env)->GetStringChars(env, str,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetStringLength(str);
#else
	    RETVAL_len_ = (*env)->GetStringLength(env, str);
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL
    CLEANUP:
#ifdef WIN32
	    env->ReleaseStringChars(str,RETVAL);
#else
	    (*env)->ReleaseStringChars(env, str,RETVAL);
#endif

jstring
NewStringUTF(utf)
	JNIEnv *		env = FETCHENV;
	const char *		utf
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->NewStringUTF(utf);
#else
	    RETVAL = (*env)->NewStringUTF(env, utf);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStringUTFLength(str);
#else
	    RETVAL = (*env)->GetStringUTFLength(env, str);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetStringUTFChars(str,&isCopy);
#else
	    RETVAL = (*env)->GetStringUTFChars(env, str,&isCopy);
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL
    CLEANUP:
#ifdef WIN32
	env->ReleaseStringUTFChars(str, RETVAL);
#else
	(*env)->ReleaseStringUTFChars(env, str, RETVAL);
#endif


jsize
GetArrayLength(array)
	JNIEnv *		env = FETCHENV;
	jarray			array
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->GetArrayLength(array);
#else
	    RETVAL = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewObjectArray(len,clazz,init);
#else
	    RETVAL = (*env)->NewObjectArray(env, len,clazz,init);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetObjectArrayElement(array,index);
#else
	    RETVAL = (*env)->GetObjectArrayElement(env, array,index);
#endif
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
#ifdef WIN32
	    env->SetObjectArrayElement(array,index,val);
#else
	    (*env)->SetObjectArrayElement(env, array,index,val);
#endif
	    RESTOREENV;
	}

jbooleanArray
NewBooleanArray(len)
	JNIEnv *		env = FETCHENV;
	jsize			len
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->NewBooleanArray(len);
#else
	    RETVAL = (*env)->NewBooleanArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewByteArray(len);
#else
	    RETVAL = (*env)->NewByteArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewCharArray(len);
#else
	    RETVAL = (*env)->NewCharArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewShortArray(len);
#else
	    RETVAL = (*env)->NewShortArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewIntArray(len);
#else
	    RETVAL = (*env)->NewIntArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewLongArray(len);
#else
	    RETVAL = (*env)->NewLongArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewFloatArray(len);
#else
	    RETVAL = (*env)->NewFloatArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->NewDoubleArray(len);
#else
	    RETVAL = (*env)->NewDoubleArray(env, len);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetBooleanArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetBooleanArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseBooleanArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseBooleanArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetByteArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetByteArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseByteArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseByteArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetCharArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetCharArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseCharArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseCharArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetShortArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetShortArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseShortArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseShortArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetIntArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetIntArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseIntArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseIntArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetLongArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetLongArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseLongArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseLongArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetFloatArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetFloatArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseFloatArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseFloatArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    RETVAL = env->GetDoubleArrayElements(array,&isCopy);
#else
	    RETVAL = (*env)->GetDoubleArrayElements(env, array,&isCopy);
#endif
#ifdef WIN32
	    RETVAL_len_ = env->GetArrayLength(array);
#else
	    RETVAL_len_ = (*env)->GetArrayLength(env, array);
#endif
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
#ifdef WIN32
	    env->ReleaseDoubleArrayElements(array,RETVAL,JNI_ABORT);
#else
	    (*env)->ReleaseDoubleArrayElements(env, array,RETVAL,JNI_ABORT);
#endif
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
#ifdef WIN32
	    env->GetBooleanArrayRegion(array,start,len,buf);
#else
	    (*env)->GetBooleanArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetByteArrayRegion(array,start,len,buf);
#else
	    (*env)->GetByteArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetCharArrayRegion(array,start,len,buf);
#else
	    (*env)->GetCharArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetShortArrayRegion(array,start,len,buf);
#else
	    (*env)->GetShortArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetIntArrayRegion(array,start,len,buf);
#else
	    (*env)->GetIntArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetLongArrayRegion(array,start,len,buf);
#else
	    (*env)->GetLongArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetFloatArrayRegion(array,start,len,buf);
#else
	    (*env)->GetFloatArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->GetDoubleArrayRegion(array,start,len,buf);
#else
	    (*env)->GetDoubleArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetBooleanArrayRegion(array,start,len,buf);
#else
	    (*env)->SetBooleanArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetByteArrayRegion(array,start,len,buf);
#else
	    (*env)->SetByteArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetCharArrayRegion(array,start,len,buf);
#else
	    (*env)->SetCharArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetShortArrayRegion(array,start,len,buf);
#else
	    (*env)->SetShortArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetIntArrayRegion(array,start,len,buf);
#else
	    (*env)->SetIntArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetLongArrayRegion(array,start,len,buf);
#else
	    (*env)->SetLongArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetFloatArrayRegion(array,start,len,buf);
#else
	    (*env)->SetFloatArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    env->SetDoubleArrayRegion(array,start,len,buf);
#else
	    (*env)->SetDoubleArrayRegion(env, array,start,len,buf);
#endif
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
#ifdef WIN32
	    RETVAL = env->RegisterNatives(clazz,methods,nMethods);
#else
	    RETVAL = (*env)->RegisterNatives(env, clazz,methods,nMethods);
#endif
	}

SysRet
UnregisterNatives(clazz)
	JNIEnv *		env = FETCHENV;
	jclass			clazz
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->UnregisterNatives(clazz);
#else
	    RETVAL = (*env)->UnregisterNatives(env, clazz);
#endif
	}
    OUTPUT:
	RETVAL  
   
SysRet
MonitorEnter(obj)
	JNIEnv *		env = FETCHENV;
	jobject			obj
    CODE:
	{
#ifdef WIN32
	    RETVAL = env->MonitorEnter(obj);
#else
	    RETVAL = (*env)->MonitorEnter(env, obj);
#endif
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
#ifdef WIN32
	    RETVAL = env->MonitorExit(obj);
#else
	    RETVAL = (*env)->MonitorExit(env, obj);
#endif
	    RESTOREENV;
	}
    OUTPUT:
	RETVAL

JavaVM *
GetJavaVM(...)
	JNIEnv *		env = FETCHENV;
    CODE:
	{
	    if (env) {	/* We're embedded. */
#ifdef WIN32
		if (env->GetJavaVM(&RETVAL) < 0)
#else
		if ((*env)->GetJavaVM(env, &RETVAL) < 0)
#endif
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
		if (!LoadLibrary("javai.dll")) {
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

		JNI_GetDefaultJavaVMInitArgs(&vm_args);
		vm_args.exit = &call_my_exit;
		if (jpldebug) {
            fprintf(stderr, "items = %d\n", items);
            fprintf(stderr, "mark = %s\n", SvPV(*mark, PL_na));
        }
		++mark;
		while (items > 1) {
		    char *s = SvPV(*mark,PL_na);
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
#ifndef KAFFE
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
		JNI_CreateJavaVM(&RETVAL, &jplcurenv, &vm_args);
		if (jpldebug) {
		    fprintf(stderr, "Created Java VM.\n");
		}
	    }
	}

