/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005,2006,2010 arton
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
 * $Id: rjbexception.c 174 2011-11-09 13:47:43Z arton $
 */

#include "ruby.h"
#include "extconf.h"
#if RJB_RUBY_VERSION_CODE < 190
#include "st.h"
#else
#include "ruby/st.h"
#endif
#include "jniwrap.h"
#include "riconv.h"
#include "rjb.h"

static VALUE missing_delegate(int argc, VALUE* argv, VALUE self)
{
    ID rmid = rb_to_id(argv[0]);
    return rb_funcallv(rb_ivar_get(self, rb_intern("@cause")), rmid, argc - 1, argv + 1);
}

static VALUE get_cause(VALUE self)
{
    return rb_funcall(rb_ivar_get(self, rb_intern("@cause")), rb_intern("cause"), 0);
}

static VALUE ex_respond_to(int argc, VALUE* argv, VALUE self)
{
    if (argc < 1 || argc > 2)
    {
        rb_raise(rb_eArgError, "respond_to? require 1 or 2 arguments");
    }
    if (rb_to_id(argv[0]) == rb_intern("to_str"))
    {
        return  Qfalse;
    }
    else  if (rb_to_id(argv[0]) == rb_intern("exception"))
    {
        return Qtrue;
    }
    else
    {
        return rb_funcallv(rb_ivar_get(self, rb_intern("@cause")), rb_intern("respond_to?"), argc, argv);
    }
}

/*
 * handle Java exception
 *  At this time, the Java exception is defined without the package name.
 *  This design may change in future release.
 */
VALUE rjb_get_exception_class(JNIEnv* jenv, jstring str)
{
    VALUE rexp;
    char* pcls;
    VALUE cname;
    const char* p = (*jenv)->GetStringUTFChars(jenv, str, JNI_FALSE);
    char* clsname = ALLOCA_N(char, strlen(p) + 1);
    strcpy(clsname, p);
    rjb_release_string(jenv, str, p);
    pcls = strrchr(clsname, '.');
    if (pcls)
    {
	pcls++;
    }
    else
    {
	pcls = clsname;
    }
    cname = rb_str_new2(pcls);
    rexp = rb_hash_aref(rjb_loaded_classes, cname);
    if (rexp == Qnil)
    {
        rexp = rb_define_class(pcls, rb_eStandardError);
        rb_define_method(rexp, "cause", get_cause, 0);
        rb_define_method(rexp, "method_missing", missing_delegate, -1);
        rb_define_method(rexp, "respond_to?", ex_respond_to, -1);
#if defined(HAVE_RB_HASH_ASET) || defined(RUBINIUS)
	rb_hash_aset(rjb_loaded_classes, cname, rexp);
#else
  #ifdef RHASH_TBL
        st_insert(RHASH_TBL(rjb_loaded_classes), cname, rexp);
  #else
        st_insert(RHASH(rjb_loaded_classes)->tbl, cname, rexp);
  #endif
#endif

    }
    return rexp;
}

/*
 * throw newly created exception with supplied message.
 */
VALUE rjb_s_throw(int argc, VALUE* argv, VALUE self)
{
    VALUE klass;
    VALUE message;
    JNIEnv* jenv = NULL;

    rjb_load_vm_default();

    jenv = rjb_attach_current_thread();
    (*jenv)->ExceptionClear(jenv);

    if (rb_scan_args(argc, argv, "11", &klass, &message) == 2)
    {
        jclass excep = rjb_find_class(jenv, klass);
	if (excep == NULL)
	{
	    rb_raise(rb_eRuntimeError, "`%s' not found", StringValueCStr(klass));
        }
	(*jenv)->ThrowNew(jenv, excep, StringValueCStr(message));
    }
    else
    {
        struct jvi_data* ptr;
	Data_Get_Struct(klass, struct jvi_data, ptr);
	if (!(*jenv)->IsInstanceOf(jenv, ptr->obj, rjb_j_throwable))
	{
	    rb_raise(rb_eRuntimeError, "arg1 must be a throwable");
	}
	else
	{
  	    (*jenv)->Throw(jenv, ptr->obj);
	}
    }
    return Qnil;
}

void rjb_check_exception(JNIEnv* jenv, int t)
{
    jthrowable exp = (*jenv)->ExceptionOccurred(jenv);
    if (exp)
    {
	VALUE rexp = Qnil;
        if (RTEST(ruby_verbose))
	{
	    (*jenv)->ExceptionDescribe(jenv);
	}
	(*jenv)->ExceptionClear(jenv);
        if(1)
	{
            char* msg = (char*)"unknown exception";
	    jclass cls = (*jenv)->GetObjectClass(jenv, exp);
 	    jstring str = (*jenv)->CallObjectMethod(jenv, exp, rjb_throwable_getMessage);
	    if (str)
	    {
	        const char* p = (*jenv)->GetStringUTFChars(jenv, str, JNI_FALSE);
		msg = ALLOCA_N(char, strlen(p) + 1);
		strcpy(msg, p);
		rjb_release_string(jenv, str, p);
	    }
	    str = (*jenv)->CallObjectMethod(jenv, cls, rjb_class_getName);
	    if (str)
	    {
		rexp = rjb_get_exception_class(jenv, str);
	    }
	    if (rexp == Qnil)
	    {
                (*jenv)->DeleteLocalRef(jenv, exp);
		rb_raise(rb_eRuntimeError, "%s", msg);
	    }
	    else
	    {
                VALUE rexpi = rb_funcall(rexp, rb_intern("new"), 1, rb_str_new2(msg));
                jvalue val;
                val.l = exp;
                rb_ivar_set(rexpi, rb_intern("@cause"), jv2rv(jenv, val));
                rb_exc_raise(rexpi);
	    }
        }
    }
}
