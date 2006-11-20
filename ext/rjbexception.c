/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005,2006 arton
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
 * $Id$
 */

#include "ruby.h"
#include "st.h"
#include "jniwrap.h"
#include "riconv.h"
#include "rjb.h"

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
	st_insert(RHASH(rjb_loaded_classes)->tbl, cname, rexp);
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
    JNIEnv* jenv = rjb_attach_current_thread();
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
//        if (t)
        if(1)
	{
 	    char* msg = "unknown exception";
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
		rb_raise(rb_eRuntimeError, "%s", msg);
	    }
	    else
	    {
		rb_raise(rexp, msg);
	    }
        }
    }
}

