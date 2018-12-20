/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004,2005,2006,2009,2010,2011 arton
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
 * $Id: load.c 180 2011-12-05 16:34:29Z arton $
 */

#include <stdlib.h>
#include <stdio.h>
#include "ruby.h"
#include "extconf.h"
#if RJB_RUBY_VERSION_CODE < 190
#include "intern.h"
#include "st.h"
#include "util.h"
#else
#include "ruby/intern.h"
#include "ruby/st.h"
#include "ruby/util.h"
#endif
#include "jniwrap.h"
#include "jp_co_infoseek_hp_arton_rjb_RBridge.h"
#include "rjb.h"

#define JVM_TYPE "client"
#define ALT_JVM_TYPE "classic"

#if defined(_WIN32) || defined(__CYGWIN__)
 #if defined(__CYGWIN__)
  #define JVMDLL "%s/jre/bin/%s/jvm.dll"
  #define DIRSEPARATOR '/'
 #else
  #define JVMDLL "%s\\jre\\bin\\%s\\jvm.dll"
  #define DIRSEPARATOR '\\'
  #if defined(_WIN64)
    #undef JVM_TYPE
    #define JVM_TYPE "server"
  #endif
 #endif
 #define CLASSPATH_SEP  ';'
#elif defined(__APPLE__) && defined(__MACH__)
  static char* JVMDLL = "%s/JavaVM";
  #define DIRSEPARATOR '/'
  #define CLASSPATH_SEP ':'
  #define HOME_NAME "/Home"
  #define HOME_NAME_LEN strlen(HOME_NAME)
  #define DEFAULT_HOME "/System/Library/Frameworks/JavaVM.framework"
#elif defined(_AIX)
  #define ARCH "ppc"
  #undef JVM_TYPE
  #define JVM_TYPE "j9vm"
#elif defined(__hpux)
  #define JVMDLL "%s/jre/lib/%s/%s/libjvm.sl"
  #define ARCH "PA_RISC"
  #undef JVM_TYPE
  #define JVM_TYPE "server"
  #define DIRSEPARATOR '/'
  #define CLASSPATH_SEP ':'
#else /* defined(_WIN32) || defined(__CYGWIN__) */
 #if defined(__sparc_v9__)
   #define ARCH "sparcv9"
 #elif defined(__sparc__)
   #define ARCH "sparc"
 #elif defined(__amd64__)
   #define ARCH "amd64"
   #undef JVM_TYPE
   #define JVM_TYPE "server"
 #elif defined(i586) || defined(__i386__)
  #define ARCH "i386"
 #elif defined(__arm__)
  #define ARCH "arm"
 #elif defined(__aarch64__)
   #define ARCH "aarch64"
   #undef JVM_TYPE
   #define JVM_TYPE "server"
 #endif
 #ifndef ARCH
  #include <sys/systeminfo.h>
 #endif
 #define JVMDLL "%s/jre/lib/%s/%s/libjvm.so"
 #define DIRSEPARATOR '/'
 #define CLASSPATH_SEP ':'
#endif

#if defined(__APPLE__) && defined(__MACH__)
 static char* CREATEJVM = "JNI_CreateJavaVM";
 static char* GETDEFAULTJVMINITARGS = "JNI_GetDefaultJavaVMInitArgs";
#else
 #define CREATEJVM "JNI_CreateJavaVM"
 #define GETDEFAULTJVMINITARGS "JNI_GetDefaultJavaVMInitArgs"
#endif

typedef int (JNICALL *GETDEFAULTJAVAVMINITARGS)(void*);
typedef int (JNICALL *CREATEJAVAVM)(JavaVM**, JNIEnv**, void*);


static VALUE jvmdll = Qnil;
static VALUE getdefaultjavavminitargsfunc;
static VALUE createjavavmfunc;

static const char* DLLibs[] = { "fiddle", "dl" };
static const char* DLNames[] = { "Fiddle", "DL" };

static VALUE safe_require(VALUE args)
{
    return rb_require(StringValueCStr(args));
}

static int open_jvm(char* libpath)
{
    int sstat;
    VALUE* argv;
    size_t i;
    int state;

#if defined(RUBINIUS)
    i = 1;
#else
    i = 0;
#endif
    for (; i < COUNTOF(DLLibs); i++) 
    {
        state = 0;
        rb_protect(safe_require, rb_str_new2(DLLibs[i]), &state);
#if !defined(RUBINIUS)
        if (state || !rb_const_defined_at(rb_cObject, rb_intern(DLNames[i])))
        {
            if (i > 0)
            {
                rb_raise(rb_eRuntimeError, "Constants DL and Fiddle is not defined.");
                return 0;
            }
        }
        else
#endif
        {
            sstat = 0;
            argv = ALLOCA_N(VALUE, 4);
            *argv = rb_const_get(rb_cObject, rb_intern(DLNames[i]));
            *(argv + 1) = rb_intern("dlopen");
            *(argv + 2) = 1;
            *(argv + 3) = rb_str_new2(libpath);
            jvmdll = rb_protect(rjb_safe_funcall, (VALUE)argv, &sstat);
            if (!sstat)
            {
                break;
            }
            else if (i > 0)
            {
                return 0;
            }
        }
    }
    /* get function pointers of JNI */
#if RJB_RUBY_VERSION_CODE < 190
    getdefaultjavavminitargsfunc = rb_funcall(rb_funcall(rb_funcall(jvmdll, rb_intern("[]"), 2, rb_str_new2(GETDEFAULTJVMINITARGS), rb_str_new2("IP")), rb_intern("to_ptr"), 0), rb_intern("to_i"), 0); 
    createjavavmfunc = rb_funcall(rb_funcall(rb_funcall(jvmdll, rb_intern("[]"), 2, rb_str_new2(CREATEJVM), rb_str_new2("IPPP")), rb_intern("to_ptr"), 0), rb_intern("to_i"), 0); 
#else
    getdefaultjavavminitargsfunc = rb_funcall(jvmdll, rb_intern("[]"), 1, rb_str_new2(GETDEFAULTJVMINITARGS));
    createjavavmfunc = rb_funcall(jvmdll, rb_intern("[]"), 1, rb_str_new2(CREATEJVM));
#endif
    return 1;
}

#if defined(__APPLE__) && defined(__MACH__)
static int file_exist(const char* dir, const char* file)
{
    VALUE path = rb_funcall(rb_cFile, rb_intern("join"), 2,
                            rb_str_new2(dir), rb_str_new2(file));
    VALUE ret = rb_funcall(rb_cFile, rb_intern("exist?"), 1, path);
    return RTEST(ret);
}
#endif

/*
 * not completed, only valid under some circumstances.
 */
static int load_jvm(const char* jvmtype)
{
    char* libpath;
    char* java_home;
    char* jh;

    jh = getenv("JAVA_HOME");
#if defined(__APPLE__) && defined(__MACH__)
    if (!jh)
    {
        jh = DEFAULT_HOME;
    }
    else
    {
        if (strlen(jh) > HOME_NAME_LEN)
        {
            size_t len = strlen(jh);
            char* p = ALLOCA_N(char, len + 8);
            jh = strcpy(p, jh);
            if (*(jh + len - 1) == '/')
            {
                --len;
                *(jh + len) = '\0';
            }
            if (strcasecmp(jh + len - HOME_NAME_LEN, HOME_NAME) == 0)
            {
                strcpy(p + len, "/..");
            }
        }
        if (!jvmtype && !file_exist(jh, "JavaVM"))
        {
            jh = DEFAULT_HOME;
        }
    }
#endif
    if (!jh)
    {
        if (RTEST(ruby_verbose))
        {
            fprintf(stderr, "no JAVA_HOME environment\n");
        }
        return 0;
    }
#if defined(_WIN32)
    if (*jh == '"' && *(jh + strlen(jh) - 1) == '"')
    {
        char* p = ALLOCA_N(char, strlen(jh) + 1);
        strcpy(p, jh + 1);
        *(p + strlen(p) - 1) = '\0';
        jh = p;
    }
#endif    
    java_home = ALLOCA_N(char, strlen(jh) + 1);
    strcpy(java_home, jh);
    if (*(java_home + strlen(jh) - 1) == DIRSEPARATOR)
    {
	*(java_home + strlen(jh) - 1) = '\0';
    }
#if defined(_WIN32) || defined(__CYGWIN__)
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home)
		       + strlen(jvmtype) + 1);
    sprintf(libpath, JVMDLL, java_home, jvmtype);
#elif defined(__APPLE__) && defined(__MACH__)
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home) + 1);
    sprintf(libpath, JVMDLL, java_home);
#else /* not Windows / MAC OS-X */
    libpath = ALLOCA_N(char, sizeof(JVMDLL) + strlen(java_home)
		       + strlen(ARCH) + strlen(jvmtype) + 1);
    sprintf(libpath, JVMDLL, java_home, ARCH, jvmtype);
#endif
    return open_jvm(libpath);
}

static int load_bridge(JNIEnv* jenv)
{
    JNINativeMethod nmethod[1];
    jbyte buff[8192];
    char* bridge;
    size_t len;
    FILE* f;
#if defined(RUBINIUS)
    VALUE v = rb_const_get(rb_cObject, rb_intern("RjbConf"));
    v = rb_const_get(v, rb_intern("BRIDGE_FILE"));
#else
    VALUE v = rb_const_get_at(rb_const_get(rb_cObject, rb_intern("RjbConf")), 
			      rb_intern("BRIDGE_FILE"));
#endif
    bridge = StringValuePtr(v);
#if defined(DOSISH)
    bridge = ALLOCA_N(char, strlen(bridge) + 8);
    strcpy(bridge, StringValuePtr(v));
    for (len = 0; bridge[len]; len++)
    {
	if (bridge[len] == '/')
	{
	    bridge[len] = '\\';
	}
    }
#endif
    f = fopen(bridge, "rb");
    if (f == NULL)
    {
	return -1;
    }
    len = fread(buff, 1, sizeof(buff), f);
    fclose(f);
    rjb_rbridge = (*jenv)->DefineClass(jenv,
            "jp/co/infoseek/hp/arton/rjb/RBridge", get_systemloader(jenv), buff, len);
    if (rjb_rbridge == NULL)
    {
	rjb_check_exception(jenv, 1);
    }
    rjb_register_bridge = (*jenv)->GetMethodID(jenv, rjb_rbridge, "register",
			   "(Ljava/lang/Class;)Ljava/lang/Object;");
    nmethod[0].name = "call";
    nmethod[0].signature = "(Ljava/lang/String;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;";
    nmethod[0].fnPtr = Java_jp_co_infoseek_hp_arton_rjb_RBridge_call;
    (*jenv)->RegisterNatives(jenv, rjb_rbridge, nmethod, 1);
    rjb_rbridge = (*jenv)->NewGlobalRef(jenv, rjb_rbridge);
    return 0;
}

void rjb_unload_vm()
{
    if (RTEST(jvmdll))
    {
        rb_funcall(jvmdll, rb_intern("close"), 0);
        jvmdll = Qnil;
    }
}

int rjb_create_jvm(JNIEnv** pjenv, JavaVMInitArgs* vm_args, char* userpath, VALUE argv)
{
    static JavaVMOption soptions[] = {
#if defined(__sparc_v9__) || defined(__sparc__)
      { "-Xusealtsigs", NULL },
#elif defined(linux) || defined(__linux__)
      { "-Xrs", NULL },
#elif defined(__APPLE__) && defined(_ARCH_PPC)
      { "-Xrs", NULL },
#endif
#if defined(SHOW_JAVAGC)
      { "-verbose:gc", NULL },
#endif
      { "DUMMY", NULL },   /* for classpath count */
    };
    char* newpath;
    size_t len;
    int result;
    GETDEFAULTJAVAVMINITARGS initargs;
    CREATEJAVAVM createjavavm;
    JavaVMOption* options;
    size_t optlen;
    size_t i;
    VALUE optval;

    if (!RTEST(jvmdll))
    {
        char* libjvm = getenv("JVM_LIB");
#if defined(_WIN32)
        if (libjvm && *libjvm == '"' && *(libjvm + strlen(libjvm) - 1) == '"')
        {
            char* p = ALLOCA_N(char, strlen(libjvm) + 1);
            strcpy(p, libjvm + 1);
            *(p + strlen(p) - 1) = '\0';
            libjvm = p;
        }
#endif    
        if (libjvm == NULL || !open_jvm(libjvm))
        {
#if defined(__APPLE__) && defined(__MACH__)
            if (!(load_jvm(NULL)))
            {
                JVMDLL = "%s/Libraries/libjvm.dylib";
                CREATEJVM = "JNI_CreateJavaVM_Impl";
                GETDEFAULTJVMINITARGS = "JNI_GetDefaultJavaVMInitArgs_Impl";
#endif
	        if (!(load_jvm(JVM_TYPE) || load_jvm(ALT_JVM_TYPE)))
                {
                    return -1;
                }
#if defined(__APPLE__) && defined(__MACH__)
            }
#endif
        }
 
#if RJB_RUBY_VERSION_CODE < 190 && !defined(RUBINIUS)
        ruby_errinfo = Qnil;
#else
        rb_set_errinfo(Qnil);
#endif
    }

    if (NIL_P(getdefaultjavavminitargsfunc))
    {
	return -1;
    }
    initargs = (GETDEFAULTJAVAVMINITARGS)NUM2ULONG(getdefaultjavavminitargsfunc);
    result = initargs(vm_args);
    if (0 > result)
    {
        return result;
    }
    len = strlen(userpath);
    if (getenv("CLASSPATH"))
    {
        len += strlen(getenv("CLASSPATH"));
    }
    newpath = ALLOCA_N(char, len + 32);
    if (getenv("CLASSPATH"))
    {
        sprintf(newpath, "-Djava.class.path=%s%c%s",
		userpath, CLASSPATH_SEP, getenv("CLASSPATH"));
    }
    else
    {
        sprintf(newpath, "-Djava.class.path=%s", userpath);
    }
    optlen = 0;
    if (!NIL_P(argv))
    {
        optlen += RARRAY_LEN(argv);
    }
    optlen += COUNTOF(soptions);
    options = ALLOCA_N(JavaVMOption, optlen);
    options->optionString = newpath;
    options->extraInfo = NULL;
    for (i = 1; i < COUNTOF(soptions); i++)
    {
	*(options + i) = soptions[i - 1];
    }
    for (; i < optlen; i++)
    {
        optval = rb_ary_entry(argv, i - COUNTOF(soptions));
	Check_Type(optval, T_STRING);
	(options + i)->optionString = StringValueCStr(optval);
	(options + i)->extraInfo = NULL;
    }
    vm_args->nOptions = (int)optlen;
    vm_args->options = options;
    vm_args->ignoreUnrecognized = JNI_TRUE;
    if (NIL_P(createjavavmfunc))
    {
	return -1;
    }
    createjavavm = (CREATEJAVAVM)NUM2ULONG(createjavavmfunc);
    result = createjavavm(&rjb_jvm, pjenv, vm_args);
    if (!result)
    {
	result = load_bridge(*pjenv);
        if (RTEST(ruby_verbose) && result < 0)
        {
            fprintf(stderr, "failed to load the bridge class\n");
        }
    }
    return result;
}
