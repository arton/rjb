/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005 arton
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id: rjb.h 180 2011-12-05 16:34:29Z arton $
 * $Log: rjb.h,v $
 * Revision 1.1  2005/01/16 17:36:10  arton
 * Initial revision
 *
 *
 */

#ifndef RJB_H
#define RJB_H

#if RJB_RUBY_VERSION_CODE < 190
#if !defined(RHASH_TBL)
#define RHASH_TBL(x) RHASH((x))->tbl
#endif
#endif

#if !defined(RSTRING_LEN)
#define RSTRING_LEN(s) (RSTRING(s)->len)
#endif
#if !defined(RSTRING_PTR)
#define RSTRING_PTR(s) (RSTRING(s)->ptr)
#endif
#if !defined(RARRAY_LEN)
#define RARRAY_LEN(s) (RARRAY(s)->len)
#endif
#if !defined(RARRAY_PTR)
#define RARRAY_PTR(s) (RARRAY(s)->ptr)
#endif

#if !defined(COUNTOF)
#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))
#endif

#if !defined(_I64_MIN)
#define _I64_MIN    (-9223372036854775807i64 - 1)
#endif
#if !defined(_I64_MAX)
#define _I64_MAX      9223372036854775807i64
#endif

#if !defined(HAVE_LONG_LONG) && defined(__LP64__)
#define HAVE_LONG_LONG 1
#endif

/* in load.c */
extern int rjb_create_jvm(JNIEnv** pjenv, JavaVMInitArgs*, char*, VALUE);

/* in rjb.c */
extern JavaVM* rjb_jvm;
extern jclass rjb_rbridge;
extern jmethodID rjb_register_bridge;
extern VALUE rjb_loaded_classes;
extern jmethodID rjb_class_getName;
extern jclass rjb_j_throwable;
extern jmethodID rjb_throwable_getMessage;
extern JNIEnv* rjb_attach_current_thread(void);
extern jclass rjb_find_class(JNIEnv* jenv, VALUE name);
extern void rjb_release_string(JNIEnv *jenv, jstring str, const char* chrs);
extern VALUE rjb_load_vm_default();
extern void rjb_unload_vm();
extern VALUE rjb_safe_funcall(VALUE args);
extern VALUE jv2rv(JNIEnv* jenv, jvalue val);
extern jobject get_systemloader(JNIEnv* jenv);
extern jclass rjb_find_class_by_name(JNIEnv* jenv, const char* name);

/* in rjbexception.c */
extern VALUE rjb_get_exception_class(JNIEnv* jenv, jstring str);
extern void rjb_check_exception(JNIEnv* jenv, int t);
extern VALUE rjb_s_throw(int, VALUE*, VALUE);

/* conversion functions */
typedef void (*R2J)(JNIEnv*, VALUE, jvalue*, const char*, int);
typedef VALUE (*J2R)(JNIEnv*, jvalue);
typedef jarray (*R2JARRAY)(JNIEnv*, VALUE, const char*);
typedef void (JNICALL *RELEASEARRAY)(JNIEnv*, jobject, void*, jint);
typedef jlong (JNICALL *INVOKEAL)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef jdouble (JNICALL *INVOKEAD)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef jfloat (JNICALL *INVOKEAF)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef jboolean (JNICALL *INVOKEAZ)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef jshort (JNICALL *INVOKEAS)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef jobject (JNICALL *INVOKEA)(JNIEnv*, jobject, jmethodID, const jvalue*);
typedef VALUE (*CONV)(JNIEnv*, void*);

/*
 * internal method class
 */
struct cls_constructor {
    jmethodID id;
    int arg_count;
    R2J* arg_convert;
    char* method_signature;
    char  result_signature;
    char  result_arraydepth;
};

struct cls_method {
    struct cls_constructor basic;
    ID name;
    int static_method;
    off_t method;
    J2R result_convert;
    /* overload only */
    struct cls_method* next;
};

/*
 * internal field class
 */
struct cls_field {
    ID name;
    jfieldID id;
    char* field_signature;
    char  result_signature;
    char  result_arraydepth;
    R2J arg_convert;
    J2R value_convert;
    int readonly;
    int static_field;
};

/*
 * Object instance
 */
struct jvi_data {
    jclass klass; /* class */
    jobject obj; /* instance */
    st_table* methods;
    st_table* fields;
};

/*
 * Class instance
 */
struct jv_data {
    struct jvi_data idata;
    st_table* static_methods;
    struct cls_constructor** constructors;
};

/*
 * Bridge instance
 */
struct rj_bridge {
    jobject bridge;
    jobject proxy;
    VALUE wrapped;
};

#endif
