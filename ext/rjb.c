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

#define RJB_VERSION "1.0.2"

#include "ruby.h"
#include "st.h"
#include "jniwrap.h"
#include "jp_co_infoseek_hp_arton_rjb_RBridge.h"
#include "riconv.h"
#include "rjb.h"

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

extern int create_jvm(JNIEnv** pjenv, JavaVMInitArgs*, char*, VALUE);
static void register_class(VALUE, VALUE);
static VALUE import_class(JNIEnv* jenv, jclass, VALUE);
static VALUE register_instance(JNIEnv* jenv, struct jvi_data*, jobject);
static VALUE rjb_s_free(struct jv_data*);
static VALUE rjb_class_forname(int argc, VALUE* argv, VALUE self);
static void setup_metadata(JNIEnv* jenv, VALUE self, struct jv_data*, VALUE classname);
static VALUE jarray2rv(JNIEnv* jenv, jvalue val);
static jarray r2objarray(JNIEnv* jenv, VALUE v, const char* cls);

static VALUE rjb;
static VALUE jklass;
VALUE rjbc;
VALUE rjbi;
VALUE rjbb;

VALUE loaded_classes;
static VALUE proxies;
JavaVM* jvm;
JNIEnv* main_jenv;
jclass rbridge;
jmethodID register_bridge;

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
jclass j_class;
jmethodID class_getName;
/* throwable */
jclass j_throwable;
jmethodID throwable_getMessage;
/* String global reference */
static jclass j_string;
static jmethodID str_tostring;
/* Object global reference */
static jclass j_object;

enum PrimitiveType {
    PRM_INT = 0,
    PRM_LONG,
    PRM_DOUBLE,
    PRM_BOOLEAN,
    PRM_CHARACTER,
    PRM_SHORT,
    PRM_BYTE,
    PRM_FLOAT,
    //
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

JNIEnv* attach_current_thread(void)
{
  JNIEnv* env;
  (*jvm)->AttachCurrentThread(jvm, (void**)&env, '\0');
  return env;
}


void release_string(JNIEnv *jenv, jstring str, const char* chrs)
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

static char* jni2javaname(char* jnicls)
{
    char* p;
    for (p = jnicls; *p; p++)
    {
	if (*p == '/')
	{
	    *p = '.';
	}
    }
    return jnicls;
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
    release_string(jenv, s, p);
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
    jstring nm = (*jenv)->CallObjectMethod(jenv, jc, class_getName);
    check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    clsname = rb_str_new2(cname);
    release_string(jenv, nm, cname);
    v = rb_hash_aref(loaded_classes, clsname);
    if (v == Qnil)
    {
        v = import_class(jenv, jc, clsname);
    }
    (*jenv)->DeleteLocalRef(jenv, jc);
    return v;
}

static VALUE jv2rv(JNIEnv* jenv, jvalue val)
{
    const char* cname;
    jstring nm;
    jclass klass;
    VALUE clsname;
    VALUE v;
    struct jv_data* ptr;
    // object to ruby
    if (!val.l) return Qnil;
    klass = (*jenv)->GetObjectClass(jenv, val.l);
    if ((*jenv)->IsSameObject(jenv, klass, j_class))
    {
        (*jenv)->DeleteLocalRef(jenv, klass);
        return jv2rclass(jenv, val.l);
    }
    nm = (*jenv)->CallObjectMethod(jenv, klass, class_getName);
    check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    if (*cname == '[')
    {
        release_string(jenv, nm, cname);
        return jarray2rv(jenv, val);
    }
    clsname = rb_str_new2(cname);
    release_string(jenv, nm, cname);
    v = rb_hash_aref(loaded_classes, clsname);
    if (v == Qnil)
    {
        v = import_class(jenv, klass, clsname);
    }
    Data_Get_Struct(v, struct jv_data, ptr);
    v = register_instance(jenv, (struct jvi_data*)ptr, val.l);
    (*jenv)->DeleteLocalRef(jenv, klass);
    (*jenv)->DeleteLocalRef(jenv, val.l);    
    return v;
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
    VALUE* pv = RARRAY(v)->ptr;
    RARRAY(v)->len = len;
    for (i = 0; i < len; i++)
    {
        *pv++ =conv(jenv, cp);
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
    { "java/lang/Character", "charValue", "()C", NULL, NULL, 0, 0, jint2rv, },
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
    jclass klass = (*jenv)->GetObjectClass(jenv, o);
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
	    default:
		rb_raise(rb_eRuntimeError, "no convertor defined(%d)", i);
		break;
	    }
	    return jpcvt[i].func(jenv, jv);
	}
    }
    jv.l = o;
    return jv2rv(jenv, jv);
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
	jv->j = NUM2INT(val);
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
    if (!release)
	jv->s = (short)NUM2INT(val);    
}
static void rv2jboolean(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
	jv->z = (val == Qnil || val == Qfalse) ? JNI_FALSE : JNI_TRUE;
}
static void rv2jstring(JNIEnv* jenv, VALUE val, jvalue* jv, const char* psig, int release)
{
    if (!release)
    {
	if (TYPE(val) == T_DATA
	    && (RBASIC(val)->klass == rjbi || RBASIC(val)->klass == rjb))
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
		check_exception(jenv, 0);
		js = (*jenv)->CallObjectMethod(jenv, ptr->obj, tostr);
		check_exception(jenv, 0);
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
	    if (RBASIC(val)->klass == rjbi || RBASIC(val)->klass == rjb)
	    {
		struct jvi_data* ptr;
		Data_Get_Struct(val, struct jvi_data, ptr);
		if ((*jenv)->IsInstanceOf(jenv, ptr->obj, j_string))
		{
		    return; // never delete at this time
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
	    // no-op
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
	        if (RBASIC(val)->klass == rjbi || RBASIC(val)->klass == rjb)
		{
		    // TODO: check instanceof (class (in psig) )
		    struct jvi_data* ptr;
		    Data_Get_Struct(val, struct jvi_data, ptr);
		    jv->l = ptr->obj;
		}
		else if (RBASIC(val)->klass == rjbb)
		{
		    struct rj_bridge* ptr;
		    Data_Get_Struct(val, struct rj_bridge, ptr);
		    jv->l = ptr->proxy;
		}
		else if (rb_class_inherited(rjbc, RBASIC(val)->klass)) 
		{
		    struct jv_data* ptr;
		    Data_Get_Struct(val, struct jv_data, ptr);
		    jv->l = ptr->idata.obj;
		}
		break;
	    case T_STRING:
		rv2jstring(jenv, val, jv, NULL, 0);
		break;
	    case T_FLOAT:
		arg.d = NUM2DBL(val);
		jv->l = (*jenv)->NewObject(jenv, jpcvt[PRM_DOUBLE].klass,
				       jpcvt[PRM_DOUBLE].ctr_id, arg);
		break;
	    case T_ARRAY:
		jv->l = r2objarray(jenv, val, "Ljava/lang/Object;");
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
            (*jenv)->DeleteLocalRef(jenv, jv->l);
	    break;
        }
    }
}

static void check_fixnumarray(VALUE v)
{
    int i;
    int len = RARRAY(v)->len;
    VALUE* p = RARRAY(v)->ptr;
    // check all fixnum (overflow is permit)
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
	ary = (*jenv)->NewByteArray(jenv, RSTRING(v)->len);
	(*jenv)->SetByteArrayRegion(jenv, ary, 0, RSTRING(v)->len,
				    RSTRING(v)->ptr);
    }
    else if (TYPE(v) == T_ARRAY)
    {
	int i;
	jbyte* pb;
	check_fixnumarray(v);
	ary = (*jenv)->NewByteArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetByteArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jbyte)FIX2ULONG(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewCharArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetCharArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jchar)FIX2ULONG(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewDoubleArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetDoubleArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jdouble)rb_num2dbl(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewFloatArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetFloatArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jfloat)rb_num2dbl(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewIntArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetIntArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jint)FIX2LONG(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewLongArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetLongArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
#if HAVE_LONG_LONG
	    *(pb + i) = (jlong)rb_num2ll(RARRAY(v)->ptr[i]);
#else
	    *(pb + i) = (jlong)FIX2LONG(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewShortArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetShortArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i) = (jshort)FIX2LONG(RARRAY(v)->ptr[i]);
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
	ary = (*jenv)->NewBooleanArray(jenv, RARRAY(v)->len);
	pb = (*jenv)->GetBooleanArrayElements(jenv, ary, NULL);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    *(pb + i)
		= (NIL_P(RARRAY(v)->ptr[i]) || RARRAY(v)->ptr[i] == Qfalse)
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
	ary = (*jenv)->NewObjectArray(jenv, RARRAY(v)->len, j_object, NULL);
	check_exception(jenv, 0);
	for (i = 0; i < RARRAY(v)->len; i++)
	{
	    jvalue jv;
	    rv2jobject(jenv, RARRAY(v)->ptr[i], &jv, NULL, 0);
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
	(*jenv)->DeleteLocalRef(jenv, jv->l);
    }
    else
    {
        int i;
        jarray ja = NULL;
	if (NIL_P(val))
	{
	    // no-op, null for an array
	}
        else if (*(psig + 1) == '[')
        {
            if (TYPE(val) != T_ARRAY) {
                rb_raise(rb_eRuntimeError, "array's rank unmatch");
            }
            ja = (*jenv)->NewObjectArray(jenv, RARRAY(val)->len, j_object, NULL);
            check_exception(jenv, 0);
            for (i = 0; i < RARRAY(val)->len; i++)
            {
                jvalue jv;
                rv2jarray(jenv, RARRAY(val)->ptr[i], &jv, psig + 1, 0);
                (*jenv)->SetObjectArrayElement(jenv, ja, i, jv.l);
            }
        }
        else
        {
            R2JARRAY r2a = r2objarray;
            for (i = 0; i < COUNTOF(jcvt); i++)
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
    int len, i;
    const char* cname;
    R2J result = NULL;
    jstring nm = (*jenv)->CallObjectMethod(jenv, o, class_getName);
    check_exception(jenv, 0);
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    if (*cname == '[')
    {
        if (siglen)
        {
            len = strlen(cname);
	    *siglen += len;
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
		    *siglen += strlen(jcvt[i].jntype);
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
    release_string(jenv, nm, cname);
    return result;
}

static J2R get_j2r(JNIEnv* jenv, jobject cls, char* psig, char* pdepth, char* ppsig, off_t* piv, int static_method)
{
    int i;
    J2R result = NULL;
    const char* cname;
    const char* jname = NULL;
    jstring nm = (*jenv)->CallObjectMethod(jenv, cls, class_getName);
    check_exception(jenv, 0);	    
    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);

    if (*cname == '[')
    {
        int start;
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
		result = jcvt[i].ja2r;
		break;
	    }
	}
	if (!result)
	{
	    result = jarray2rv;
	}
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
    release_string(jenv, nm, cname);
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
 
static void create_methodinfo(JNIEnv* jenv, st_table* tbl, jobject m, int static_method)
{
    struct cls_method* result;
    struct cls_method* pm;    
    char* param = NULL;
    const char* jname;
    jstring nm;
    jobjectArray parama;
    jobject cls;
    jsize param_count;

    result = ALLOC(struct cls_method);
    parama = (*jenv)->CallObjectMethod(jenv, m, getParameterTypes);
    check_exception(jenv, 0);
    param_count = (*jenv)->GetArrayLength(jenv, parama);
    check_exception(jenv, 0);
    setup_methodbase(jenv, &result->basic, parama, param_count);

    nm = (*jenv)->CallObjectMethod(jenv, m, method_getName);
    check_exception(jenv, 0);	    
    jname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    result->name = rb_intern(jname);
    release_string(jenv, nm, jname);
    result->basic.id = (*jenv)->FromReflectedMethod(jenv, m);
    check_exception(jenv, 0);
    cls = (*jenv)->CallObjectMethod(jenv, m, getReturnType);
    check_exception(jenv, 0);
    setup_j2r(jenv, cls, result, static_method);
    (*jenv)->DeleteLocalRef(jenv, cls);
    result->static_method = static_method;
    if (st_lookup(tbl, result->name, (st_data_t*)&pm))
    {
	result->next = pm->next;
	pm->next = result;
    }
    else
    {
	result->next = NULL;
	st_insert(tbl, result->name, (VALUE)result);
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
    char sig = 0;

    result = ALLOC(struct cls_field);
    memset(result, 0, sizeof(struct cls_field));
    nm = (*jenv)->CallObjectMethod(jenv, f, field_getName);
    check_exception(jenv, 0);	    
    jname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
    result->name = rb_intern(jname);
    release_string(jenv, nm, jname);
    result->id = (*jenv)->FromReflectedField(jenv, f);
    check_exception(jenv, 0);
    cls = (*jenv)->CallObjectMethod(jenv, f, field_getType);
    check_exception(jenv, 0);
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
	check_exception(jenv, 0);
	pc = ALLOC(struct cls_constructor);
	tbl[i] = pc;
	parama = (*jenv)->CallObjectMethod(jenv, c, ctrGetParameterTypes);
	check_exception(jenv, 0);
	pcount = (*jenv)->GetArrayLength(jenv, parama);
	check_exception(jenv, 0);
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
	check_exception(jenv, 0);	
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
	check_exception(jenv, 0);	
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
	check_exception(jenv, 0);
	modifier = (*jenv)->CallIntMethod(jenv, f, field_getModifiers);
	check_exception(jenv, 0);
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

	    // constants make define directly in the ruby object
	    cls = (*jenv)->CallObjectMethod(jenv, f, field_getType);
	    check_exception(jenv, 0);
	    iv = 0;
	    sig = depth = 0;
	    j2r = get_j2r(jenv, cls, &sig, &depth, sigs, &iv, 1);
	    if (!j2r) j2r = jv2rv;
	    (*jenv)->DeleteLocalRef(jenv, cls);
	    nm = (*jenv)->CallObjectMethod(jenv, f, field_getName);
	    check_exception(jenv, 0);
	    cname = (*jenv)->GetStringUTFChars(jenv, nm, NULL);
	    check_exception(jenv, 0);
	    jfid = (*jenv)->GetStaticFieldID(jenv, klass, cname, sigs);
	    check_exception(jenv, 0);
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
	    if (!isupper(*cname))
	    {
 	        char* p = ALLOCA_N(char, strlen(cname) + 1);
		strcpy(p, cname);
		*p = toupper(*p);
		if (isupper(*p)) 
		{
	            rb_define_const(RBASIC(self)->klass, p, j2r(jenv, jv));
		}
	    }
	    else
	    {
	        rb_define_const(RBASIC(self)->klass, cname, j2r(jenv, jv));
	    }

	    release_string(jenv, nm, cname);
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
    check_exception(jenv, 0);
    mid = (*jenv)->GetMethodID(jenv, klass, "getMethods", "()[Ljava/lang/reflect/Method;");
    check_exception(jenv, 0);
    methods = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    check_exception(jenv, 0);
    setup_methods(jenv, &ptr->idata.methods, &ptr->static_methods, methods);
    mid = (*jenv)->GetMethodID(jenv, klass, "getConstructors", "()[Ljava/lang/reflect/Constructor;");
    check_exception(jenv, 0);
    methods = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    check_exception(jenv, 0);
    setup_constructors(jenv, &ptr->constructors, methods);
    mid = (*jenv)->GetMethodID(jenv, klass, "getFields", "()[Ljava/lang/reflect/Field;");
    check_exception(jenv, 0);
    flds = (*jenv)->CallNonvirtualObjectMethod(jenv, ptr->idata.obj, klass, mid);
    check_exception(jenv, 0);
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
    VALUE user_path;
    VALUE vm_argv;
    char* userpath;
    int i;
    jclass jmethod;
    jclass jfield;
    jclass jconstructor;

    if (jvm)
    {
	return Qnil;
    }

    memset(&vm_args, 0, sizeof(vm_args));
    vm_args.version = JNI_VERSION_1_4;
    rb_scan_args(argc, argv, "02", &user_path, &vm_argv);
    if (!NIL_P(user_path))
    {
        Check_Type(user_path, T_STRING);
	userpath = StringValueCStr(user_path);
    }
    else
    {
	userpath = ".";
    }
    if (!NIL_P(vm_argv))
    {
        Check_Type(vm_argv, T_ARRAY);
    }
    jenv = NULL;
    res = create_jvm(&jenv, &vm_args, userpath, vm_argv);
    if (res < 0)
    {
	jvm = NULL;
	rb_raise(rb_eRuntimeError, "can't create Java VM");
    } else {
        main_jenv = jenv;
    }

    jconstructor = (*jenv)->FindClass(jenv, "java/lang/reflect/Constructor");
    check_exception(jenv, 1);
    ctrGetParameterTypes = (*jenv)->GetMethodID(jenv, jconstructor, "getParameterTypes", "()[Ljava/lang/Class;");
    check_exception(jenv, 1);
    jmethod = (*jenv)->FindClass(jenv, "java/lang/reflect/Method");
    method_getModifiers = (*jenv)->GetMethodID(jenv, jmethod, "getModifiers", "()I");
    check_exception(jenv, 1);
    method_getName = (*jenv)->GetMethodID(jenv, jmethod, "getName", "()Ljava/lang/String;");
    check_exception(jenv, 1);	
    getParameterTypes = (*jenv)->GetMethodID(jenv, jmethod, "getParameterTypes", "()[Ljava/lang/Class;");
    check_exception(jenv, 1);	
    getReturnType = (*jenv)->GetMethodID(jenv, jmethod, "getReturnType", "()Ljava/lang/Class;");
    check_exception(jenv, 1);

    jfield = (*jenv)->FindClass(jenv, "java/lang/reflect/Field");
    field_getModifiers = (*jenv)->GetMethodID(jenv, jfield, "getModifiers", "()I");
    check_exception(jenv, 1);
    field_getName = (*jenv)->GetMethodID(jenv, jfield, "getName", "()Ljava/lang/String;");
    check_exception(jenv, 1);	
    field_getType = (*jenv)->GetMethodID(jenv, jfield, "getType", "()Ljava/lang/Class;");
    check_exception(jenv, 1);	

    j_class = (*jenv)->FindClass(jenv, "java/lang/Class");
    check_exception(jenv, 1);
    class_getName = (*jenv)->GetMethodID(jenv, j_class, "getName", "()Ljava/lang/String;");
    check_exception(jenv, 1);
    j_class = (*jenv)->NewGlobalRef(jenv, j_class);

    j_throwable = (*jenv)->FindClass(jenv, "java/lang/Throwable");
    check_exception(jenv, 1);
    throwable_getMessage = (*jenv)->GetMethodID(jenv, j_throwable, "getMessage", "()Ljava/lang/String;");
    check_exception(jenv, 1);

    j_string = (*jenv)->FindClass(jenv, "java/lang/String");
    check_exception(jenv, 1);
    str_tostring = (*jenv)->GetMethodID(jenv, j_string, "toString", "()Ljava/lang/String;");
    check_exception(jenv, 1);
    j_string = (*jenv)->NewGlobalRef(jenv, j_string);

    j_object = (*jenv)->FindClass(jenv, "java/lang/Object");
    check_exception(jenv, 1);
    j_object = (*jenv)->NewGlobalRef(jenv, j_object);

    for (i = PRM_INT; i < PRM_LAST; i++)
    {
	jclass klass = (*jenv)->FindClass(jenv, jpcvt[i].classname);
	if (i == PRM_BOOLEAN)
	{
	    jpcvt[i].ctr_id = (*jenv)->GetStaticMethodID(jenv,
			 klass, "valueOf", jpcvt[i].ctrsig);
	    check_exception(jenv, 1);
	}
	else if (jpcvt[i].ctrsig)
	{
	    jpcvt[i].ctr_id = (*jenv)->GetMethodID(jenv, klass,
						   "<init>", jpcvt[i].ctrsig);
	    check_exception(jenv, 1);
	}
	jpcvt[i].to_prim_id = (*jenv)->GetMethodID(jenv, klass,
				   jpcvt[i].to_prim_method, jpcvt[i].prmsig);
	check_exception(jenv, 1);
        jpcvt[i].klass = (*jenv)->NewGlobalRef(jenv, klass);
    }

    jklass = import_class(jenv, j_class, rb_str_new2("java.lang.Class"));
    rb_define_method(RBASIC(jklass)->klass, "forName", rjb_class_forname, -1);
    rb_gc_register_address(&jklass);
    
    return Qnil;
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
    st_foreach(RHASH(loaded_classes)->tbl, clear_classes, 0);
    if (jvm)
    {
	(*jvm)->DestroyJavaVM(jvm);
	jvm = NULL;
    }
    return Qnil;
}

/*
 * return all classes that were already loaded.
 * this method simply returns the global hash,
 * but it's safe because the hash was frozen.
 */
static VALUE rjb_s_classes(VALUE self)
{
    return loaded_classes;
}

/*
 * free java class
 */
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

/*
 * finalize Object instance
 */
static VALUE rjb_delete_ref(struct jvi_data* ptr)
{
    JNIEnv* jenv = attach_current_thread();
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
    JNIEnv* jenv = attach_current_thread();    
    (*jenv)->DeleteLocalRef(jenv, ptr->proxy);    
    (*jenv)->DeleteLocalRef(jenv, ptr->bridge);
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
    JNIEnv* jenv = attach_current_thread();
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
    st_delete(RHASH(loaded_classes)->tbl, clsname, NULL);
    */
    return Qnil;
}

/*
 * create new instance of this class
 */
static VALUE createinstance(JNIEnv* jenv, int argc, VALUE* argv,
	    struct jvi_data* org, struct cls_constructor* pc)
{
    int i;
    char* psig = pc->method_signature;
    jobject obj = NULL;
    VALUE result;

    jvalue* args = (argc) ? ALLOCA_N(jvalue, argc) : NULL;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pc->arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 0);
	psig = next_sig(psig);
    }
    obj = (*jenv)->NewObjectA(jenv, org->obj, pc->id, args);
    if (!obj)
    {
	check_exception(jenv, 1);
    }
    psig = pc->method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pc->arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 1);
	psig = next_sig(psig);
    }

    result = register_instance(jenv, org, obj);
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

static VALUE register_instance(JNIEnv* jenv, struct jvi_data* org, jobject obj)
{
    VALUE v;
    struct jvi_data* ptr = ALLOC(struct jvi_data);
    memset(ptr, 0, sizeof(struct jvi_data));
    v = Data_Wrap_Struct(rjbi, NULL, rjb_delete_ref, ptr);
    ptr->klass = org->obj;
    ptr->obj = (*jenv)->NewGlobalRef(jenv, obj);
    ptr->methods = org->methods;
    ptr->fields = org->fields;
    return v;
}

/*
 * temporary signature check
 * return !0 if found
 */
static int check_rtype(JNIEnv* jenv, VALUE v, char* p)
{
    char* pcls = NULL;
    if (*p == 'L')
    {
        char* pt = strchr(p, ';');
	if (pt)
	{
	    int len = pt - p - 1;
	    pcls = ALLOCA_N(char, len + 1);
            strncpy(pcls, p + 1, len);
	    *(pcls + len) = '\0';
	}
    }
    if (pcls && !strcmp("java.lang.Object", pcls))
    {
        return 1;
    }
    switch (TYPE(v))
    {
    case T_FIXNUM:
        return (int)strchr("BCDFIJS", *p);
    case T_FLOAT:
	return (int)strchr("DF", *p);
    case T_STRING:
      return pcls && !strcmp("java.lang.String", pcls);
    case T_TRUE:
    case T_FALSE:
        return *p == 'Z';
    case T_ARRAY:
        return *p == '[';
    case T_DATA:
        if (RBASIC(v)->klass == rjbi && pcls)
	{
	    // imported object
	    jclass cls;
            struct jvi_data* ptr;
	    int result = 0;
            if (!strcmp("java.lang.String", pcls)) return 1;
	    Data_Get_Struct(v, struct jvi_data, ptr);
	    cls = (*jenv)->FindClass(jenv, java2jniname(pcls));
	    if (cls)
	    {
	        result = (cls && (*jenv)->IsInstanceOf(jenv, ptr->obj, cls));
	        (*jenv)->DeleteLocalRef(jenv, cls);
	    }
	    return result;
	}
	// fall down to the next case 
    case T_OBJECT:
	// fall down to the next case
    default:
        if (pcls || *p == '[')
        {
            return 1;
        }
	return 0;
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
    JNIEnv* jenv = attach_current_thread();

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
		ret = createinstance(jenv, argc - 1, argv + 1, &ptr->idata, *pc);
		break;
	    }
	}
    }
    return ret;
}

static VALUE rjb_newinstance(int argc, VALUE* argv, VALUE self)
{
    VALUE ret = Qnil;
    struct jv_data* ptr;
    struct cls_constructor** pc;
    JNIEnv* jenv = attach_current_thread();

    Data_Get_Struct(self, struct jv_data, ptr);

    if (ptr->constructors)
    {
        int i, found = 0;
	char* psig;
	for (pc = ptr->constructors; *pc; pc++)
	{
	    if ((*pc)->arg_count == argc)
	    {
	        found = 1;
		psig = (*pc)->method_signature;
		for (i = 0; i < argc; i++)
		{
		    if (!check_rtype(jenv, *(argv + i), psig))
                    {
		        found = 0;
			break;
		    }
		    psig = next_sig(psig);
		}
		if (found)
		{
		    ret = createinstance(jenv, argc, argv, &ptr->idata, *pc);
		    break;
		}
	    }
	}
    }
    return ret;
}

/*
 * find java class from classname
 */
jclass find_class(JNIEnv* jenv, VALUE name)
{
    char* cname;
    char* jnicls;
    
    cname = StringValueCStr(name);
    jnicls = ALLOCA_N(char, strlen(cname) + 1);
    strcpy(jnicls, cname);
    return (*jenv)->FindClass(jenv, java2jniname(jnicls));
}

/*
 * jclass Rjb::bind(rbobj, interface_name)
 */
static VALUE rjb_s_bind(VALUE self, VALUE rbobj, VALUE itfname)
{
    VALUE result = Qnil;
    JNIEnv* jenv = attach_current_thread();
    
    jclass itf = find_class(jenv, itfname); 
    check_exception(jenv, 1);
    if (itf)
    {
	struct rj_bridge* ptr = ALLOC(struct rj_bridge);
	memset(ptr, 0, sizeof(struct rj_bridge));
	ptr->bridge = (*jenv)->NewGlobalRef(jenv,
                                   (*jenv)->AllocObject(jenv, rbridge));
	if (!ptr->bridge)
	{
	    free(ptr);
	    check_exception(jenv, 1);
	    return Qnil;
	}
	ptr->proxy = (*jenv)->CallObjectMethod(jenv, ptr->bridge,
					       register_bridge, itf);
        ptr->proxy = (*jenv)->NewGlobalRef(jenv, ptr->proxy);
	ptr->wrapped = rbobj;
	result = Data_Wrap_Struct(rjbb, rj_bridge_mark, rj_bridge_free, ptr);
	rb_ary_push(proxies, result);
    }
    return result;
}

/*
 * Jclass Rjb::import(classname)
 */
static VALUE rjb_s_import(VALUE self, VALUE clsname)
{
    JNIEnv* jenv;
    jclass jcls;
    VALUE v = rb_hash_aref(loaded_classes, clsname);
    if (v != Qnil)
    {
	return v;
    }

    if (!jvm) 
    {
        /* auto-load with default setting */
        rjb_s_load(0, NULL, 0);
    }
    jenv = attach_current_thread();
    jcls = find_class(jenv, clsname);
    if (!jcls)
    {
	check_exception(jenv, 0);
	rb_raise(rb_eRuntimeError, "`%s' not found", StringValueCStr(clsname));
    }
    v = import_class(jenv, jcls, clsname);
    return v;
}

static void register_class(VALUE self, VALUE clsname)
{
    rb_define_singleton_method(self, "new", rjb_newinstance, -1);
    rb_define_singleton_method(self, "new_with_sig", rjb_newinstance_s, -1);
    /*
     * the hash was frozen, so it need to call st_ func directly.
     */
    st_insert(RHASH(loaded_classes)->tbl, clsname, self);
}

/*
 * return class name
 */
static VALUE rjb_i_class(VALUE self)
{
    JNIEnv* jenv = attach_current_thread();
    struct jvi_data* ptr;
    jstring nm;
    Data_Get_Struct(self, struct jvi_data, ptr);
    nm = (*jenv)->CallObjectMethod(jenv, ptr->klass, class_getName);
    check_exception(jenv, 0);
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
    int i, found;
    jvalue jv;
    jvalue* args;
    char* psig;
    struct cls_method* orgpm = pm;

    for (found = 0; pm; pm = pm->next)
    {
        if (argc == pm->basic.arg_count)
        {
            if (sig) 
            {
                if (!strcmp(sig, pm->basic.method_signature))
                {
	            found = 1;
                    break;
		}
	    }
            else
            {
	        psig = pm->basic.method_signature;
                found = 1;
                for (i = 0; i < argc; i++)
                {
		    if (!check_rtype(jenv, *(argv + i), psig))
                    {
                        found = 0;
                        break;
                    }
		    psig = next_sig(psig);
                }
                if (found) break;
	    }
	}
    }
    if (!found)
    {
	char* tname = rb_id2name(orgpm->name);
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
    psig = pm->basic.method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pm->basic.arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 0);
	psig = next_sig(psig);
    }
    switch (pm->basic.result_signature)
    {
    case 'D':
      {
        INVOKEAD voked = *(INVOKEAD*)(((char*)*jenv) + pm->method);
	jv.d = voked(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
    case 'Z':
    case 'B':
      {
        INVOKEAZ vokez = *(INVOKEAZ*)(((char*)*jenv) + pm->method);
	jv.z = vokez(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
    case 'F':
      {
        INVOKEAF vokef = *(INVOKEAF*)(((char*)*jenv) + pm->method);
	jv.f = vokef(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
    case 'C':
    case 'S':
      {
        INVOKEAS vokes = *(INVOKEAS*)(((char*)*jenv) + pm->method);
	jv.s = vokes(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
#if HAVE_LONG_LONG
    case 'J':
      {
        INVOKEAL vokel = *(INVOKEAL*)(((char*)*jenv) + pm->method);
	jv.j = vokel(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
#endif
    default:
      {
        INVOKEA voke = *(INVOKEA*)(((char*)*jenv) + pm->method);
        jv.l = voke(jenv, ptr->obj, pm->basic.id, args);
      }
      break;
    }
    check_exception(jenv, 1);
    psig = pm->basic.method_signature;
    for (i = 0; i < argc; i++)
    {
	R2J pr2j = *(pm->basic.arg_convert + i);
	pr2j(jenv, argv[i], args + i, psig, 1);
	psig = next_sig(psig);
    }
    if (pm->basic.result_arraydepth)
    {
        return ja2r(pm->result_convert, jenv, jv, pm->basic.result_arraydepth);
    }
    else
    {
        return pm->result_convert(jenv, jv);
    }
}

/*
 * Object invocation
 */
static VALUE invoke_by_instance(ID rmid, int argc, VALUE* argv,
				struct jvi_data* ptr, char* sig)
{
    VALUE ret = Qnil;
    JNIEnv* jenv = attach_current_thread();
    struct cls_field* pf;
    struct cls_method* pm;
    char* tname = rb_id2name(rmid);
    
    if (argc == 0 && st_lookup(ptr->fields, rmid, (st_data_t*)&pf))
    {
        ret = getter(jenv, pf, ptr);
    }
    else if (argc == 1 && *(tname + strlen(tname) - 1) == '=')
    {
        char* fname = ALLOCA_N(char, strlen(tname) + 1);
	strcpy(fname, tname);
	fname[strlen(tname) - 1] = '\0';
	if (st_lookup(ptr->fields, rb_intern(fname), (st_data_t*)&pf))
	{
	    setter(jenv, pf, ptr, *argv);
        }
	else
	{
	    rb_raise(rb_eRuntimeError, "Fail: unknown field name `%s'", fname);
	}
    }
    else if (st_lookup(ptr->methods, rmid, (st_data_t*)&pm))
    {
	ret = invoke(jenv, pm, ptr, argc, argv, sig);
    }
    else
    {
	rb_raise(rb_eRuntimeError, "Fail: unknown method name `%s'", tname);
    }
    return ret;
}

static VALUE rjb_i_invoke(int argc, VALUE* argv, VALUE self)
{
    VALUE vsig, rmid, rest;
    char* sig;
    struct jvi_data* ptr;

    rb_scan_args(argc, argv, "2*", &rmid, &vsig, &rest);
    rmid = rb_to_id(rmid);
    sig = StringValueCStr(vsig);
    Data_Get_Struct(self, struct jvi_data, ptr);

    return invoke_by_instance(rmid, argc -2, argv + 2, ptr, sig);
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
    char* tname = rb_id2name(rmid);
    JNIEnv* jenv = attach_current_thread();

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
    VALUE vsig, rmid, rest;
    char* sig;
    struct jv_data* ptr;
    
    rb_scan_args(argc, argv, "2*", &rmid, &vsig, &rest);
    rmid = rb_to_id(rmid);
    sig = StringValueCStr(vsig);
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
    char* rmname = rb_id2name(rmid);
    if (isupper(*rmname))
    {
        VALUE r, args[2];
        int state = 0;
	args[0] = RBASIC(self)->klass;
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
    rb_protect((VALUE(*)(VALUE))rb_require, (VALUE)"iconv", NULL);

    loaded_classes = rb_hash_new();
    OBJ_FREEZE(loaded_classes);
    rb_global_variable(&loaded_classes);
    proxies = rb_ary_new();
    rb_global_variable(&proxies);
    
    rjb = rb_define_module("Rjb");
    rb_define_module_function(rjb, "load", rjb_s_load, -1);
    rb_define_module_function(rjb, "unload", rjb_s_unload, -1);
    rb_define_module_function(rjb, "import", rjb_s_import, 1);
    rb_define_module_function(rjb, "bind", rjb_s_bind, 2);
    rb_define_module_function(rjb, "classes", rjb_s_classes, 0);
    rb_define_module_function(rjb, "throw", rjb_s_throw, -1);
    rb_define_const(rjb, "VERSION", rb_str_new2(RJB_VERSION));

    /* Java class object */    
    rjbc = rb_class_new(rb_cObject);
    rb_gc_register_address(&rjbc);
    rb_define_method(rjbc, "method_missing", rjb_missing, -1);
    rb_define_method(rjbc, "_invoke", rjb_invoke, -1);
    rb_define_method(rjbc, "_classname", rjb_i_class, 0);    

    /* Java instance object */
    rjbi = rb_class_new(rb_cObject);
    rb_gc_register_address(&rjbi);
    rb_define_method(rjbi, "method_missing", rjb_i_missing, -1);    
    rb_define_method(rjbi, "_invoke", rjb_i_invoke, -1);
    rb_define_method(rjbi, "_classname", rjb_i_class, 0);

    /* Ruby-Java Bridge object */
    rjbb = rb_class_new(rb_cObject);
    rb_gc_register_address(&rjbb);
}

static VALUE safe_funcall(VALUE args)
{
    VALUE* argp = (VALUE*)args;
    return rb_funcall2(*argp, *(argp + 1), *(argp + 2), argp + 3);
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

    for (i = 0; i < RARRAY(proxies)->len; i++)
    {
	struct rj_bridge* ptr;
	VALUE val = RARRAY(proxies)->ptr[i];
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
            result = rb_protect(safe_funcall, (VALUE)argv, &sstat);
	    rv2jobject(jenv, result, &j, NULL, 0);
	    // I can't delete this object...
            break;
	}
    }
    return j.l;
}
