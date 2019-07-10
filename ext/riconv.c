/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004 Kuwashima
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
 * $Id: riconv.c 117 2010-06-04 12:16:25Z arton $
 */

#include "ruby.h"
#include "extconf.h"

#if defined _WIN32 || defined __CYGWIN__
#include <windows.h>
#endif

#if defined HAVE_NL_LANGINFO
#include <langinfo.h>
static const char* const NL_EUC_TABLE[] = { "EUC-JISX0213", "EUC-JP", "EUC-JP-MS" };
static const char* const NL_SJIS_TABLE[] = { "SHIFT_JIS", "SHIFT_JISX0213", "WINDOWS-31J" };
#endif

#if defined HAVE_SETLOCALE
#include <locale.h>
#endif
static const char* const LOCALE_EUC_TABLE[] = { "japanese", "ja_JP.eucJP", "japanese.euc", "ja_JP", "ja_JP.ujis" };
static const char* const LOCALE_SJIS_TABLE[] = { "japanese.sjis", "ja_JP.SJIS" };
static const char* const LOCALE_UTF8_TABLE[] = { "ja_JP.UTF-8", "ja_JP.utf8" };

#include "riconv.h"

static const char* const CS_EUCJP = "EUC-JP";
static const char* const CS_CP932 = "CP932";
static const char* const CS_SJIS = "SHIFT_JIS";
static const char* const CS_UTF8 = "UTF-8";


#if RJB_RUBY_VERSION_CODE < 190
static VALUE objIconvJ2R;
static VALUE objIconvR2J;
static const char* charcode; //is this necessary?
static char Kcode = '\0';

static int find_table(const char* const str, const char* const table[])
{
    int i;
    int size = sizeof(table) / sizeof(table[0]);
    for (i = 0; i < size; ++i)
    {
        if (strstr(str, table[i])) return 1;
    }
    return 0;
}
#endif

#if RJB_RUBY_VERSION_CODE < 190
static const char* get_charcode_name_by_locale(const char* const name)
{
    if (find_table(name, LOCALE_UTF8_TABLE))
        return NULL;
    else if (find_table(name, LOCALE_EUC_TABLE))
        return CS_EUCJP;
    else if (find_table(name, LOCALE_SJIS_TABLE))
        return CS_SJIS;
    else
        return NULL;
}
/*
 * Get better charcode name.
 */
static const char* get_charcode_name()
{
    const char* result = NULL;
    const char* lang = NULL;

    switch(Kcode)
    {
    case 'E':
        result = CS_EUCJP;
        break;
    case 'S':
#if defined _WIN32 || defined __CYGWIN__
        result = CS_CP932;
#else
        result = CS_SJIS;
#endif
        break;
    case 'U':
        //as is.
        break;
    case 'N':
    default:
#if defined _WIN32 || defined __CYGWIN__
        if (932 == GetACP()) result = CS_CP932;
#elif defined HAVE_NL_LANGINFO
        setlocale(LC_ALL, "C"); //initialize
        lang = nl_langinfo(CODESET);
        if (find_table(lang, NL_EUC_TABLE))
                result =  CS_EUCJP;
        else if (find_table(lang, NL_SJIS_TABLE))
                result = CS_SJIS;
#elif defined HAVE_SETLOCALE
        setlocale(LC_ALL, "C"); //initialize
        result = get_charcode_name_by_locale(setlocale(LC_ALL, NULL));
#elif defined HAVE_GETENV
        if (result = get_charcode_name_by_locale(getenv("LC_ALL")))
                ;
        else if (result = get_charcode_name_by_locale(getenv("LC_CTYPE")))
                ;
        else if (result = get_charcode_name_by_locale(getenv("LANG")))
                ;
#endif
        break;
    }
    return result;
}
#endif

#if RJB_RUBY_VERSION_CODE < 190
static void reinit()
{
    charcode = get_charcode_name();
    if (charcode)
    {
        VALUE rb_iconv_klass = rb_const_get(rb_cObject, rb_intern("Iconv"));
        if (RTEST(rb_iconv_klass)) {
            ID sym_new = rb_intern("new");
	    rb_gc_unregister_address(&objIconvR2J);
            objIconvR2J = rb_funcall(rb_iconv_klass, sym_new, 2, rb_str_new2(CS_UTF8), rb_str_new2(charcode));
	    rb_gc_register_address(&objIconvR2J);
	    rb_gc_unregister_address(&objIconvJ2R);
            objIconvJ2R = rb_funcall(rb_iconv_klass, sym_new, 2, rb_str_new2(charcode), rb_str_new2(CS_UTF8));
	    rb_gc_register_address(&objIconvJ2R);
        }
    }
    else
    {
        objIconvR2J = objIconvJ2R = Qnil;
    }
}
#endif

#if RJB_RUBY_VERSION_CODE < 190
static void check_kcode()
{
    VALUE rb_iconv_klass = rb_const_get(rb_cObject, rb_intern("Iconv"));
    VALUE kcode = rb_gv_get("$KCODE");
    if (RTEST(rb_iconv_klass) && TYPE(kcode) == T_STRING) {
        char* kcodep = StringValuePtr(kcode);
        char upper_kcode = toupper(*kcodep);
        if (Kcode != upper_kcode)
        {
            Kcode = upper_kcode;
            reinit();
        }
    }
    else
    {
        objIconvR2J = objIconvJ2R = Qnil;
    }
}
#else
VALUE cEncoding = Qnil;
VALUE encoding_utf8 = Qnil;
static void init_encoding_vars()
{
    cEncoding = rb_const_get(rb_cObject, rb_intern("Encoding"));
    encoding_utf8 = rb_const_get(cEncoding, rb_intern("UTF_8"));
}
static int contains_surrogate_pair(const unsigned char* p)
{
    while (*p)
    {
        switch (*p & 0xf0)
        {
        case 0xf0:
            return 1;
        case 0xe0:
            p += 3;
            break;
        default:
            p += (*p & 0x80) ? 2 : 1;
        }
    }
    return 0;
}
static int contains_auxchar(const unsigned char* p)
{
    while (*p)
    {
      if (*p == 0xed)
        {
#if defined(DEBUG)
          printf("find %02x %02x %02x %02x %02x %02x\n", *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5));
#endif
          return 1;
        }
        switch (*p & 0xe0)
        {
        case 0xe0:
            p++;
        case 0xc0:
            p++;
        default:
            p++;
        }
    }
    return 0;
}

static VALUE encode_to_cesu8(const unsigned char* p)
{
    size_t len = strlen(p);
    char* newstr = ALLOCA_N(char, len + (len + 1) / 2);
    char* dest = newstr;
    int sval, i;
    while (*p)
    {
        switch (*p & 0xf0)
        {
        case 0xf0:
            sval = *p++ & 7;
            for (i = 0; i < 3; i++)
            {
                sval <<= 6;
                sval |= (*p++ & 0x3f);
            }
            *dest++ = '\xed';
            *dest++ = 0xa0 | (((sval >> 16) - 1) & 0x0f);
            *dest++ = 0x80 | ((sval >> 10) & 0x3f);
            *dest++ = '\xed';
            *dest++ = 0xb0 | ((sval >> 6) & 0x0f);
            *dest++ = 0x80 | (sval & 0x3f);
            break;
        case 0xe0:
            *dest++ = *p++;
        case 0xc0:
        case 0xc1:          
            *dest++ = *p++;
        default:
            *dest++ = *p++;
        }
    }
    return rb_str_new(newstr, dest - newstr);
}
static VALUE encode_to_utf8(const unsigned char* p)
{
    size_t len = strlen(p);
    char* newstr = ALLOCA_N(char, len);
    char* dest = newstr;
    int sval, i;
    while (*p)
    {
        if (*p == 0xed)
        {
            char v = *(p + 1);
            char w = *(p + 2);
            char y = *(p + 4);
            char z = *(p + 5);
            p += 6;
            sval = 0x10000 + ((v & 0x0f) << 16) + ((w & 0x3f) << 10) + ((y & 0x0f) << 6) + (z & 0x3f);
            sval = (((v + 1) & 0x0f) << 16) + ((w & 0x3f) << 10) + ((y & 0x0f) << 6) + (z & 0x3f);
           *dest++ = 0xf0 | ((sval >> 18));
           *dest++ = 0x80 | ((sval >> 12) & 0x3f);
           *dest++ = 0x80 | ((sval >> 6) & 0x3f);
           *dest++ = 0x80 | (sval & 0x3f);
           continue;
        }
        switch (*p & 0xe0)
        {
        case 0xe0:
            *dest++ = *p++;          
        case 0xc0:
        case 0xc1:          
            *dest++ = *p++;                    
        default:
            *dest++ = *p++;                   
        }
    }
    return rb_str_new(newstr, dest - newstr);
}
#endif

#if defined(DEBUG)
static void debug_out(VALUE v)
{
    char* p = StringValuePtr(v);
    printf("-- %d, %d, %s\n", rb_num2long(rb_funcall(v, rb_intern("size"), 0)),
           strlen(p), p);
    fflush(stdout);
}
#else
#define debug_out(n)
#endif

VALUE exticonv_local_to_utf8(VALUE local_string)
{
#if RJB_RUBY_VERSION_CODE < 190
    check_kcode();
    if(RTEST(objIconvR2J))
    {
        return rb_funcall(objIconvR2J, rb_intern("iconv"), 1, local_string);
    }
    else
    {
        return local_string;
    }
#else
    VALUE encoding;
    if (NIL_P(cEncoding))
    {
      init_encoding_vars();
    }
    encoding = rb_funcall(local_string, rb_intern("encoding"), 0);
    if (encoding != encoding_utf8)
    {
        VALUE ret = rb_funcall(local_string, rb_intern("encode"), 2, encoding_utf8, encoding);
        debug_out(local_string);
        debug_out(ret);
        local_string = ret;
    }
    if (contains_surrogate_pair(StringValuePtr(local_string)))
    {
        local_string = encode_to_cesu8(StringValuePtr(local_string));
    }
    return local_string;
#endif
}

VALUE exticonv_utf8_to_local(VALUE utf8_string)
{
#if RJB_RUBY_VERSION_CODE < 190
    check_kcode();
    if(RTEST(objIconvR2J))
    {
        return rb_funcall(objIconvJ2R, rb_intern("iconv"), 1, utf8_string);
    }
    else
    {
        return utf8_string;
    }
#else
    if (NIL_P(cEncoding))
    {
        init_encoding_vars();
    }
    if (contains_auxchar(StringValuePtr(utf8_string)))
    {
        utf8_string = encode_to_utf8(StringValuePtr(utf8_string));
    }
    return rb_funcall(utf8_string, rb_intern("force_encoding"), 1, encoding_utf8);
#endif
}
