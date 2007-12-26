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
 * $Id$
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
static const char* const LOCALE_EUC_TABLE[] = { "japanese", "ja_JP.eucJP", "japanese.euc", "ja_JP", "ja_JP.ujis" };
static const char* const LOCALE_SJIS_TABLE[] = { "japanese.sjis", "ja_JP.SJIS" };
static const char* const LOCALE_UTF8_TABLE[] = { "ja_JP.UTF-8", "ja_JP.utf8" };
#endif

#include "riconv.h"

static const char* const CS_EUCJP = "EUC-JP";
static const char* const CS_CP932 = "CP932";
static const char* const CS_SJIS = "SHIFT_JIS";
static const char* const CS_UTF8 = "UTF-8";


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
        setlocale(LC_ALL, ""); //initialize
        lang = nl_langinfo(CODESET);
        if (find_table(lang, NL_EUC_TABLE))
                result =  CS_EUCJP;
        else if (find_table(lang, NL_SJIS_TABLE))
                result = CS_SJIS;
#elif defined HAVE_SETLOCALE
        setlocale(LC_ALL, ""); //initialize
        result = get_charcode_name_by_locale(setlocale(LC_ALL, NULL));
#elif defined HAVE_GETENV
        printf("HAVE_GETENV\n");
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
    VALUE rb_cEncoding, default_external, encoding, ascii8bit;
    rb_cEncoding = rb_const_get(rb_cObject, rb_intern("Encoding"));
    default_external = rb_funcall(rb_cEncoding, rb_intern("default_external"), 0);
    ascii8bit = rb_const_get(rb_cEncoding, rb_intern("ASCII_8BIT"));
    encoding = rb_funcall(local_string, rb_intern("encoding"), 0);

    if (ascii8bit == encoding && ascii8bit == default_external)
    {
	return local_string; //don't know default encoding
    }
    else if (encoding == ascii8bit)
    {
	local_string = rb_funcall(local_string, rb_intern("force_encoding"), 1, default_external);
    }
    return rb_funcall(local_string, rb_intern("encode"), 1, rb_str_new2("utf-8"));
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
    VALUE rb_cEncoding, encoding, ascii8bit;
    rb_cEncoding = rb_const_get(rb_cObject, rb_intern("Encoding"));
    encoding = rb_funcall(rb_cEncoding, rb_intern("default_external"), 0);
    ascii8bit = rb_const_get(rb_cEncoding, rb_intern("ASCII_8BIT"));

    utf8_string = rb_funcall(utf8_string, rb_intern("force_encoding"), 1, rb_str_new2("utf-8"));
    if (ascii8bit == encoding)
    {
	return utf8_string; //don't know default encoding
    }
    else
    {
        return rb_funcall(utf8_string, rb_intern("encode"), 1, encoding);
    }
#endif
}
