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
#include "riconv.h"

static const char* const cs_eucjp = "EUC-JP";
static const char* const cs_sjis = "Shift_JIS";
static const char* const cs_cp932 = "CP932";
static const char* const cs_ms932 = "MS932";
static const char* const cs_utf8 = "UTF-8";

static VALUE objIconvJ2R;
static VALUE objIconvR2J;
static const char* charcode; //is this necessary?
static char Kcode = 0; //

/* wrap Regexp::match */
static int regxp_is_match(VALUE str, VALUE regxp)
{
    return rb_funcall(str, rb_intern("match"), 1, RREGEXP(regxp)) != Qnil ? 1 : 0 ;
}

/* result true if windows */
static int platform_is_windows()
{
    VALUE platform = rb_const_get(rb_cObject, rb_intern("RUBY_PLATFORM"));
    return regxp_is_match(platform, rb_str_new2("msvcrt|mswin|bccwin|mingw|cygwin"))
	? 1
	: 0;
}

/*
 * Get better charcode name
 */
static const char* get_charcode_name()
{
    const char* result = NULL;

    switch(Kcode)
    {
	case 'E':
	    result = cs_eucjp;
	    break;
	case 'S':
	    if (platform_is_windows())
	    {
		result = cs_cp932;
	    }
	    else
	    {
		result = cs_sjis;
	    }
	    break;
	case 'N':
	    if (platform_is_windows())
	    {
		//for ruby1.8. YARV support better m17n.
		result = cs_cp932;
	    }
	    else
	    {
		result = cs_utf8;
	    }
	    break;
	case 'U':
	default:
	    result = cs_utf8;
	    break;
    }
    return result;
}


static void reinit()
{
    VALUE rb_iconv_klass = rb_const_get(rb_cObject, rb_intern("Iconv"));
    ID sym_new = rb_intern("new");
    charcode = get_charcode_name();
    objIconvR2J = rb_funcall(rb_iconv_klass, sym_new, 2, rb_str_new2(cs_utf8), rb_str_new2(charcode));
    objIconvJ2R = rb_funcall(rb_iconv_klass, sym_new, 2, rb_str_new2(charcode), rb_str_new2(cs_utf8));
}

static void check_kcode()
{
    VALUE kcode = rb_gv_get("$KCODE");
    if (TYPE(kcode) == T_STRING) {
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
    	Kcode = 0;
	objIconvR2J = objIconvJ2R = Qnil;
    }
}

VALUE exticonv_local_to_utf8(VALUE local_string)
{
    check_kcode();
    if(Kcode && Kcode != 'U' && RTEST(objIconvR2J))
    {
	return rb_funcall(objIconvR2J, rb_intern("iconv"), 1, local_string);
    }
    else
    {
	return local_string;
    }
}

VALUE exticonv_utf8_to_local(VALUE utf8_string)
{
    check_kcode();
    if(Kcode && Kcode != 'U' && RTEST(objIconvJ2R))
    {
	return rb_funcall(objIconvJ2R, rb_intern("iconv"), 1, utf8_string);
    }
    else
    {
	return utf8_string;
    }
}
