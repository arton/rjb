/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005,2006,2007,2008,2009,2010,2011,2012,2014-2016,2018 arton
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
 */

#define RJB_VERSION "1.6.2"

#include "ruby.h"
#include "extconf.h"
#if RJB_RUBY_VERSION_CODE < 190
#include "st.h"
#else
#include "ruby/st.h"
#endif
#include "jniwrap.h"
#include "jp_co_infoseek_hp_arton_rjb_RBridge.h"
#include "riconv.h"
#include "rjb.h"
#include "ctype.h"

/*
 * Method Modifier Flag defined in
 * http://java.sun.com/docs/books/vmspec/2nd-edition/html/ClassFile.doc.html#88358
 */
#define ACC_PUBLIC  0x0001
#define ACC_PRIVATE  0x0002
#define ACC_PROTECTED  0x0004
#define ACC_STATIC  0x0008
#define ACC_FINAL  0x0010
#define ACC_VOLATILE  0x0040
#define ACC_TRANSIENT  0x0080

#define RJB_FIND_CLASS(var, name)              \
    var = rjb_find_class_by_name(jenv, name); \
    rjb_check_exception(jenv, 1)
#define RJB_HOLD_CLASS(var, name)              \
    var = rjb_find_class_by_name(jenv, name); \
    rjb_check_exception(jenv, 1);               \
    var = (*jenv)->NewGlobalRef(jenv, var)
#define RJB_LOAD_METHOD(var, obj, name, sig) \
    var = (*jenv)->GetMethodID(jenv, obj, name, sig); \
    rjb_check_exception(jenv, 1)
#define RJB_LOAD_STATIC_METHOD(var, obj, name, sig) \
    var = (*jenv)->GetStaticMethodID(jenv, obj, name, sig); \
    rjb_check_exception(jenv, 1)
#if defined(RUBINIUS)
#define CLASS_NEW(obj, name) rb_define_class_under(rjb, name, obj)
#define CLASS_INHERITED(spr, kls) RTEST(rb_funcall(spr, rb_intern(">="), 1, kls))
#else
#define CLASS_NEW(obj, name) rb_define_class_under(rjb, name, obj)
#define CLASS_INHERITED(spr, kls) RTEST(rb_funcall(spr, rb_intern(">="), 1, kls))
#endif
#define IS_RJB_OBJECT(v) (CLASS_INHERITED(rjbi, rb_obj_class(v)) || rb_obj_class(v) == rjb || CLASS_INHERITED(rjbb, rb_obj_class(v)))
#define USER_INITIALIZE "@user_initialize"

static void register_class(VALUE, VALUE);
static VALUE import_class(JNIEnv* jenv, jclass, VALUE);
static VALUE register_instance(JNIEnv* jenv, VALUE klass, struct jv_data*, jobject);
static VALUE rjb_s_free(struct jv_data*);
static VALUE rjb_class_forname(int argc, VALUE* argv, VALUE self);
static void setup_metadata(JNIEnv* jenv, VALUE self, struct jv_data*, VALUE classname);
static VALUE jarray2rv(JNIEnv* jenv, jvalue val);
static jarray r2objarray(JNIEnv* jenv, VALUE v, const char* cls);
static VALUE jv2rv_withprim(JNIEnv* jenv, jobject o);
static J2R get_arrayconv(const char* cname, char* depth);
static jarray r2barray(JNIEnv* jenv, VALUE v, const char* cls);
static VALUE rjb_s_bind(VALUE self, VALUE rbobj, VALUE itfname);

static VALUE rjb;
static VALUE jklass;
static VALUE rjbc;
static VALUE rjbi;
static VALUE rjbb;
static VALUE rjba;

static ID user_initialize;
static ID initialize_proxy;
static ID cvar_classpath;
static ID anonymousblock;
static ID id_call;

VALUE rjb_loaded_classes;
static VALUE proxies;
JavaVM* rjb_jvm;
jclass rjb_rbridge;
jmethodID rjb_register_bridge;
jmethodID rjb_load_class;
static JNIEnv* main_jenv;
static VALUE primitive_conversion = Qfalse;

/*
 * Object cache, never destroyed
 */
/* method */
static jmethodID method_getModifiers;
static jmethodID method_getName;
static jmethodID getParameterTypes;
static jmethodID getReturnType;
/* field */
static jmethodID field_getModifiers;
static jmethodID field_getName;
static jmethodID field_getType;
/* constructor */
static jmethodID ctrGetParameterTypes;
/* class */
static jclass j_class;
jmethodID rjb_class_getName;
/* throwable */
jclass rjb_j_throwable;
jmethodID rjb_throwable_getMessage;
/* String global reference */
static jclass j_string;
static jmethodID str_tostring;
/* Object global reference */
static jclass j_object;
/* ClassLoader */
static jclass j_classloader;
static jmethodID get_system_classloader;
/* URLClassLoader */
static jclass j_url_loader;
static jobject url_loader;
static jmethodID url_loader_new;
static jmethodID url_geturls;
static jmethodID url_add_url;
/* URL global reference */
static jclass j_url;
static jmethodID url_new;

enum PrimitiveType {
    PRM_INT = 0,
    PRM_LONG,
    PRM_DOUBLE,
    PRM_BOOLEAN,
    PRM_CHARACTER,
    PRM_SHORT,
    PRM_BYTE,
    PRM_FLOAT,
    /* */
    PRM_LAST
};

/*
 * Native type conversion table
 */
typedef struct jobject_ruby_table {
    const char* classname;
    const char* to_prim_method;
    const char* prmsig;
    const char* ctrsig;
    jclass klass; /* primitive class */
    jmethodID to_prim_id;
    jmethodID ctr_id;
    J2R func;
} jprimitive_table;

JNIEnv* rjb_attach_current_thread(void)
{
  JNIEnv* env;
  if (!rjb_jvm) return NULL;
  (*rjb_jvm)->AttachCurrentThread(rjb_jvm, (void**)&env, '\0');
  return env;
}

void rjb_release_string(JNIEnv *jenv, jstring str, const char* chrs)
{
    (*jenv)->ReleaseStringUTFChars(jenv, str, chrs);
    (*jenv)->DeleteLocalRef(jenv, str);
}

static char* java2jniname(char* jnicls)
{
    char* p;
    for (p = jnicls; *p; p++)
    {
	if (*p == '.')
	{
	    *p = '/';
	}
    }
    return jnicls;
}

static char* jniname2java(char* jniname)
{
    char* p;
    for (p = jniname; *p; p++)
    {
	if (*p == '/')
	{
	    *p = '.';
	}
    }
    return jniname;
}

static char* next_sig(char* p)
{
    if (!*p)
    {
	return p;
    }
    if (*p == '[')
    {
	p++;
    }
    if (*p == 'L')
    {
	while (*p && *p != ';')
	{
	    p++;
	}
    }
    return (*p) ? ++p : p;
}

static VALUE jstring2val(JNIEnv* jenv, jstring s)
{
    const char* p;
    VALUE v;

    if (s == NULL)
    {
        return Qnil;
    }
    p = (*jenv)->GetStringUTFChars(jenv, s, NULL);
    v = rb_str_new2(p);
    v = exticonv_utf8_to_local(v);
    rjb_release_string(jenv, s, p);
    return v;
}

/*
 * Type conversion tables
 */
typedef struct type_conversion_table {
    const char* jtype;
    const char* jntype;
    R2J r2j;
    J2R j2r;
    J2R ja2r;
    R2JARRAY r2ja;
    off_t jcall;	/* for instance method */
    off_t jscall;	/* for static method */
} jconv_table;

/*
 * conversion methods
 * val will be released in this function.
 */
static VALUE jv2rclass(JNIEnv* jenv, jclass jc)
{
    const char* cname;
    VALUE clsname;
    VALUE v;
    jstring nm = (*jenv)->CallObjectMethod(jenv, jc, rjb_class_getName);
    rjb_check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    clsname = rb_str_new2(cname);
    rjb_release_string(jenv, nm, cname);
    v = rb_hash_aref(rjb_loaded_classes, clsname);
    if (v == Qnil)
    {
        v = import_class(jenv, jc, clsname);
    }
    (*jenv)->DeleteLocalRef(jenv, jc);
    return v;
}

static VALUE jv2rv_r(JNIEnv* jenv, jvalue val)
{
    const char* cname;
    jstring nm;
    jclass klass;
    VALUE clsname;
    VALUE v;
    struct jv_data* ptr;
    /* object to ruby */
    if (!val.l) return Qnil;
    klass = (*jenv)->GetObjectClass(jenv, val.l);

    if ((*jenv)->IsSameObject(jenv, klass, j_class))
    {
        (*jenv)->DeleteLocalRef(jenv, klass);
        return jv2rclass(jenv, val.l);
    }
    nm = (*jenv)->CallObjectMethod(jenv, klass, rjb_class_getName);
    rjb_check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    if (*cname == '[')
    {
        char depth = 0;
        J2R j2r = get_arrayconv(cname, &depth);
        rjb_release_string(jenv, nm, cname);
        v = j2r(jenv, val);
        (*jenv)->DeleteLocalRef(jenv, klass);
        (*jenv)->DeleteLocalRef(jenv, val.l);
        return v;
    }
    clsname = rb_str_new2(cname);
    rjb_release_string(jenv, nm, cname);
    v = rb_hash_aref(rjb_loaded_classes, clsname);
    if (v == Qnil)
    {
        v = import_class(jenv, klass, clsname);
    }
    Data_Get_Struct(v, struct jv_data, ptr);
    v = register_instance(jenv, v, (struct jv_data*)ptr, val.l);
    (*jenv)->DeleteLocalRef(jenv, klass);
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}

VALUE jv2rv(JNIEnv* jenv, jvalue val)
{
    if (RTEST(primitive_conversion))
    {
        return jv2rv_withprim(jenv, val.l);
    }
    return jv2rv_r(jenv, val);
}

static VALUE jvoid2rv(JNIEnv* jenv, jvalue val)
{
    return Qnil;
}

static VALUE jbyte2rv(JNIEnv* jenv, jvalue val)
{
    return INT2NUM(val.b);
}

static VALUE jchar2rv(JNIEnv* jenv, jvalue val)
{
    return INT2NUM(val.c);
}

static VALUE jdouble2rv(JNIEnv* jenv, jvalue val)
{
    return rb_float_new(val.d);
}

static VALUE jfloat2rv(JNIEnv* jenv, jvalue val)
{
    return rb_float_new((double)val.f);
}

static VALUE jint2rv(JNIEnv* jenv, jvalue val)
{
    return INT2NUM(val.i);
}

static VALUE jlong2rv(JNIEnv* jenv, jvalue val)
{
#if HAVE_LONG_LONG
    return LL2NUM(val.j);
#else
    char bignum[64];
    sprintf(bignum, "%ld * 0x100000000 + 0x%lx",
	    (long)(val.j >> 32), (unsigned long)val.j);
    return rb_eval_string(bignum);
#endif
}

static VALUE jshort2rv(JNIEnv* jenv, jvalue val)
{
    return INT2NUM(val.s);
}

static VALUE jboolean2rv(JNIEnv* jenv, jvalue val)
{
    return (val.z) ? Qtrue : Qfalse;
}

static VALUE jstring2rv(JNIEnv* jenv, jvalue val)
{
    return jstring2val(jenv, (jstring)val.l);
}

static VALUE ja2r(J2R conv, JNIEnv* jenv, jvalue val, int depth)
{
    jsize len;
    VALUE v;
    int i;
    if (!val.l) return Qnil;
    if (depth == 1)
    {
        return conv(jenv, val);
    }
    len = (*jenv)->GetArrayLength(jenv, val.l);
    v = rb_ary_new2(len);
    for (i = 0; i < len; i++)
    {
	jvalue wrap;
	wrap.l = (*jenv)->GetObjectArrayElement(jenv, val.l, i);
	rb_ary_push(v, ja2r(conv, jenv, wrap, depth - 1));
    }
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}

static VALUE jarray2rv(JNIEnv* jenv, jvalue val)
{
    jsize len;
    VALUE v;
    int i;
    if (!val.l) return Qnil;
    len = (*jenv)->GetArrayLength(jenv, val.l);
    v = rb_ary_new2(len);
    for (i = 0; i < len; i++)
    {
	jvalue wrap;
	wrap.l = (*jenv)->GetObjectArrayElement(jenv, val.l, i);
	/* wrap.l will be release in jv2rv */
	rb_ary_push(v, jv2rv(jenv, wrap));
    }
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}

static VALUE ca2rv(JNIEnv* jenv, void* p)
{
    return INT2FIX(*(jchar*)p);
}

static VALUE da2rv(JNIEnv* jenv, void* p)
{
    return rb_float_new(*(jdouble*)p);
}

static VALUE fa2rv(JNIEnv* jenv, void* p)
{
    return rb_float_new(*(jfloat*)p);
}

static VALUE ia2rv(JNIEnv* jenv, void* p)
{
    return INT2NUM(*(jint*)p);
}

static VALUE la2rv(JNIEnv* jenv, void* p)
{
#if HAVE_LONG_LONG
    return LL2NUM(*(jlong*)p);
#else
    return LONG2NUM(*(jlong*)p);
#endif
}

static VALUE sa2rv(JNIEnv* jenv, void* p)
{
    return INT2FIX(*(jshort*)p);
}

static VALUE ba2rv(JNIEnv* jenv, void* p)
{
    return (*(jboolean*)p) ? Qtrue : Qfalse;
}

/*
 * val : released in this function.
 */
static VALUE call_conv(JNIEnv* jenv, jvalue val, size_t sz, void* p, CONV conv, size_t fnc)
{
    int i;
    char* cp = (char*)p;
    jsize len = (*jenv)->GetArrayLength(jenv, val.l);
    VALUE v = rb_ary_new2(len);
    for (i = 0; i < len; i++)
    {
        rb_ary_push(v, conv(jenv, cp));
	cp += sz;
    }
    (*(RELEASEARRAY*)(((char*)*jenv) + fnc))(jenv, val.l, p, JNI_ABORT);
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}

static VALUE jbytearray2rv(JNIEnv* jenv, jvalue val)
{
    jsize len = (*jenv)->GetArrayLength(jenv, val.l);
    jbyte* p = (*jenv)->GetByteArrayElements(jenv, val.l, NULL);
    VALUE v = rb_str_new((char*)p, len);
    (*jenv)->ReleaseByteArrayElements(jenv, val.l, p, JNI_ABORT);
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}
static VALUE jchararray2rv(JNIEnv* jenv, jvalue val)
{
    jchar* p = (*jenv)->GetCharArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jchar), p, ca2rv,
			offsetof(struct JNINativeInterface_, ReleaseCharArrayElements));
}
static VALUE jdoublearray2rv(JNIEnv* jenv, jvalue val)
{
    jdouble* p = (*jenv)->GetDoubleArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jdouble), p, da2rv,
			offsetof(struct JNINativeInterface_, ReleaseDoubleArrayElements));
}
static VALUE jfloatarray2rv(JNIEnv* jenv, jvalue val)
{
    jfloat* p = (*jenv)->GetFloatArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jfloat), p, fa2rv,
			offsetof(struct JNINativeInterface_, ReleaseFloatArrayElements));
}
static VALUE jintarray2rv(JNIEnv* jenv, jvalue val)
{
    jint* p = (*jenv)->GetIntArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jint), p, ia2rv,
			offsetof(struct JNINativeInterface_, ReleaseIntArrayElements));
}
static VALUE jlongarray2rv(JNIEnv* jenv, jvalue val)
{
    jlong* p = (*jenv)->GetLongArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jlong), p, la2rv,
			offsetof(struct JNINativeInterface_, ReleaseLongArrayElements));
}
static VALUE jshortarray2rv(JNIEnv* jenv, jvalue val)
{
    jshort* p = (*jenv)->GetShortArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jshort), p, sa2rv,
			offsetof(struct JNINativeInterface_, ReleaseShortArrayElements));
}
static VALUE jstringarray2rv(JNIEnv* jenv, jvalue val)
{
    int i;
    jsize len = (*jenv)->GetArrayLength(jenv, val.l);
    VALUE v = rb_ary_new2(len);
    for (i = 0; i < len; i++)
    {
	jobject p = (*jenv)->GetObjectArrayElement(jenv, val.l, i);
	rb_ary_push(v, jstring2val(jenv, (jstring)p));
    }
    (*jenv)->DeleteLocalRef(jenv, val.l);
    return v;
}
static VALUE jbooleanarray2rv(JNIEnv* jenv, jvalue val)
{
    jboolean* p = (*jenv)->GetBooleanArrayElements(jenv, val.l, NULL);
    return call_conv(jenv, val, sizeof(jboolean), p, ba2rv,
			offsetof(struct JNINativeInterface_, ReleaseBooleanArrayElements));
}

/*
 * table that handles java primitive type.
 * index: according to enum PrimitiveType.
 */
static jprimitive_table jpcvt[] = {
    { "java/lang/Integer", "intValue", "()I", "(I)V", NULL, 0, 0, jint2rv, },
    { "java/lang/Long", "longValue", "()J", "(J)V", NULL, 0, 0, jlong2rv, },
    { "java/lang/Double", "doubleValue", "()D", "(D)V", NULL, 0, 0, jdouble2rv, },
    { "java/lang/Boolean", "booleanValue", "()Z", "(Z)Ljava/lang/Boolean;",
      NULL, 0, 0, jboolean2rv, },
    { "java/lang/Character", "charValue", "()C", NULL, NULL, 0, 0, jchar2rv, },
    { "java/lang/Short", "intValue", "()I", NULL, NULL, 0, 0, jint2rv, },
    { "java/lang/Byte", "intValue", "()I", NULL, NULL, 0, 0, jint2rv, },
    { "java/lang/Float", "doubleValue", "()D", NULL, NULL, 0, 0, jdouble2rv, },
};

/*
 * o will be released in this function.
 */
static VALUE jv2rv_withprim(JNIEnv* jenv, jobject o)
{
    jvalue jv;
    int i;
    jclass klass;
    jv.j = 0;
    if (!o)
	rb_raise(rb_eRuntimeError, "Object is NULL");
    klass = (*jenv)->GetObjectClass(jenv, o);
    for (i = PRM_INT; i < PRM_LAST; i++)
    {
        if ((*jenv)->IsSameObject(jenv, jpcvt[i].klass, klass))
	{
	    switch (*(jpcvt[i].to_prim_method))
	    {
	    case 'i':
		jv.i = (*jenv)->CallIntMethod(jenv, o, jpcvt[i].to_prim_id);
		break;
	    case 'b':
		jv.z = (*jenv)->CallBooleanMethod(jenv, o, jpcvt[i].to_prim_id);
		break;
	    case 'd':
		jv.d = (*jenv)->CallDoubleMethod(jenv, o, jpcvt[i].to_prim_id);
		break;
	    case 'c':
		jv.c = (*jenv)->CallCharMethod(jenv, o, jpcvt[i].to_prim_id);
		break;
            case 'l':
                jv.j = (*jenv)->CallLongMethod(jenv, o, jpcvt[i].to_prim_id);
                break;
	    default:
		rb_raise(rb_eRuntimeError, "no converter defined(%d)", i);
		break;
	    }
            (*jenv)->DeleteLocalRef(jenv, o);
	    return jpcvt[i].func(jenv, jv);
	}
    }
    if ((*jenv)->IsSameObject(jenv, j_string, klass))
    {
        return jstring2val(jenv, o);
    }
    jv.l = o;
    return jv2rv_r(jenv, jv);
}

/*
 * functions convert VALUE to jvalue
 */
static void rv2jv(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    jv->l = NULL;
}

static void rv2jbyte(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
	jv->b = (jbyte)NUM2INT(val);
}
static void rv2jchar(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
	jv->c = (jchar)NUM2INT(val);
}
static void rv2jdouble(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (release) return;
    switch (TYPE(val))
    {
    case T_FIXNUM:
	jv->d = NUM2INT(val);
	break;
    case T_FLOAT:
	jv->d = NUM2DBL(val);
	break;
    default:
	rb_raise(rb_eRuntimeError, "can't change to double");
	break;
    }
}
static void rv2jfloat(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (release) return;
    switch (TYPE(val))
    {
    case T_FIXNUM:
	jv->f = (float)NUM2INT(val);
	break;
    case T_FLOAT:
	jv->f = (float)NUM2DBL(val);
	break;
    default:
	rb_raise(rb_eRuntimeError, "can't change to float");
	break;
    }
}
static void rv2jint(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
	jv->i = NUM2INT(val);
}
static void rv2jlong(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (release) return;
    switch (TYPE(val))
    {
    case T_FIXNUM:
	jv->j = FIX2LONG(val);
	break;
    default:
#if HAVE_LONG_LONG
	jv->j = NUM2LL(val);
#else
	rb_raise(rb_eRuntimeError, "can't change to long");
#endif
	break;
    }
}
static void rv2jshort(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (release) return;
    if (TYPE(val) == T_FIXNUM)
    {
        int n = FIX2INT(val);
        if (abs(n) < 0x7fff)
        {
            jv->s = (short)n;
            return;
        }
    }
    rb_raise(rb_eRuntimeError, "can't change to short");
}
static void rv2jboolean(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
	jv->z = (RTEST(val)) ? JNI_TRUE : JNI_FALSE;
}
static void rv2jstring(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
    {
	if (TYPE(val) == T_DATA && IS_RJB_OBJECT(val))
	{
	    struct jvi_data* ptr;
	    Data_Get_Struct(val, struct jvi_data, ptr);
	    if ((*jenv)->IsInstanceOf(jenv, ptr->obj, j_string))
	    {
		jv->l = ptr->obj;
	    }
	    else
	    {
		jmethodID tostr;
		jstring js;
		tostr = (*jenv)->GetMethodID(jenv, ptr->klass, "toString", "()Ljava/lang/String;");
		rjb_check_exception(jenv, 0);
		js = (*jenv)->CallObjectMethod(jenv, ptr->obj, tostr);
		rjb_check_exception(jenv, 0);
		jv->l = js;
	    }
	}
	else
        {
            if (NIL_P(val))
            {
	        jv->l = NULL;
	    }
	    else
	    {
                val = exticonv_local_to_utf8(val);
                jv->l = (*jenv)->NewStringUTF(jenv, StringValuePtr(val));
	    }
	}
    }
    else
    {
	if (TYPE(val) == T_DATA)
	{
            if (IS_RJB_OBJECT(val))
	    {
		struct jvi_data* ptr;
		Data_Get_Struct(val, struct jvi_data, ptr);
		if ((*jenv)->IsInstanceOf(jenv, ptr->obj, j_string))
		{
		    return; /* never delete at this time */
		}
	    }
        }
	(*jenv)->DeleteLocalRef(jenv, jv->l);
    }
}

/*
 * psig may be NULL (from proxy/array call)
 */
static void rv2jobject(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
    {
	jv->l = NULL;
	if (val == Qtrue || val == Qfalse)
	{
	    jv->l = (*jenv)->CallStaticObjectMethod(jenv,
		    jpcvt[PRM_BOOLEAN].klass, jpcvt[PRM_BOOLEAN].ctr_id,
				    (val == Qtrue) ? JNI_TRUE : JNI_FALSE);
	}
	else if (NIL_P(val))
	{
	    /* no-op */
	}
	else if (FIXNUM_P(val))
	{
	    jvalue arg;
	    int idx = PRM_INT;
#if HAVE_LONG_LONG
	    arg.j = FIX2LONG(val);
	    if (arg.j < INT_MIN || arg.j > INT_MAX)
	    {
		idx = PRM_LONG;
	    }
#else
	    arg.i = FIX2LONG(val);
#endif
	    jv->l = (*jenv)->NewObject(jenv, jpcvt[idx].klass,
				       jpcvt[idx].ctr_id, arg);
	}
	else
	{
	    jvalue arg;
	    switch (TYPE(val))
	    {
	    case T_DATA:
                if (IS_RJB_OBJECT(val))
		{
                    /* TODO: check instanceof (class (in psig) ) */
		    struct jvi_data* ptr;
		    Data_Get_Struct(val, struct jvi_data, ptr);
		    jv->l = ptr->obj;
		}
		else if (rb_obj_class(val) == rjbb)
		{
		    struct rj_bridge* ptr;
		    Data_Get_Struct(val, struct rj_bridge, ptr);
		    jv->l = ptr->proxy;
		}
		else if (CLASS_INHERITED(rjbc, rb_obj_class(val)))
		{
		    struct jv_data* ptr;
		    Data_Get_Struct(val, struct jv_data, ptr);
		    jv->l = ptr->idata.obj;
		}
		break;
	    case T_STRING:
                if (psig && *psig == '[' && *(psig + 1) == 'B') {
                    jv->l = r2barray(jenv, val, NULL);
                } else {
                    rv2jstring(jenv, val, jv, NULL, 0);
                }
		break;
	    case T_FLOAT:
		arg.d = NUM2DBL(val);
		jv->l = (*jenv)->NewObject(jenv, jpcvt[PRM_DOUBLE].klass,
				       jpcvt[PRM_DOUBLE].ctr_id, arg.d);
		break;
	    case T_ARRAY:
		jv->l = r2objarray(jenv, val, "Ljava/lang/Object;");
		break;
#if HAVE_LONG_LONG
            case T_BIGNUM:
                arg.j = rb_big2ll(val);
                jv->l = (*jenv)->NewObject(jenv, jpcvt[PRM_LONG].klass,
				       jpcvt[PRM_LONG].ctr_id, arg);
                break;
#endif
            case T_OBJECT:
            default:
#if defined(DEBUG)
              {
                VALUE v = rb_funcall(val, rb_intern("inspect"), 0);
                fprintf(stderr, "rtype:%d, sig=%s\n", TYPE(val), psig);
                fprintf(stderr, "obj:%s\n", StringValueCStr(v));
                fflush(stderr);
              }
#endif
                rb_raise(rb_eRuntimeError, "can't convert to java type");
                break;
	    }
	}
    }
    else
    {
        switch (TYPE(val))
	{
        case T_STRING:
        case T_FLOAT:
        case T_ARRAY:
        case T_BIGNUM:
            if (jv->l) (*jenv)->DeleteLocalRef(jenv, jv->l);
            break;
        }
    }
}

static void check_fixnumarray(VALUE v)
{
    size_t i;
    size_t len = RARRAY_LEN(v);
    VALUE* p = RARRAY_PTR(v);
    /* check all fixnum (overflow is permit) */
    for (i = 0; i < len; i++)
    {
	if (!FIXNUM_P(*p++))
	{
	    rb_raise(rb_eRuntimeError, "array element must be a fixnum");
	}
    }
}

static jarray r2barray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_STRING)
    {
        ary = (*jenv)->NewByteArray(jenv, (jint)RSTRING_LEN(v));
	(*jenv)->SetByteArrayRegion(jenv, ary, 0, (jint)RSTRING_LEN(v),
				    (const jbyte*)RSTRING_PTR(v));
    }
    else if (TYPE(v) == T_ARRAY)
    {
	int i;
	jbyte* pb;
	check_fixnumarray(v);
	ary = (*jenv)->NewByteArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetByteArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jbyte)FIX2ULONG(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseByteArrayElements(jenv, ary, pb, 0);
    }
    if (!ary)
    {
	rb_raise(rb_eRuntimeError, "can't coerce to byte array");
    }
    return ary;
}

static jarray r2carray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jchar* pb;
	check_fixnumarray(v);
	ary = (*jenv)->NewCharArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetCharArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jchar)FIX2ULONG(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseCharArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to char array");
}

static jarray r2darray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jdouble* pb;
	ary = (*jenv)->NewDoubleArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetDoubleArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jdouble)rb_num2dbl(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseDoubleArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to double array");
}

static jarray r2farray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jfloat* pb;
	ary = (*jenv)->NewFloatArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetFloatArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jfloat)rb_num2dbl(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseFloatArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to float array");
}

static jarray r2iarray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jint* pb;
	check_fixnumarray(v);
	ary = (*jenv)->NewIntArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetIntArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jint)FIX2LONG(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseIntArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to int array");
}

static jarray r2larray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jlong* pb;
	ary = (*jenv)->NewLongArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetLongArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
#if HAVE_LONG_LONG
	    *(pb + i) = (jlong)rb_num2ll(RARRAY_PTR(v)[i]);
#else
	    *(pb + i) = (jlong)FIX2LONG(RARRAY_PTR(v)[i]);
#endif
	}
	(*jenv)->ReleaseLongArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to long array");
}

static jarray r2sarray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jshort* pb;
	check_fixnumarray(v);
	ary = (*jenv)->NewShortArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetShortArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i) = (jshort)FIX2LONG(RARRAY_PTR(v)[i]);
	}
	(*jenv)->ReleaseShortArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to short array");
}

static jarray r2boolarray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
	jboolean* pb;
	ary = (*jenv)->NewBooleanArray(jenv, (jint)RARRAY_LEN(v));
	pb = (*jenv)->GetBooleanArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    *(pb + i)
		= (!RTEST(RARRAY_PTR(v)[i]))
			? JNI_FALSE : JNI_TRUE;
	}
	(*jenv)->ReleaseBooleanArrayElements(jenv, ary, pb, 0);
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to boolean array");
}

static jarray r2voidarray(JNIEnv* jenv, VALUE v, const char* cls)
{
    rb_raise(rb_eRuntimeError, "void never arrayed");
}

static jarray r2objarray(JNIEnv* jenv, VALUE v, const char* cls)
{
    jarray ary = NULL;
    if (TYPE(v) == T_ARRAY)
    {
	int i;
        jclass jcls = NULL;
        char* p = strchr(cls, ';');
        if (p)
        {
            volatile VALUE clsname = rb_str_new(cls + 1, p - cls - 1); // skip first 'L'
            jcls = rjb_find_class(jenv, clsname);
        }
        ary = (*jenv)->NewObjectArray(jenv, (jint)RARRAY_LEN(v), (jcls) ? jcls : j_object, NULL);
	rjb_check_exception(jenv, 0);
	for (i = 0; i < RARRAY_LEN(v); i++)
	{
	    jvalue jv;
	    rv2jobject(jenv, RARRAY_PTR(v)[i], &jv, NULL, 0);
	    (*jenv)->SetObjectArrayElement(jenv, ary, i, jv.l);
	}
	return ary;
    }
    rb_raise(rb_eRuntimeError, "can't coerce to object array");
}

/*
 * Type convertion tables
 */
static const jconv_table jcvt[] = {
    { "byte", "B", rv2jbyte, jbyte2rv,
      jbytearray2rv, r2barray,
      offsetof(struct JNINativeInterface_, CallByteMethodA),
      offsetof(struct JNINativeInterface_, CallStaticByteMethodA), },
    { "char", "C", rv2jchar, jchar2rv,
      jchararray2rv, r2carray,
      offsetof(struct JNINativeInterface_, CallCharMethodA),
      offsetof(struct JNINativeInterface_, CallStaticCharMethodA), },
    { "double", "D", rv2jdouble, jdouble2rv,
      jdoublearray2rv, r2darray,
      offsetof(struct JNINativeInterface_, CallDoubleMethodA),
      offsetof(struct JNINativeInterface_, CallStaticDoubleMethodA), },
    { "float", "F", rv2jfloat, jfloat2rv,
      jfloatarray2rv, r2farray,
      offsetof(struct JNINativeInterface_, CallFloatMethodA),
      offsetof(struct JNINativeInterface_, CallStaticFloatMethodA), },
    { "int", "I", rv2jint, jint2rv,
      jintarray2rv, r2iarray,
      offsetof(struct JNINativeInterface_, CallIntMethodA),
      offsetof(struct JNINativeInterface_, CallStaticIntMethodA), },
    { "long", "J", rv2jlong, jlong2rv,
      jlongarray2rv, r2larray,
      offsetof(struct JNINativeInterface_, CallLongMethodA),
      offsetof(struct JNINativeInterface_, CallStaticLongMethodA), },
    { "short", "S", rv2jshort, jshort2rv,
      jshortarray2rv, r2sarray,
      offsetof(struct JNINativeInterface_, CallShortMethodA),
      offsetof(struct JNINativeInterface_, CallStaticShortMethodA), },
    { "boolean", "Z", rv2jboolean, jboolean2rv,
      jbooleanarray2rv, r2boolarray,
      offsetof(struct JNINativeInterface_, CallBooleanMethodA),
      offsetof(struct JNINativeInterface_, CallStaticBooleanMethodA), },
    { "void", "V", rv2jv, jvoid2rv,
      NULL, r2voidarray,
      offsetof(struct JNINativeInterface_, CallVoidMethodA),
      offsetof(struct JNINativeInterface_, CallStaticVoidMethodA), },
    { "java.lang.String", "Ljava.lang.String;", rv2jstring, jstring2rv,
      jstringarray2rv, r2objarray,
      offsetof(struct JNINativeInterface_, CallObjectMethodA),
      offsetof(struct JNINativeInterface_, CallStaticObjectMethodA), },
};

static void rv2jarray(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (*psig != '[')
    {
	rb_raise(rb_eRuntimeError, "argument signature not array");
    }
    if (release)
    {
        if (TYPE(val) == T_STRING && *(psig + 1) == 'B')
        {
            // copy array's contents into arg string
            jsize len = (*jenv)->GetArrayLength(jenv, jv->l);
            jbyte* p = (*jenv)->GetByteArrayElements(jenv, jv->l, NULL);
            if (len <= RSTRING_LEN(val))
            {
                memcpy(StringValuePtr(val), p, len);
            }
            else
            {
                VALUE src = rb_str_new((char*)p, len);
                rb_str_set_len(val, 0);
                rb_str_append(val, src);
            }
        }
        else if (TYPE(val) == T_ARRAY && *(psig + 1) == 'C')
        {
            int i;
            jsize len = (*jenv)->GetArrayLength(jenv, jv->l);
            jchar* p = (*jenv)->GetCharArrayElements(jenv, jv->l, NULL);
            rb_ary_clear(val);
            for (i = 0; i < len; i++, p++)
            {
                rb_ary_push(val, INT2FIX(*p));
            }
        }
	(*jenv)->DeleteLocalRef(jenv, jv->l);
    }
    else
    {
        jint i;
        jarray ja = NULL;
	if (NIL_P(val))
	{
	    /* no-op, null for an array */
	}
        else if (*(psig + 1) == '[')
        {
            if (TYPE(val) != T_ARRAY) {
                rb_raise(rb_eRuntimeError, "array's rank unmatch");
            }
            ja = (*jenv)->NewObjectArray(jenv, (jint)RARRAY_LEN(val), j_object, NULL);
            rjb_check_exception(jenv, 0);
            for (i = 0; i < (jint)RARRAY_LEN(val); i++)
            {
                jvalue jv;
                rv2jarray(jenv, RARRAY_PTR(val)[i], &jv, psig + 1, 0);
                (*jenv)->SetObjectArrayElement(jenv, ja, (jint)i, jv.l);
            }
        }
        else
        {
            R2JARRAY r2a = r2objarray;
            for (i = 0; i < (jint)COUNTOF(jcvt); i++)
            {
                if (*(psig + 1) == jcvt[i].jntype[0])
                {
                    r2a = jcvt[i].r2ja;
                    break;
                }
            }
            ja = r2a(jenv, val, psig + 1);
	}
	jv->l = ja;
    }
}

/*
 */
static R2J get_r2j(JNIEnv* jenv, jobject o, int* siglen,  char* sigp)
{
    size_t len, i;
    const char* cname;
    R2J result = NULL;
    jstring nm = (*jenv)->CallObjectMethod(jenv, o, rjb_class_getName);
    rjb_check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    if (*cname == '[')
    {
        if (siglen)
        {
            len = strlen(cname);
	    *siglen += (int)len;
	    strcpy(sigp, cname);
        }
	result = rv2jarray;
    }
    else
    {
        for (i = 0; i < COUNTOF(jcvt); i++)
        {
	    if (!strcmp(cname, jcvt[i].jtype))
	    {
                if (siglen)
                {
                    *siglen += (int)strlen(jcvt[i].jntype);
		    strcpy(sigp, jcvt[i].jntype);
                }
	        result = jcvt[i].r2j;
		break;
	    }
        }
        if (!result)
        {
            if (siglen)
	    {
	        *siglen += sprintf(sigp, "L%s;", cname);
	    }
            result = rv2jobject;
	}
    }
    rjb_release_string(jenv, nm, cname);
    return result;
}

static J2R get_arrayconv(const char* cname, char* pdepth)
{
    size_t i;
    size_t start;
    for (start = 1; *(cname + start) == '['; start++);
    *pdepth = (char)start;
    for (i = 0; i < COUNTOF(jcvt); i++)
    {
        if (*(cname + start) == jcvt[i].jntype[0])
        {
            if (jcvt[i].jntype[0] == 'L'
                && strncmp(cname + start, jcvt[i].jntype, strlen(jcvt[i].jntype)))
            {
                break;
            }
            return jcvt[i].ja2r;
        }
    }
    return &jarray2rv;
}

static J2R get_j2r(JNIEnv* jenv, jobject cls, char* psig, char* pdepth, char* ppsig, off_t* piv, int static_method)
{
    size_t i;
    J2R result = NULL;
    const char* cname;
    const char* jname = NULL;
    jstring nm = (*jenv)->CallObjectMethod(jenv, cls, rjb_class_getName);
    rjb_check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);

    if (*cname == '[')
    {
        result = get_arrayconv(cname, pdepth);
	jname = cname;
    }
    else
    {
	for (i = 0; i < COUNTOF(jcvt); i++)
	{
	    if (!strcmp(cname, jcvt[i].jtype))
	    {
		result = jcvt[i].j2r;
		*piv = (static_method) ? jcvt[i].jscall : jcvt[i].jcall;
		if (jcvt[i].jntype[0] != 'L')
		{
		    *psig = jcvt[i].jntype[0];
		}
		jname = jcvt[i].jntype;
		break;
	    }
	}
    }
    if (ppsig)
    {
	if (!jname)
	{
	    sprintf(ppsig, "L%s;", cname);
	}
	else
	{
	    strcpy(ppsig, jname);
	}
	java2jniname(ppsig);
    }
    rjb_release_string(jenv, nm, cname);
    return result;
}

static void setup_j2r(JNIEnv* jenv, jobject cls, struct cls_method* pm, int static_method)
{
    off_t iv = 0;
    J2R result = get_j2r(jenv, cls, &pm->basic.result_signature, &pm->basic.result_arraydepth, NULL, &iv, static_method);
    pm->result_convert = (result) ? result : jv2rv;
    if (iv)
    {
	pm->method = iv;
    }
    else
    {
	pm->method = (static_method)
	    ? offsetof(struct JNINativeInterface_, CallStaticObjectMethodA)
	    : offsetof(struct JNINativeInterface_, CallObjectMethodA);
    }
}

static void fill_convert(JNIEnv* jenv, struct cls_constructor* cls, jobjectArray tp, int count)
{
    int i, siglen;
    R2J* tbl = ALLOC_N(R2J, count);
    char** sig = (char**)ALLOCA_N(char*, count);
    char siga[256];
    cls->arg_convert = tbl;
    memset(tbl, 0, sizeof(R2J) * count);
    siglen = 0;
    for (i = 0; i < count; i++)
    {
	jobject o = (*jenv)->GetObjectArrayElement(jenv, tp, i);
	*(tbl + i) = get_r2j(jenv, o, &siglen, siga);
	*(sig + i) = ALLOCA_N(char, strlen(siga) + 1);
	strcpy(*(sig + i), siga);
    }
    cls->method_signature = ALLOC_N(char, siglen + 1);
    *(cls->method_signature) = 0;
    for (i = 0; i < count; i++)
    {
	strcat(cls->method_signature, *(sig + i));
    }
}

/*
 * create method info structure
 * m = instance of Method class
 * c = instance of the class
 */
static void setup_methodbase(JNIEnv* jenv, struct cls_constructor* pm,
			     jobjectArray parama, jsize pcount)
{
    pm->arg_count = pcount;
    pm->method_signature = NULL;
    pm->result_signature = 'O';
    pm->result_arraydepth = 0;
    pm->arg_convert = NULL;
    if (pcount)
    {
	fill_convert(jenv, pm, parama, pcount);
    }
}

static void register_methodinfo(struct cls_method* newpm, st_table* tbl)
{
    struct cls_method* pm;

    if (st_lookup(tbl, newpm->name, (st_data_t*)&pm))
    {
	newpm->next = pm->next;
	pm->next = newpm;
    }
    else
    {
	newpm->next = NULL;
	st_insert(tbl, newpm->name, (VALUE)newpm);
    }
}

static struct cls_method* clone_methodinfo(struct cls_method* pm)
{
    struct cls_method* result = ALLOC(struct cls_method);
    memcpy(result, pm, sizeof(struct cls_method));
    return result;
}

static int make_alias(const char* jname, char* rname)
{
    int ret = 0;
    while (*jname)
    {
        if (isupper(*jname))
        {
            *rname++ = '_';
            *rname++ = tolower(*jname++);
            ret = 1;
        }
        else
        {
            *rname++ = *jname++;
        }
    }
    *rname = '\0';
    return ret;
}

static void create_methodinfo(JNIEnv* jenv, st_table* tbl, jobject m, int static_method)
{
    struct cls_method* result;
    struct cls_method* pm;
    const char* jname;
    int alias;
    jstring nm;
    jobjectArray parama;
    jobject cls;
    jsize param_count;
    char* rname;

    result = ALLOC(struct cls_method);
    parama = (*jenv)->CallObjectMethod(jenv, m, getParameterTypes);
    rjb_check_exception(jenv, 0);
    param_count = (*jenv)->GetArrayLength(jenv, parama);
    rjb_check_exception(jenv, 0);
    setup_methodbase(jenv, &result->basic, parama, param_count);
    nm = (*jenv)->CallObjectMethod(jenv, m, method_getName);
    rjb_check_exception(jenv, 0);
    jname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    rname = ALLOCA_N(char, strlen(jname) * 2 + 8);
    alias = make_alias(jname, rname);
    result->name = rb_intern(jname);
    rjb_release_string(jenv, nm, jname);
    result->basic.id = (*jenv)->FromReflectedMethod(jenv, m);
    rjb_check_exception(jenv, 0);
    cls = (*jenv)->CallObjectMethod(jenv, m, getReturnType);
    rjb_check_exception(jenv, 0);
    setup_j2r(jenv, cls, result, static_method);
    (*jenv)->DeleteLocalRef(jenv, cls);
    result->static_method = static_method;
    register_methodinfo(result, tbl);
    /* create method alias */
    pm = NULL;
    if (strlen(rname) > 3
        && (*rname == 'g' || *rname == 's') && *(rname + 1) == 'e' && *(rname + 2) == 't')
    {
        pm = clone_methodinfo(result);
        if (*rname == 's')
        {
            if (result->basic.arg_count == 1)
            {
                rname += 3;
                strcat(rname, "=");
            }
        }
        else
        {
            rname += 3;
        }
        if (*rname == '_') rname++;
    }
    else if (strlen(rname) > 2 && result->basic.result_signature == 'Z'
             && *rname == 'i' && *(rname + 1) == 's')
    {
        pm = clone_methodinfo(result);
        rname += 2;
        if (*rname == '_') rname++;
        strcat(rname, "?");
    }
    else if (alias)
    {
        pm = clone_methodinfo(result);
    }
    if (pm)
    {
        pm->name = rb_intern(rname);
        register_methodinfo(pm, tbl);
    }
}

static void create_fieldinfo(JNIEnv* jenv, st_table* tbl, jobject f, int readonly, int static_field)
{
    struct cls_field* result;
    const char* jname;
    jstring nm;
    jobject cls;
    char sigs[256];
    off_t iv = 0;

    result = ALLOC(struct cls_field);
    memset(result, 0, sizeof(struct cls_field));
    nm = (*jenv)->CallObjectMethod(jenv, f, field_getName);
    rjb_check_exception(jenv, 0);
    jname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    result->name = rb_intern(jname);
    rjb_release_string(jenv, nm, jname);
    result->id = (*jenv)->FromReflectedField(jenv, f);
    rjb_check_exception(jenv, 0);
    cls = (*jenv)->CallObjectMethod(jenv, f, field_getType);
    rjb_check_exception(jenv, 0);
    result->value_convert = get_j2r(jenv, cls, &result->result_signature, &result->result_arraydepth, sigs, &iv, 0);
    result->arg_convert = get_r2j(jenv, cls, NULL, NULL);
    (*jenv)->DeleteLocalRef(jenv, cls);
    result->field_signature = ALLOC_N(char, strlen(sigs) + 1);
    strcpy(result->field_signature, sigs);
    if (!result->value_convert) result->value_convert = jv2rv;
    result->readonly = readonly;
    result->static_field = static_field;
    st_insert(tbl, result->name, (VALUE)result);
}

static void setup_constructors(JNIEnv* jenv, struct cls_constructor*** pptr, jobjectArray methods)
{
    int i;
    struct cls_constructor* pc;
    jsize mcount = (*jenv)->GetArrayLength(jenv, methods);
    struct cls_constructor** tbl = ALLOC_N(struct cls_constructor*, mcount + 1);
    *pptr = tbl;
    for (i = 0; i < mcount; i++)
    {
	jobjectArray parama;
	jsize pcount;
	jobject c = (*jenv)->GetObjectArrayElement(jenv, methods, i);
	rjb_check_exception(jenv, 0);
	pc = ALLOC(struct cls_constructor);
	tbl[i] = pc;
	parama = (*jenv)->CallObjectMethod(jenv, c, ctrGetParameterTypes);
	rjb_check_exception(jenv, 0);
	pcount = (*jenv)->GetArrayLength(jenv, parama);
	rjb_check_exception(jenv, 0);
	setup_methodbase(jenv, pc, parama, pcount);
	pc->id = (*jenv)->FromReflectedMethod(jenv, c);
	(*jenv)->DeleteLocalRef(jenv, c);
    }
    tbl[mcount] = NULL;
}

static void setup_methods(JNIEnv* jenv, st_table** tbl, st_table** static_tbl,
			  jobjectArray methods)
{
    int i;
    jint modifier;
    jsize mcount = (*jenv)->GetArrayLength(jenv, methods);
    *tbl = st_init_numtable_with_size(mcount);
    *static_tbl = st_init_numtable();
    for (i = 0; i < mcount; i++)
    {
	jobject m = (*jenv)->GetObjectArrayElement(jenv, methods, i);
	rjb_check_exception(jenv, 0);
	modifier = (*jenv)->CallIntMethod(jenv, m, method_getModifiers);
	if (!(modifier & ACC_STATIC))
	{
	    create_methodinfo(jenv, *tbl, m, 0);
	}
	else
	{
	    create_methodinfo(jenv, *static_tbl, m, 1);
	}
	(*jenv)->DeleteLocalRef(jenv, m);
    }
}

static void setup_fields(JNIEnv* jenv, st_table** tbl, jobjectArray flds)
{
    int i;
    jint modifier;
    jsize fcount = (*jenv)->GetArrayLength(jenv, flds);
    *tbl = st_init_numtable_with_size(fcount);
    for (i = 0; i < fcount; i++)
    {
	jobject f = (*jenv)->GetObjectArrayElement(jenv, flds, i);
	rjb_check_exception(jenv, 0);
	modifier = (*jenv)->CallIntMethod(jenv, f, field_getModifiers);
	create_fieldinfo(jenv, *tbl, f, modifier & ACC_FINAL, modifier & ACC_STATIC);
	(*jenv)->DeleteLocalRef(jenv, f);
    }
}

static void load_constants(JNIEnv* jenv, jclass klass, VALUE self, jobjectArray flds)
{
    int i;
    jint modifier;
    jsize fcount = (*jenv)->GetArrayLength(jenv, flds);
    for (i = 0; i < fcount; i++)
    {
	jobject f = (*jenv)->GetObjectArrayElement(jenv, flds, i);
	rjb_check_exception(jenv, 0);
	modifier = (*jenv)->CallIntMethod(jenv, f, field_getModifiers);
	rjb_check_exception(jenv, 0);
	if ((modifier & (ACC_PUBLIC | ACC_STATIC | ACC_FINAL)) == (ACC_PUBLIC | ACC_STATIC | ACC_FINAL))
	{
	    jstring nm;
	    const char* cname;
	    jobject cls;
	    char sig;
	    char depth;
	    off_t iv;
	    J2R j2r;
	    jvalue jv;
	    jfieldID jfid;
	    char sigs[256];
            char* pname;

	    /* constants make define directly in the ruby object */
	    cls = (*jenv)->CallObjectMethod(jenv, f, field_getType);
	    rjb_check_exception(jenv, 0);
	    iv = 0;
	    sig = depth = 0;
	    j2r = get_j2r(jenv, cls, &sig, &depth, sigs, &iv, 1);
	    if (!j2r) j2r = jv2rv;
	    (*jenv)->DeleteLocalRef(jenv, cls);
	    nm = (*jenv)->CallObjectMethod(jenv, f, field_getName);
	    rjb_check_exception(jenv, 0);
	    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
	    rjb_check_exception(jenv, 0);
	    jfid = (*jenv)->GetStaticFieldID(jenv, klass, cname, sigs);
	    rjb_check_exception(jenv, 0);
	    switch (sig)
	    {
	    case 'D':
		jv.d = (*jenv)->GetStaticDoubleField(jenv, klass, jfid);
		break;
	    case 'Z':
		jv.z = (*jenv)->GetStaticBooleanField(jenv, klass, jfid);
		break;
	    case 'B':
		jv.b = (*jenv)->GetStaticByteField(jenv, klass, jfid);
		break;
	    case 'F':
		jv.f = (*jenv)->GetStaticFloatField(jenv, klass, jfid);
		break;
	    case 'C':
		jv.c = (*jenv)->GetStaticCharField(jenv, klass, jfid);
		break;
	    case 'S':
		jv.s = (*jenv)->GetStaticShortField(jenv, klass, jfid);
		break;
	    case 'J':
		jv.j = (*jenv)->GetStaticLongField(jenv, klass, jfid);
		break;
	    case 'I':
		jv.i = (*jenv)->GetStaticIntField(jenv, klass, jfid);
		break;
	    default:
		jv.l = (*jenv)->GetStaticObjectField(jenv, klass, jfid);
		break;
	    }
            pname = (char*)cname;
	    if (!isupper(*cname))
	    {
 	        pname = ALLOCA_N(char, strlen(cname) + 1);
		strcpy(pname, cname);
		*pname = toupper(*pname);
		if (!isupper(*pname)
                    || rb_const_defined(rb_obj_class(self), rb_intern(pname)))
		{
	            pname = NULL;
		}
	    }
	    if (pname)
	    {
	        rb_define_const(rb_obj_class(self), pname, j2r(jenv, jv));
	    }
	    rjb_release_string(jenv, nm, cname);
	}
	(*jenv)->DeleteLocalRef(jenv, f);
    }
}

static void setup_metadata(JNIEnv* jenv, VALUE self, struct jv_data* ptr, VALUE classname)
{
    jmethodID mid;
    jobjectArray methods;
    jobjectArray flds;

    jclass klass = (*jenv)->GetObjectClass(jenv, ptr->idata.obj);
    ptr->idata.klass = (*jenv)->NewGlobalRef(jenv, klass);
    rjb_check_exception(jenv, 0);
    mid = (*jenv)->GetMethodID(jenv, klass, "getMethods", "()[Ljava/lang/reflect/Method;");
    rjb_check_exception(jenv, 0);
    methods = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    rjb_check_exception(jenv, 0);
    setup_methods(jenv, &ptr->idata.methods, &ptr->static_methods, methods);
    mid = (*jenv)->GetMethodID(jenv, klass, "getConstructors", "()[Ljava/lang/reflect/Constructor;");
    rjb_check_exception(jenv, 0);
    methods = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    rjb_check_exception(jenv, 0);
    setup_constructors(jenv, &ptr->constructors, methods);
    mid = (*jenv)->GetMethodID(jenv, klass, "getFields", "()[Ljava/lang/reflect/Field;");
    rjb_check_exception(jenv, 0);
    flds = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    rjb_check_exception(jenv, 0);
    setup_fields(jenv, &ptr->idata.fields, flds);

    register_class(self, classname);
    load_constants(jenv, ptr->idata.obj, self, flds);
}

/*
 * load Java Virtual Machine
 * def load(class_path = '', vmargs = [])
 * class_path: passes for the class dir and jar name
 * vmargs: strng array of vmarg (such as -Xrs)
 *
 * change in rjb 0.1.7, omit first argument for JNI version.
 *  because I misunderstood the number means (JVM but JNI).
 */
static VALUE rjb_s_load(int argc, VALUE* argv, VALUE self)
{
    JNIEnv* jenv;
    JavaVMInitArgs vm_args;
    jint res;
    VALUE classpath;
    VALUE user_path;
    VALUE vm_argv;
    char* userpath;
    ID stradd = rb_intern("<<");
    ID pathsep = rb_intern("PATH_SEPARATOR");
    int i;
    jclass jmethod;
    jclass jfield;
    jclass jconstructor;

    if (rjb_jvm)
    {
	return Qnil;
    }

    memset(&vm_args, 0, sizeof(vm_args));
    vm_args.version = JNI_VERSION_1_4;
    rb_scan_args(argc, argv, "02", &user_path, &vm_argv);
    if (!NIL_P(user_path))
    {
        Check_Type(user_path, T_STRING);
    }
    else
    {
	user_path = rb_str_new2(".");
    }
    classpath = rb_cvar_get(rjb, cvar_classpath);
    for (i = 0; i < RARRAY_LEN(classpath); i++)
    {
        rb_funcall(user_path, stradd, 1, rb_const_get(rb_cFile, pathsep));
        rb_funcall(user_path, stradd, 1, rb_ary_entry(classpath, 0));
    }
    userpath = StringValueCStr(user_path);

    if (!NIL_P(vm_argv))
    {
        Check_Type(vm_argv, T_ARRAY);
    }
    jenv = NULL;
    res = rjb_create_jvm(&jenv, &vm_args, userpath, vm_argv);
    if (res < 0)
    {
	rjb_jvm = NULL;
	rb_raise(rb_eRuntimeError, "can't create Java VM");
    } else {
        main_jenv = jenv;
    }

    RJB_FIND_CLASS(jconstructor, "java/lang/reflect/Constructor");
    RJB_LOAD_METHOD(ctrGetParameterTypes, jconstructor, "getParameterTypes", "()[Ljava/lang/Class;");
    RJB_FIND_CLASS(jmethod, "java/lang/reflect/Method");
    RJB_LOAD_METHOD(method_getModifiers, jmethod, "getModifiers", "()I");
    RJB_LOAD_METHOD(method_getName, jmethod, "getName", "()Ljava/lang/String;");
    RJB_LOAD_METHOD(getParameterTypes, jmethod, "getParameterTypes", "()[Ljava/lang/Class;");
    RJB_LOAD_METHOD(getReturnType, jmethod, "getReturnType", "()Ljava/lang/Class;");
    rjb_check_exception(jenv, 1);

    RJB_FIND_CLASS(jfield, "java/lang/reflect/Field");
    RJB_LOAD_METHOD(field_getModifiers, jfield, "getModifiers", "()I");
    RJB_LOAD_METHOD(field_getName, jfield, "getName", "()Ljava/lang/String;");
    RJB_LOAD_METHOD(field_getType, jfield, "getType", "()Ljava/lang/Class;");
    rjb_check_exception(jenv, 1);

    RJB_HOLD_CLASS(j_class, "java/lang/Class");
    RJB_LOAD_METHOD(rjb_class_getName, j_class, "getName", "()Ljava/lang/String;");
    rjb_check_exception(jenv, 1);

    RJB_HOLD_CLASS(rjb_j_throwable, "java/lang/Throwable");
    RJB_LOAD_METHOD(rjb_throwable_getMessage, rjb_j_throwable, "getMessage", "()Ljava/lang/String;");
    rjb_check_exception(jenv, 1);

    RJB_HOLD_CLASS(j_string, "java/lang/String");
    RJB_LOAD_METHOD(str_tostring, j_string, "toString", "()Ljava/lang/String;");
    rjb_check_exception(jenv, 1);

    RJB_HOLD_CLASS(j_object, "java/lang/Object");
    rjb_check_exception(jenv, 1);

    RJB_HOLD_CLASS(j_url, "java/net/URL");
    RJB_LOAD_METHOD(url_new, j_url, "<init>", "(Ljava/lang/String;)V");
    rjb_check_exception(jenv, 1);

    for (i = PRM_INT; i < PRM_LAST; i++)
    {
	jclass klass;
        RJB_FIND_CLASS(klass, jpcvt[i].classname);
	if (i == PRM_BOOLEAN)
	{
            RJB_LOAD_STATIC_METHOD(jpcvt[i].ctr_id, klass, "valueOf", jpcvt[i].ctrsig);
	}
	else if (jpcvt[i].ctrsig)
	{
            RJB_LOAD_METHOD(jpcvt[i].ctr_id, klass, "<init>", jpcvt[i].ctrsig);
	}
        RJB_LOAD_METHOD(jpcvt[i].to_prim_id, klass,
				   jpcvt[i].to_prim_method, jpcvt[i].prmsig);

        jpcvt[i].klass = (*jenv)->NewGlobalRef(jenv, klass);
    }

    jklass = import_class(jenv, j_class, rb_str_new2("java.lang.Class"));
    rb_define_method(rb_singleton_class(jklass), "forName", rjb_class_forname, -1);
    rb_define_alias(rb_singleton_class(jklass), "for_name", "forName");
    rb_gc_register_address(&jklass);

    return Qnil;
}

/*
 * load Java Virtual Machine with default arguments.
 */
VALUE rjb_load_vm_default()
{
    if (rjb_jvm) return Qfalse;

    rb_warning("Rjb::implicit jvm loading");
    return rjb_s_load(0, NULL, 0);
}

/*
 * common prelude
 */
JNIEnv* rjb_prelude()
{
    JNIEnv* jenv = NULL;
    rjb_load_vm_default();
    jenv = rjb_attach_current_thread();
    (*jenv)->ExceptionClear(jenv);
    return jenv;
}

jobject get_systemloader(JNIEnv* jenv)
{
    if (!j_classloader)
    {
        RJB_HOLD_CLASS(j_classloader, "java/lang/ClassLoader");
        RJB_LOAD_STATIC_METHOD(get_system_classloader, j_classloader,
                           "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        rjb_check_exception(jenv, 1);
    }
    return (*jenv)->CallStaticObjectMethod(jenv, j_classloader, get_system_classloader);
}

static jobject get_class_loader(JNIEnv* jenv)
{
    return (url_loader) ? url_loader : get_systemloader(jenv);
}

/*
 * unload Java Virtual Machine
 *
 * def unload()
 *   classes.clear
 *   unload(jvm)
 * end
 */
static int clear_classes(VALUE key, VALUE val, VALUE dummy)
{
    return ST_DELETE;
}
static VALUE rjb_s_unload(int argc, VALUE* argv, VALUE self)
{
    int result = 0;
#if defined(HAVE_RB_HASH_FOREACH) || defined(RUBINIUS)
	rb_hash_foreach(rjb_loaded_classes, clear_classes, 0);
#else
#if defined(RHASH_TBL)
    st_foreach(RHASH_TBL(rjb_loaded_classes), clear_classes, 0);
#else
    st_foreach(RHASH(rjb_loaded_classes)->tbl, clear_classes, 0);
#endif
#endif

    if (rjb_jvm)
    {
        JNIEnv* jenv = rjb_attach_current_thread();
        (*jenv)->ExceptionClear(jenv);
        result = (*rjb_jvm)->DestroyJavaVM(rjb_jvm);
        rjb_jvm = NULL;
        rjb_unload_vm();
    }
    return INT2NUM(result);
}

static VALUE rjb_s_loaded(VALUE self)
{
    return (rjb_jvm) ? Qtrue : Qfalse;
}

/*
 * return all classes that were already loaded.
 * this method simply returns the global hash,
 * but it's safe because the hash was frozen.
 */
static VALUE rjb_s_classes(VALUE self)
{
    return rjb_loaded_classes;
}

/**
 * For JRuby conpatible option
 */
static VALUE rjb_s_set_pconversion(VALUE self, VALUE val)
{
    primitive_conversion = (RTEST(val)) ? Qtrue : Qfalse;
    return val;
}

/**
 * For JRuby conpatible option
 */
static VALUE rjb_s_get_pconversion(VALUE self)
{
    return primitive_conversion;
}


/*
 * free java class
 */
#if 0
static void free_constructor(struct cls_constructor* p)
{
    free(p->arg_convert);
    free(p->method_signature);
}
static int free_method_item(ID key, struct cls_method* pm, int dummy)
{
    for (; pm; pm = pm->next)
    {
	free_constructor(&pm->basic);
    }
    return ST_CONTINUE;
}
#endif

/*
 * finalize Object instance
 */
static VALUE rjb_delete_ref(struct jvi_data* ptr)
{
    JNIEnv* jenv = rjb_attach_current_thread();
    if (jenv)
    {
	(*jenv)->DeleteGlobalRef(jenv, ptr->obj);
    }
    return Qnil;
}

/*
 * finalize Bridge instance
 */
static VALUE rj_bridge_free(struct rj_bridge* ptr)
{
    JNIEnv* jenv = rjb_attach_current_thread();
    if (jenv)
    {
        (*jenv)->DeleteLocalRef(jenv, ptr->proxy);
        (*jenv)->DeleteLocalRef(jenv, ptr->bridge);
    }
    return Qnil;
}

/*
 * mark wrapped object in the Bridge
 */
static void rj_bridge_mark(struct rj_bridge* ptr)
{
    rb_gc_mark(ptr->wrapped);
}

/*
 * finalize Class instance
 */
static VALUE rjb_s_free(struct jv_data* ptr)
{
    /* class never delete
    JNIEnv* jenv = rjb_attach_current_thread();
    struct cls_constructor** c;

    rjb_delete_ref(&ptr->idata);
    if (ptr->constructors)
    {
	for (c = ptr->constructors; *c; c++)
	{
	    free_constructor(*c);
	}
    }
    free(ptr->constructors);
    if (ptr->idata.methods)
    {
	st_foreach(ptr->idata.methods, (int(*)())free_method_item, 0);
	st_free_table(ptr->idata.methods);
    }
    (*jenv)->DeleteGlobalRef(jenv, ptr->idata.klass);
    st_delete(RHASH(rjb_loaded_classes)->tbl, clsname, NULL);
    */
    return Qnil;
}

/*
 * create new instance of this class
 */
static VALUE createinstance(JNIEnv* jenv, int argc, VALUE* argv,
	    VALUE self, struct cls_constructor* pc)
{
    int i;
    char* psig = pc->method_signature;
    jobject obj = NULL;
    VALUE result;
    struct jv_data* jklass;
    struct jvi_data* org;
    jvalue* args = (argc) ? ALLOCA_N(jvalue, argc) : NULL;

    Data_Get_Struct(self, struct jv_data, jklass);
    org = &jklass->idata;

    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pc->arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 0);
	psig = next_sig(psig);
	rjb_check_exception(jenv, 1);
    }
    obj = (*jenv)->NewObjectA(jenv, org->obj, pc->id, args);
    if (!obj)
    {
	rjb_check_exception(jenv, 1);
    }
    psig = pc->method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pc->arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 1);
	psig = next_sig(psig);
    }

    result = register_instance(jenv, self, jklass, obj);
    (*jenv)->DeleteLocalRef(jenv, obj);
    return result;
}

static VALUE import_class(JNIEnv* jenv, jclass jcls, VALUE clsname)
{
    VALUE v;
    VALUE rexp;
    struct jv_data* ptr;
    char* pclsname = StringValueCStr(clsname);
    char* nm = ALLOCA_N(char, strlen(pclsname) + 1);
    strcpy(nm, pclsname);
    *nm = toupper(*nm);
    for (pclsname = nm; *pclsname; pclsname++)
    {
        if (*pclsname == '.')
	{
	    *pclsname = '_';
	}
    }
    rexp = rb_define_class_under(rjb, nm, rjbc);
    ptr = ALLOC(struct jv_data);
    memset(ptr, 0, sizeof(struct jv_data));
    v = Data_Wrap_Struct(rexp, NULL, rjb_s_free, ptr);
    ptr->idata.obj = (*jenv)->NewGlobalRef(jenv, jcls);
    setup_metadata(jenv, v, ptr, clsname);
    return v;
}

static VALUE rjb_a_initialize(VALUE self, VALUE proc)
{
    return rb_ivar_set(self, anonymousblock, proc);
}

static VALUE rjb_a_missing(int argc, VALUE* argv, VALUE self)
{
    VALUE proc = rb_ivar_get(self, anonymousblock);
    return rb_funcall2(proc, id_call, argc, argv);
}

static VALUE rjb_i_prepare_proxy(VALUE self)
{
    return rb_funcall(self, rb_intern("instance_eval"), 1,
                      rb_str_new2("instance_eval(&" USER_INITIALIZE ")"));
}

static VALUE register_instance(JNIEnv* jenv, VALUE klass, struct jv_data* org, jobject obj)
{
    volatile VALUE v;
    VALUE iproc;
    struct jvi_data* ptr = ALLOC(struct jvi_data);
    memset(ptr, 0, sizeof(struct jvi_data));
    v = Data_Wrap_Struct(rjbi, NULL, rjb_delete_ref, ptr);
    ptr->klass = org->idata.obj;
    ptr->obj = (*jenv)->NewGlobalRef(jenv, obj);
    ptr->methods = org->idata.methods;
    ptr->fields = org->idata.fields;
    iproc = rb_ivar_get(klass, user_initialize);
    if (iproc != Qnil)
    {
        rb_ivar_set(v, user_initialize, iproc);
        rb_funcall(v, rb_intern("_prepare_proxy"), 0, 0);
    }
    rb_funcall(v, initialize_proxy, 0, 0);
    return v;
}

#define IS_BYTE(b) (!((b) & 0xffffff00))
#define IS_SHORT(b) (!((b) & 0xffff0000))
/*
 * temporary signature check
 * return !0 if found
 */
#define UNMATCHED 0
#define SATISFIED 1
#define SOSO 2
#define PREFERABLE 3
static int check_rtype(JNIEnv* jenv, VALUE* pv, char* p)
{
    size_t i;
    char* pcls = NULL;
    if (*p == 'L')
    {
        char* pt = strchr(p, ';');
	if (pt) {
	    size_t len = pt - p - 1;
	    pcls = ALLOCA_N(char, len + 1);
            strncpy(pcls, p + 1, len);
	    *(pcls + len) = '\0';
	}
    }
    if (pcls && !strcmp("java.lang.Object", pcls))
    {
        return SATISFIED;
    }
    switch (TYPE(*pv))
    {
    case T_FIXNUM:
        if (strchr("IJ", *p)) return SOSO;
        return strchr("BCDFS", *p) != NULL;
    case T_BIGNUM:
        return strchr("BCDFIJS", *p) != NULL;
    case T_FLOAT:
        if (*p == 'D') return SOSO;
        if (*p == 'F') return SATISFIED;
        return UNMATCHED;
    case T_STRING:
        if (pcls && (!strcmp("java.lang.String", pcls)
                     || !strcmp("java.lang.CharSequence", pcls)))
        {
            return PREFERABLE;
        }
        else if (*p == '[' && *(p + 1) == 'B')
        {
            return SATISFIED;
        }
        return UNMATCHED;
    case T_TRUE:
    case T_FALSE:
        return (*p == 'Z') ? SOSO : UNMATCHED;
    case T_ARRAY:
        if (*p == '[')
        {
            int weight = (*(p + 1) == 'C') ? SOSO : PREFERABLE;
            size_t len = RARRAY_LEN(*pv);
            VALUE* ppv = RARRAY_PTR(*pv);
            unsigned long ul;
            if (!strchr("BCSI", *(p + 1))) return SOSO; // verify later
            if (len > 32) len = 32;
            for (i = 0; i < len; i++, ppv++)
            {
                if (!FIXNUM_P(*ppv))
                {
                    return UNMATCHED;
                }
                ul = (unsigned long)FIX2LONG(*ppv);
                if (*(p + 1) == 'B')
                {
                    if (!IS_BYTE(ul)) return UNMATCHED;
                }
                else if (*(p + 1) == 'C' || *(p + 1) == 'S')
                {
                    if (!IS_SHORT(ul)) return UNMATCHED;
                }
            }
            return weight;
        }
        return UNMATCHED;
    case T_DATA:
    case T_OBJECT:
        if (IS_RJB_OBJECT(*pv) && pcls)
	{
	    /* imported object */
	    jclass cls;
            struct jvi_data* ptr;
	    int result = 0;
            if (!strcmp("java.lang.String", pcls)) return SATISFIED;
	    Data_Get_Struct(*pv, struct jvi_data, ptr);
            RJB_FIND_CLASS(cls, java2jniname(pcls));
	    if (cls)
            {
	        result = (cls && (*jenv)->IsInstanceOf(jenv, ptr->obj, cls));
	        (*jenv)->DeleteLocalRef(jenv, cls);
	    }
	    return (result) ? PREFERABLE : UNMATCHED;
	} else if (pcls) {
            VALUE blockobj = rb_class_new_instance(1, pv, rjba);
            *pv = rjb_s_bind(rjbb, blockobj, rb_str_new2(pcls));
        }
	/* fall down to the next case */
    default:
        if (pcls || *p == '[')
        {
            return SATISFIED;
        }
	return UNMATCHED;
    }
}

/*
 * new instance with signature
 */
static VALUE rjb_newinstance_s(int argc, VALUE* argv, VALUE self)
{
    VALUE vsig, rest;
    char* sig;
    VALUE ret = Qnil;
    struct jv_data* ptr;
    int found = 0;
    JNIEnv* jenv = rjb_prelude();

    rb_scan_args(argc, argv, "1*", &vsig, &rest);
    sig = StringValueCStr(vsig);
    Data_Get_Struct(self, struct jv_data, ptr);
    if (ptr->constructors)
    {
	struct cls_constructor** pc = ptr->constructors;
	for (pc = ptr->constructors; *pc; pc++)
	{
	    if ((*pc)->arg_count == argc - 1
		&& !strcmp(sig, (*pc)->method_signature))
	    {
	        found = 1;
		ret = createinstance(jenv, argc - 1, argv + 1, self, *pc);
		break;
	    }
	}
    }
    if (!found) {
	rb_raise(rb_eRuntimeError, "Constructor not found");
    }
    return ret;
}

static VALUE rjb_newinstance(int argc, VALUE* argv, VALUE self)
{
    VALUE ret = Qnil;
    struct jv_data* ptr;
    struct cls_constructor** pc;
    struct cls_constructor** found_pc = NULL;
    int found = 0;
    int weight = 0;
    int cweight;
    JNIEnv* jenv = rjb_prelude();

    Data_Get_Struct(self, struct jv_data, ptr);

    if (ptr->constructors)
    {
        int i;
	char* psig;
	for (pc = ptr->constructors; *pc; pc++)
	{
            found = 0;
	    if ((*pc)->arg_count == argc)
	    {
                found = 1;
                cweight = 0;
		psig = (*pc)->method_signature;
		for (i = 0; i < argc; i++)
		{
                    int w = check_rtype(jenv, argv + i, psig);
		    if (!w)
                    {
		        found = 0;
			break;
		    }
                    cweight += w;
		    psig = next_sig(psig);
		}
	    }
            if (found)
            {
                if (cweight > weight || weight == 0)
                {
                    found_pc = pc;
                    weight = cweight;
                }
            }
	}
        if (found_pc)
        {
#if defined(DEBUG)
            fprintf(stderr, "ctr sig=%s\n", (*found_pc)->method_signature);
            fflush(stderr);
#endif
            ret = createinstance(jenv, argc, argv, self, *found_pc);
        }
    }
    if (!found_pc) {
	rb_raise(rb_eRuntimeError, "Constructor not found");
    }
    return ret;
}

/*
 * find java class using added classloader
 */
jclass rjb_find_class_by_name(JNIEnv* jenv, const char* name)
{
    jclass cls;
    if (url_loader)
    {
        jvalue v;
        char* binname = ALLOCA_N(char, strlen(name) + 32);
        strcpy(binname, name);
        v.l = (*jenv)->NewStringUTF(jenv, jniname2java(binname));
        cls = (*jenv)->CallObjectMethod(jenv, url_loader, rjb_load_class, v);
        (*jenv)->DeleteLocalRef(jenv, v.l);
    }
    else
    {
        cls = (*jenv)->FindClass(jenv, name);
    }
    return cls;
}

/*
 * find java class from classname
 */
jclass rjb_find_class(JNIEnv* jenv, VALUE name)
{
    char* cname;
    char* jnicls;

    Check_Type(name, T_STRING);
    cname = StringValueCStr(name);
    jnicls = ALLOCA_N(char, strlen(cname) + 1);
    strcpy(jnicls, cname);
    return rjb_find_class_by_name(jenv, java2jniname(jnicls));
}

/*
 * get specified method signature
 */
static VALUE get_signatures(VALUE mname, st_table* st)
{
    VALUE ret;
    struct cls_method* pm;
    ID rmid = rb_to_id(mname);

    if (!st_lookup(st, rmid, (st_data_t*)&pm))
    {
        const char* tname = rb_id2name(rmid);
        rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s'", tname);
    }
    ret = rb_ary_new();
    for (; pm; pm = pm->next)
    {
        if (pm->basic.method_signature) {
            rb_ary_push(ret, rb_str_new2(pm->basic.method_signature));
        } else {
            rb_ary_push(ret, Qnil);
        }
    }
    return ret;
}

static VALUE rjb_get_signatures(VALUE self, VALUE mname)
{
    struct jv_data* ptr;

    Data_Get_Struct(self, struct jv_data, ptr);
    return get_signatures(mname, ptr->idata.methods);
}

static VALUE rjb_get_static_signatures(VALUE self, VALUE mname)
{
    struct jv_data* ptr;

    Data_Get_Struct(self, struct jv_data, ptr);
    return get_signatures(mname, ptr->static_methods);
}

static VALUE rjb_get_ctor_signatures(VALUE self)
{
    VALUE ret;
    struct jv_data* ptr;
    struct cls_constructor** pc;

    Data_Get_Struct(self, struct jv_data, ptr);
    ret = rb_ary_new();
    if (ptr->constructors)
    {
        for (pc = ptr->constructors; *pc; pc++)
        {
            const char* sig = (*pc)->method_signature;
            rb_ary_push(ret, rb_str_new2(sig ? sig : ""));
        }
    }
    return ret;
}

/*
 * jclass Rjb::bind(rbobj, interface_name)
 */
static VALUE rjb_s_bind(VALUE self, VALUE rbobj, VALUE itfname)
{
    VALUE result = Qnil;
    jclass itf;
    JNIEnv* jenv = rjb_prelude();

    itf = rjb_find_class(jenv, itfname);
    rjb_check_exception(jenv, 1);
    if (itf)
    {
	struct rj_bridge* ptr = ALLOC(struct rj_bridge);
	memset(ptr, 0, sizeof(struct rj_bridge));
	ptr->bridge = (*jenv)->NewGlobalRef(jenv,
                                   (*jenv)->AllocObject(jenv, rjb_rbridge));
	if (!ptr->bridge)
	{
	    free(ptr);
	    rjb_check_exception(jenv, 1);
	    return Qnil;
	}
	ptr->proxy = (*jenv)->CallObjectMethod(jenv, ptr->bridge,
					       rjb_register_bridge, itf);
        ptr->proxy = (*jenv)->NewGlobalRef(jenv, ptr->proxy);
	ptr->wrapped = rbobj;
	result = Data_Wrap_Struct(rjbb, rj_bridge_mark, rj_bridge_free, ptr);
	rb_ary_push(proxies, result);
        rb_ivar_set(result, rb_intern("@wrapped"), rbobj);
    }
    return result;
}

/*
 * Rjb's class is not Class but Object, so add class_eval for the Java class.
 */
static VALUE rjb_class_eval(int argc, VALUE* argv, VALUE self)
{
    if (rb_block_given_p())
    {
        rb_ivar_set(self, user_initialize, rb_block_proc());
    }
    return self;
}

static VALUE rjb_s_impl(VALUE self)
{
    VALUE obj;
    VALUE proc;
    rb_need_block();
    proc = rb_block_proc();
    obj = rb_class_new_instance(1, &proc, rjba);
    return rjb_s_bind(rjbb, obj, rb_funcall(self, rb_intern("name"), 0));
}


/*
 * jclass Rjb::bind(rbobj, interface_name)
 */
static VALUE rjb_s_unbind(VALUE self, VALUE rbobj)
{
#if defined(RUBINIUS)
    return rb_funcall(proxies, rb_intern("delete"), 1, rbobj);
#else
    return rb_ary_delete(proxies, rbobj);
#endif
}

/*
 * Jclass Rjb::import(classname)
 */
static VALUE rjb_s_import(VALUE self, VALUE clsname)
{
    JNIEnv* jenv;
    jclass jcls;
    VALUE v = rb_hash_aref(rjb_loaded_classes, clsname);
    if (v != Qnil)
    {
	return v;
    }

    jenv = rjb_prelude();
    jcls = rjb_find_class(jenv, clsname);
    if (!jcls)
    {
	rjb_check_exception(jenv, 0);
	rb_raise(rb_eRuntimeError, "`%s' not found", StringValueCStr(clsname));
    }
    v = import_class(jenv, jcls, clsname);
    return v;
}

static void register_class(VALUE self, VALUE clsname)
{
    rb_define_singleton_method(self, "new", rjb_newinstance, -1);
    rb_define_singleton_method(self, "new_with_sig", rjb_newinstance_s, -1);
    rb_define_singleton_method(self, "class_eval", rjb_class_eval, -1);
    rb_define_singleton_method(self, "sigs", rjb_get_signatures, 1);
    rb_define_singleton_method(self, "static_sigs", rjb_get_static_signatures, 1);
    rb_define_singleton_method(self, "ctor_sigs", rjb_get_ctor_signatures, 0);
    rb_ivar_set(self, user_initialize, Qnil);
    /*
     * the hash was frozen, so it need to call st_ func directly.
     */

#if defined(HAVE_RB_HASH_ASET) || defined(RUBINIUS)
	rb_hash_aset(rjb_loaded_classes, clsname, self);
#else
#ifdef RHASH_TBL
    st_insert(RHASH_TBL(rjb_loaded_classes), clsname, self);
#else
    st_insert(RHASH(rjb_loaded_classes)->tbl, clsname, self);
#endif
#endif
}

static jobject conv_jarname_to_url(JNIEnv* jenv, VALUE jarname)
{
    jvalue arg;
    jobject url;
#if defined(DOSISH)
    size_t len;
#endif
    char* jarp;
    char* urlp;

    SafeStringValue(jarname);
    jarp = StringValueCStr(jarname);
    urlp = ALLOCA_N(char, strlen(jarp) + 32);
    if (strncmp(jarp, "http:", 5) && strncmp(jarp, "https:", 6))
    {
#if defined(DOSISH)
        if (strlen(jarp) > 1 && jarp[1] == ':')
        {
            sprintf(urlp, "file:///%s", jarp);
        }
        else
#endif
        {
            sprintf(urlp, "file://%s", jarp);
        }
    }
    else
    {
        strcpy(urlp, jarp);
    }
#if defined(DOSISH)
    for (len = 0; len < strlen(urlp); len++)
    {
        if (urlp[len] == '\\')
        {
            urlp[len] = '/';
        }
    }
#endif
    arg.l = (*jenv)->NewStringUTF(jenv, urlp);
    rjb_check_exception(jenv, 0);
    url = (*jenv)->NewObject(jenv, j_url, url_new, arg);
    rjb_check_exception(jenv, 0);
    return url;
}

/*
 * Rjb::add_classpath(jarname)
 */
static VALUE rjb_s_add_classpath(VALUE self, VALUE jarname)
{
    VALUE cpath = rb_cvar_get(self, cvar_classpath);
    SafeStringValue(jarname);
    rb_ary_push(cpath, jarname);
    return cpath;
}

/*
 * Rjb::add_jar(jarname)
 */
static VALUE rjb_s_add_jar(VALUE self, VALUE jarname)
{
    size_t i;
    JNIEnv* jenv;
    size_t count;
    jvalue args[2];

    if (rb_type(jarname) != T_ARRAY)
    {
        SafeStringValue(jarname);
        count = 0;
    }
    else
    {
        count = RARRAY_LEN(jarname);
    }
    jenv = rjb_prelude();
    if (!j_url_loader)
    {
        j_url_loader = (*jenv)->NewGlobalRef(jenv,
                                             (*jenv)->FindClass(jenv, "java/net/URLClassLoader"));
        RJB_LOAD_METHOD(rjb_load_class, j_url_loader, "loadClass",
                        "(Ljava/lang/String;)Ljava/lang/Class;");
        RJB_LOAD_METHOD(url_loader_new, j_url_loader, "<init>",
                        "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
        RJB_LOAD_METHOD(url_geturls, j_url_loader, "getURLs",
                        "()[Ljava/net/URL;");
        RJB_LOAD_METHOD(url_add_url, j_url_loader, "addURL",
                        "(Ljava/net/URL;)V");
    }
    if (!url_loader)
    {
        args[0].l = (*jenv)->NewObjectArray(jenv, (jsize)((count == 0) ? 1 : count), j_url, NULL);
        rjb_check_exception(jenv, 0);
        if (!count)
        {
            (*jenv)->SetObjectArrayElement(jenv, args[0].l, 0,
                                       conv_jarname_to_url(jenv, jarname));
        }
        else
        {
            for (i = 0; i < count; i++) {
                (*jenv)->SetObjectArrayElement(jenv, args[0].l, (jint)i,
                                       conv_jarname_to_url(jenv, rb_ary_entry(jarname, i)));
            }
        }
        rjb_check_exception(jenv, 0);
        args[1].l = get_class_loader(jenv);
        url_loader = (*jenv)->NewObjectA(jenv, j_url_loader, url_loader_new, args);
        rjb_check_exception(jenv, 0);
        (*jenv)->NewGlobalRef(jenv, url_loader);
        (*jenv)->DeleteLocalRef(jenv, args[0].l);
    }
    else
    {
        jvalue v;
        if (count)
        {
            for (i = 0; i < count; i++)
            {
                v.l = conv_jarname_to_url(jenv, rb_ary_entry(jarname, i));
                (*jenv)->CallObjectMethod(jenv, url_loader, url_add_url, v);
                rjb_check_exception(jenv, 0);
                (*jenv)->DeleteLocalRef(jenv, v.l);
            }
        }
        else
        {
            v.l = conv_jarname_to_url(jenv, jarname);
            (*jenv)->CallObjectMethod(jenv, url_loader, url_add_url, v);
            rjb_check_exception(jenv, 0);
            (*jenv)->DeleteLocalRef(jenv, v.l);
        }
    }
    return Qtrue;
}

static VALUE rjb_s_urls(VALUE self)
{
    JNIEnv* jenv;
    jvalue ret;
    if (!url_loader) return Qnil;
    jenv = rjb_prelude();
    ret.l = (*jenv)->CallObjectMethod(jenv, url_loader, url_geturls);
    return jarray2rv(jenv, ret);
}


/*
 * return class name
 */
static VALUE rjb_i_class(VALUE self)
{
    JNIEnv* jenv = rjb_attach_current_thread();
    struct jvi_data* ptr;
    jstring nm;
    Data_Get_Struct(self, struct jvi_data, ptr);
    nm = (*jenv)->CallObjectMethod(jenv, ptr->klass, rjb_class_getName);
    rjb_check_exception(jenv, 0);
    return jstring2val(jenv, nm);
}

/*
 * invoker
 */
static VALUE getter(JNIEnv* jenv, struct cls_field* pf,  struct jvi_data* ptr)
{
    jvalue jv;
    switch (pf->result_signature)
    {
    case 'D':
        if (pf->static_field)
        {
            jv.d = (*jenv)->GetStaticDoubleField(jenv, ptr->klass, pf->id);
        }
        else
        {
            jv.d = (*jenv)->GetDoubleField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'Z':
        if (pf->static_field)
        {
            jv.z = (*jenv)->GetStaticBooleanField(jenv, ptr->klass, pf->id);
        }
        else
        {
            jv.z = (*jenv)->GetBooleanField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'B':
        if (pf->static_field)
        {
            jv.b = (*jenv)->GetStaticByteField(jenv, ptr->klass, pf->id);
        }
        else
        {
            jv.b = (*jenv)->GetByteField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'F':
        if (pf->static_field)
        {
            jv.f = (*jenv)->GetStaticFloatField(jenv, ptr->klass, pf->id);
        }
        else
        {
            jv.f = (*jenv)->GetFloatField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'C':
        if (pf->static_field)
        {
            jv.c = (*jenv)->GetStaticCharField(jenv, ptr->klass, pf->id);
        }
        else
        {
            jv.c = (*jenv)->GetCharField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'S':
        if (pf->static_field)
        {
	    jv.s = (*jenv)->GetStaticShortField(jenv, ptr->klass, pf->id);
        }
        else
        {
	    jv.s = (*jenv)->GetShortField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'J':
        if (pf->static_field)
        {
	    jv.j = (*jenv)->GetStaticLongField(jenv, ptr->klass, pf->id);
        }
        else
        {
	    jv.j = (*jenv)->GetLongField(jenv, ptr->obj, pf->id);
        }
	break;
    case 'I':
        if (pf->static_field)
        {
	    jv.i = (*jenv)->GetStaticIntField(jenv, ptr->klass, pf->id);
        }
        else
        {
	    jv.i = (*jenv)->GetIntField(jenv, ptr->obj, pf->id);
        }
	break;
    default:
        if (pf->static_field)
        {
	    jv.l = (*jenv)->GetStaticObjectField(jenv, ptr->klass, pf->id);
        }
        else
        {
	    jv.l = (*jenv)->GetObjectField(jenv, ptr->obj, pf->id);
        }
	break;
    }
    if (pf->result_arraydepth)
    {
        return ja2r(pf->value_convert, jenv, jv, pf->result_arraydepth);
    }
    else
    {
        return pf->value_convert(jenv, jv);
    }
}

static void setter(JNIEnv* jenv, struct cls_field* pf,  struct jvi_data* ptr, VALUE val)
{
    jvalue jv;
    pf->arg_convert(jenv, val, &jv, pf->field_signature, 0);
    switch (*pf->field_signature)
    {
    case 'D':
        if (pf->static_field)
        {
            (*jenv)->SetStaticDoubleField(jenv, ptr->klass, pf->id, jv.d);
        }
        else
        {
            (*jenv)->SetDoubleField(jenv, ptr->obj, pf->id, jv.d);
        }
	break;
    case 'Z':
        if (pf->static_field)
        {
            (*jenv)->SetStaticBooleanField(jenv, ptr->klass, pf->id, jv.z);
        }
        else
        {
            (*jenv)->SetBooleanField(jenv, ptr->obj, pf->id, jv.z);
        }
	break;
    case 'B':
        if (pf->static_field)
        {
            (*jenv)->SetStaticByteField(jenv, ptr->klass, pf->id, jv.b);
        }
        else
        {
            (*jenv)->SetByteField(jenv, ptr->obj, pf->id, jv.b);
        }
	break;
    case 'F':
        if (pf->static_field)
        {
            (*jenv)->SetStaticFloatField(jenv, ptr->klass, pf->id, jv.f);
        }
        else
        {
            (*jenv)->SetFloatField(jenv, ptr->obj, pf->id, jv.f);
        }
	break;
    case 'C':
        if (pf->static_field)
        {
            (*jenv)->SetStaticCharField(jenv, ptr->klass, pf->id, jv.c);
        }
        else
        {
            (*jenv)->SetCharField(jenv, ptr->obj, pf->id, jv.c);
        }
	break;
    case 'S':
        if (pf->static_field)
        {
            (*jenv)->SetStaticShortField(jenv, ptr->klass, pf->id, jv.s);
        }
        else
        {
            (*jenv)->SetShortField(jenv, ptr->obj, pf->id, jv.s);
        }
	break;
    case 'J':
        if (pf->static_field)
        {
            (*jenv)->SetStaticLongField(jenv, ptr->klass, pf->id, jv.j);
        }
        else
        {
            (*jenv)->SetLongField(jenv, ptr->obj, pf->id, jv.j);
        }
	break;
    case 'I':
        if (pf->static_field)
        {
            (*jenv)->SetStaticIntField(jenv, ptr->klass, pf->id, jv.i);
        }
        else
        {
            (*jenv)->SetIntField(jenv, ptr->obj, pf->id, jv.i);
        }
	break;
    default:
        if (pf->static_field)
        {
            (*jenv)->SetStaticObjectField(jenv, ptr->klass, pf->id, jv.l);
        }
        else
        {
            (*jenv)->SetObjectField(jenv, ptr->obj, pf->id, jv.l);
        }
	break;
    }
    pf->arg_convert(jenv, val, &jv, pf->field_signature, 1);
}

static VALUE invoke(JNIEnv* jenv, struct cls_method* pm, struct jvi_data* ptr,
		    int argc, VALUE* argv, const char* sig)
{
    int i, found, cweight;
    jvalue jv;
    jvalue* args;
    char* psig;
    struct cls_method* orgpm = pm;
    struct cls_method* prefer_pm = NULL;
    int weight = 0;

    if (rb_block_given_p())
    {
        VALUE* pargs = ALLOCA_N(VALUE, argc + 1);
        memcpy(pargs, argv, argc * sizeof(VALUE));
        *(pargs + argc) = rb_block_proc();
        ++argc;
        argv = pargs;
    }

    for (; pm; pm = pm->next)
    {
        found = 0;
        if (argc == pm->basic.arg_count)
        {
            if (sig && pm->basic.method_signature)
            {
                if (!strcmp(sig, pm->basic.method_signature))
                {
	            found = 1;
                    prefer_pm = pm;
                    break;
		}
	    }
            else
            {
                found = 1;
                cweight = 0;
	        psig = pm->basic.method_signature;
                for (i = 0; i < argc; i++)
                {
                    int w = check_rtype(jenv, argv + i, psig);
		    if (!w)
                    {
                        found = 0;
                        break;
                    }
                    cweight += w;
		    psig = next_sig(psig);
                }
	    }
	}
        if (found)
        {
            if (cweight > weight || weight == 0)
            {
                prefer_pm = pm;
                weight = cweight;
            }
        }
    }
    if (!prefer_pm)
    {
	const char* tname = rb_id2name(orgpm->name);
	if (sig)
	{
	    rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s(\'%s\')'", tname, sig);
	}
	else
	{
	    rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s'", tname);
	}
    }
    args = (argc) ? ALLOCA_N(jvalue, argc) : NULL;
    psig = prefer_pm->basic.method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(prefer_pm->basic.arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 0);
	psig = next_sig(psig);
    }
    switch (prefer_pm->basic.result_signature)
    {
    case 'D':
      {
        INVOKEAD voked = *(INVOKEAD*)(((char*)*jenv) + prefer_pm->method);
	jv.d = voked(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
    case 'Z':
    case 'B':
      {
        INVOKEAZ vokez = *(INVOKEAZ*)(((char*)*jenv) + prefer_pm->method);
	jv.z = vokez(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
    case 'F':
      {
        INVOKEAF vokef = *(INVOKEAF*)(((char*)*jenv) + prefer_pm->method);
	jv.f = vokef(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
    case 'C':
    case 'S':
      {
        INVOKEAS vokes = *(INVOKEAS*)(((char*)*jenv) + prefer_pm->method);
	jv.s = vokes(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
#if HAVE_LONG_LONG
    case 'J':
      {
        INVOKEAL vokel = *(INVOKEAL*)(((char*)*jenv) + prefer_pm->method);
	jv.j = vokel(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
#endif
    default:
      {
        INVOKEA voke = *(INVOKEA*)(((char*)*jenv) + prefer_pm->method);
        jv.l = voke(jenv, ptr->obj, prefer_pm->basic.id, args);
      }
      break;
    }
    rjb_check_exception(jenv, 1);
    psig = prefer_pm->basic.method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(prefer_pm->basic.arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 1);
	psig = next_sig(psig);
    }
    if (prefer_pm->basic.result_arraydepth)
    {
        return ja2r(prefer_pm->result_convert, jenv, jv, prefer_pm->basic.result_arraydepth);
    }
    else
    {
        return prefer_pm->result_convert(jenv, jv);
    }
}

/*
 * Object invocation
 */
static VALUE invoke_by_instance(ID rmid, int argc, VALUE* argv,
				struct jvi_data* ptr, char* sig)
{
    VALUE ret = Qnil;
    JNIEnv* jenv = rjb_attach_current_thread();
    struct cls_field* pf;
    struct cls_method* pm;
    const char* tname = rb_id2name(rmid);
    if (argc == 0 && st_lookup(ptr->fields, rmid, (st_data_t*)&pf))
    {
        ret = getter(jenv, pf, ptr);
    }
    else
    {
        if (argc == 1 && *(tname + strlen(tname) - 1) == '=')
        {
            char* fname = ALLOCA_N(char, strlen(tname) + 1);
            strcpy(fname, tname);
            fname[strlen(tname) - 1] = '\0';
            if (st_lookup(ptr->fields, rb_intern(fname), (st_data_t*)&pf))
            {
                setter(jenv, pf, ptr, *argv);
                return ret;
            }
            /* fall through for the setter alias name */
        }
        if (st_lookup(ptr->methods, rmid, (st_data_t*)&pm))
        {
            ret = invoke(jenv, pm, ptr, argc, argv, sig);
        }
        else
        {
            rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s'", tname);
        }
    }
    return ret;
}

static VALUE get_signature(int* argc, VALUE* argv, VALUE* rmid)
{
    VALUE vsig;
    rb_scan_args(*argc, argv, "1*", rmid, &vsig);
    if (*argc == 1)
    {
        ++*argc;
        vsig = Qnil;
    }
    else
    {
        vsig = *(argv + 1);
    }
    return vsig;
}

static VALUE rjb_i_invoke(int argc, VALUE* argv, VALUE self)
{
    VALUE vsig, rmid;
    char* sig;
    struct jvi_data* ptr;

    vsig = get_signature(&argc, argv, &rmid);
    rmid = rb_to_id(rmid);
    sig = NIL_P(vsig) ? NULL :  StringValueCStr(vsig);
    Data_Get_Struct(self, struct jvi_data, ptr);

    return invoke_by_instance(rmid, argc - 2, argv + 2, ptr, sig);
}

static VALUE rjb_i_missing(int argc, VALUE* argv, VALUE self)
{
    struct jvi_data* ptr;
    ID rmid = rb_to_id(argv[0]);

    Data_Get_Struct(self, struct jvi_data, ptr);

    return invoke_by_instance(rmid, argc -1, argv + 1, ptr, NULL);
}

/*
 * Class invocation (first static method, then instance method)
 */
static VALUE invoke_by_class(ID rmid, int argc, VALUE* argv,
			     struct jv_data* ptr, char* sig)
{
    VALUE ret = Qnil;
    struct jv_data* clsptr;
    struct cls_field* pf;
    struct cls_method* pm;
    const char* tname = rb_id2name(rmid);
    JNIEnv* jenv = rjb_attach_current_thread();

    Data_Get_Struct(jklass, struct jv_data, clsptr);
    if (argc == 0 && st_lookup(ptr->idata.fields, rmid, (st_data_t*)&pf))
    {
        if (!pf->static_field)
	{
            rb_raise(rb_eRuntimeError, "instance field `%s' for class", tname);
	}
        ret = getter(jenv, pf, &ptr->idata);
    }
    else if (argc == 1 && *(tname + strlen(tname) - 1) == '=')
    {
        char* fname = ALLOCA_N(char, strlen(tname) + 1);
	strcpy(fname, tname);
	fname[strlen(tname) - 1] = '\0';
	if (st_lookup(ptr->idata.fields, rb_intern(fname), (st_data_t*)&pf))
	{
            if (!pf->static_field)
	    {
                rb_raise(rb_eRuntimeError, "instance field `%s' for class", fname);
            }
	    setter(jenv, pf, &ptr->idata, *argv);
        }
	else
	{
	    rb_raise(rb_eRuntimeError, "Fail: unknown field name `%s'", fname);
	}
    }
    else if (st_lookup(ptr->static_methods, rmid, (st_data_t*)&pm))
    {
	ret = invoke(jenv, pm, &ptr->idata, argc, argv, sig);
    }
    else if (st_lookup(clsptr->idata.methods, rmid, (st_data_t*)&pm))
    {
	ret = invoke(jenv, pm, &ptr->idata, argc, argv, sig);
    }
    else
    {
        if (st_lookup(ptr->idata.methods, rmid, (st_data_t*)&pm))
	{
	    rb_raise(rb_eRuntimeError, "instance method `%s' for class", tname);
	}
	else
        {
	    rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s'", tname);
	}
    }

    return ret;
}

static VALUE rjb_invoke(int argc, VALUE* argv, VALUE self)
{
    VALUE vsig, rmid;
    char* sig;
    struct jv_data* ptr;

    vsig = get_signature(&argc, argv, &rmid);
    rmid = rb_to_id(rmid);
    sig = NIL_P(vsig) ? NULL : StringValueCStr(vsig);
    Data_Get_Struct(self, struct jv_data, ptr);

    return invoke_by_class(rmid, argc - 2, argv + 2, ptr, sig);
}

static VALUE find_const(VALUE pv)
{
    VALUE* p = (VALUE*)pv;
    return rb_const_get(*p, (ID)*(p + 1));
}

static VALUE rjb_missing(int argc, VALUE* argv, VALUE self)
{
    struct jv_data* ptr;
    ID rmid = rb_to_id(argv[0]);
    const char* rmname = rb_id2name(rmid);

    if (isupper(*rmname))
    {
        VALUE r, args[2];
        int state = 0;
	args[0] = rb_obj_class(self);
	args[1] = rmid;
        r = rb_protect(find_const, (VALUE)args, &state);
	if (!state)
        {
	    return r;
	}
    }

    Data_Get_Struct(self, struct jv_data, ptr);
    return invoke_by_class(rmid, argc - 1, argv + 1, ptr, NULL);
}

/*
 * Class#forName entry.
 */
static VALUE rjb_class_forname(int argc, VALUE* argv, VALUE self)
{
    if (argc == 1)
    {
        return rjb_s_import(self, *argv);
    }
    else
    {
        struct jv_data* ptr;
	ID rmid = rb_intern("forName");
	Data_Get_Struct(self, struct jv_data, ptr);
	return invoke_by_class(rmid, argc, argv, ptr, NULL);
    }
}

/*
 * Class initializer called by Ruby while requiring this library
 */
void Init_rjbcore()
{
#if RJB_RUBY_VERSION_CODE < 190
  #if defined(RUBINIUS)
    rb_require("iconv");
  #else
    rb_protect((VALUE(*)(VALUE))rb_require, (VALUE)"iconv", NULL);
  #endif
#endif
    rjb_loaded_classes = rb_hash_new();
#ifndef RUBINIUS
    OBJ_FREEZE(rjb_loaded_classes);
#endif
    rb_global_variable(&rjb_loaded_classes);
    proxies = rb_ary_new();
    rb_global_variable(&proxies);
    user_initialize = rb_intern(USER_INITIALIZE);
    initialize_proxy = rb_intern("initialize_proxy");

    rjb = rb_define_module("Rjb");
    rb_define_module_function(rjb, "load", rjb_s_load, -1);
    rb_define_module_function(rjb, "unload", rjb_s_unload, -1);
    rb_define_module_function(rjb, "loaded?", rjb_s_loaded, 0);
    rb_define_module_function(rjb, "import", rjb_s_import, 1);
    rb_define_module_function(rjb, "bind", rjb_s_bind, 2);
    rb_define_module_function(rjb, "unbind", rjb_s_unbind, 1);
    rb_define_module_function(rjb, "classes", rjb_s_classes, 0);
    rb_define_module_function(rjb, "throw", rjb_s_throw, -1);
    rb_define_module_function(rjb, "primitive_conversion=", rjb_s_set_pconversion, 1);
    rb_define_module_function(rjb, "primitive_conversion", rjb_s_get_pconversion, 0);
    rb_define_module_function(rjb, "add_classpath", rjb_s_add_classpath, 1);
    rb_define_module_function(rjb, "add_jar", rjb_s_add_jar, 1);
    rb_define_alias(rjb, "add_jars", "add_jar");
    rb_define_module_function(rjb, "urls", rjb_s_urls, 0);
    rb_define_const(rjb, "VERSION", rb_str_new2(RJB_VERSION));
    rb_define_class_variable(rjb, "@@classpath", rb_ary_new());
    cvar_classpath = rb_intern("@@classpath");

    /* Java class object */
    rjbc = CLASS_NEW(rb_cObject, "Rjb_JavaClass");
    rb_gc_register_address(&rjbc);
    rb_define_method(rjbc, "method_missing", rjb_missing, -1);
    rb_define_method(rjbc, "impl", rjb_s_impl, 0);
    rb_define_method(rjbc, "_invoke", rjb_invoke, -1);
    rb_define_method(rjbc, "_classname", rjb_i_class, 0);

    /* Java instance object */
    rjbi = CLASS_NEW(rb_cObject, "Rjb_JavaProxy");
    rb_gc_register_address(&rjbi);
    rb_define_method(rjbi, "method_missing", rjb_i_missing, -1);
    rb_define_method(rjbi, "_invoke", rjb_i_invoke, -1);
    rb_define_method(rjbi, "_classname", rjb_i_class, 0);
    rb_define_method(rjbi, "_prepare_proxy", rjb_i_prepare_proxy, 0);
    rb_define_alias(rjbi, "include", "extend");

    /* Ruby-Java Bridge object */
    rjbb = CLASS_NEW(rb_cObject, "Rjb_JavaBridge");
    rb_gc_register_address(&rjbb);

    /* anonymous interface object */
    rjba = CLASS_NEW(rb_cObject, "Rjb_AnonymousClass");
    rb_gc_register_address(&rjba);
    rb_define_method(rjba, "initialize", rjb_a_initialize, 1);
    rb_define_method(rjba, "method_missing", rjb_a_missing, -1);
    anonymousblock = rb_intern("@anon_block");
    id_call = rb_intern("call");
}

VALUE rjb_safe_funcall(VALUE args)
{
    VALUE* argp = (VALUE*)args;
    return rb_funcall2(*argp, *(argp + 1), (int)*(argp + 2), argp + 3);
}

/**
  Entry point from JavaVM through java.reflect.Proxy
  */
JNIEXPORT jobject JNICALL Java_jp_co_infoseek_hp_arton_rjb_RBridge_call
  (JNIEnv * jenv, jobject bridge, jstring name, jobject proxy, jobjectArray args)
{
    int i;
    jvalue j;
    memset(&j, 0, sizeof(j));
    for (i = 0; i < RARRAY_LEN(proxies); i++)
    {
	struct rj_bridge* ptr;
	VALUE val = RARRAY_PTR(proxies)[i];
	Data_Get_Struct(val, struct rj_bridge, ptr);
	if ((*jenv)->IsSameObject(jenv, proxy, ptr->proxy))
	{
            int sstat;
	    VALUE result;
	    VALUE* argv = NULL;
	    int argc = 3;
            ID id = rb_to_id(jstring2val(jenv, name));
	    if (args)
	    {
		int i;
		jsize js = (*jenv)->GetArrayLength(jenv, args);
		argc += (int)js;
		argv = ALLOCA_N(VALUE, argc);
		memset(argv, 0, sizeof(VALUE*) * argc);
		for (i = 3; i < argc; i++)
		{
		    jobject f = (*jenv)->GetObjectArrayElement(jenv, args, i - 3);
		    /* f will be release in jv2rv_withprim */
		    *(argv + i) = jv2rv_withprim(jenv, f);
		}
	    }
	    else
	    {
		argv = ALLOCA_N(VALUE, argc + 1);
		memset(argv, 0, sizeof(VALUE*) * (argc + 1));
	    }
            *argv = ptr->wrapped;
            *(argv + 1) = id;
            *(argv + 2) = argc - 3;
            result = rb_protect(rjb_safe_funcall, (VALUE)argv, &sstat);
	    rv2jobject(jenv, result, &j, NULL, 0);
	    /* I can't delete this object... */
            break;
	}
    }
    return j.l;
}
