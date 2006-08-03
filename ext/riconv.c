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
 * $Id: riconv.c 2 2006-04-11 19:04:40Z arton $
 */

#include "ruby.h"
#include "riconv.h"

static const char* cs_eucjp = "EUC-JP";
static const char* cs_sjis = "Shift_JIS";
static const char* cs_cp932 = "CP932";
static const char* cs_ms932 = "MS932";
static const char* cs_utf8 = "UTF-8";

/* Regexp::matchのラッパー */
static int regxp_is_match(VALUE str, VALUE regxp)
{
    return rb_funcall(str, rb_intern("match"), 1, RREGEXP(regxp)) != Qnil ? 1 : 0 ;
}

/* RUBY_PLATFORMから類推してWindowsなら1を、そうでなければ0を返す */
static int platform_is_windows()
{
  VALUE platform = rb_const_get(rb_cObject, rb_intern("RUBY_PLATFORM"));
    return regxp_is_match(platform, rb_str_new2("msvcrt|mswin|bccwin|mingw|cygwin"))
      ? 1
      : 0;
}

/*
 * RUBY_PLATFORMと$KCODEを元にIconvで有効な適当な文字コード名を返す
 * TODO: 実行環境のLOCALE,LANG,LC_ALL等も参考にする
 */
static char* get_charset_name()
{
    char* result = NULL;
    ID id_Iconv = rb_intern("Iconv");
    
    if (rb_const_defined(rb_cObject, id_Iconv))
      {
        VALUE kcode = rb_gv_get("$KCODE");
        VALUE platform = rb_const_get(rb_cObject, rb_intern("RUBY_PLATFORM"));
        char* kcodep = (TYPE(kcode) == T_STRING) ? StringValuePtr(kcode) : "";

        switch(toupper(*kcodep))
        {
        case 'E':
            result = (char*)cs_eucjp;
            break;
        case 'S':
          if (platform_is_windows())
            {
              result = (char*)cs_cp932;
            }
          else
            {
              result = (char*)cs_sjis;
            }
          break;
        case 'N':
          if (platform_is_windows())
            {
              result = (char*)cs_cp932;
            }
          else
            {
                result = NULL;
            }
            break;
        case 'U':
        default:
            result = NULL;
            break;
        }
      }else{
        result = NULL;
      }
    return result;
}

/*
 * 拡張ライブラリのIconvがある場合は1,ない場合は0を返す
 */
static int has_exticonv()
{
	ID id_Iconv = rb_intern("Iconv");
        VALUE Iconv = rb_const_get(rb_cObject, id_Iconv);
	if(Iconv == Qnil)
	{
	        return 0;
	}
	else
	{
		return 1;
	}
}

VALUE exticonv_local_to_utf8(VALUE local_string)
{
	char* cs = get_charset_name();
	if(cs)
	{
		return exticonv_cc(local_string, cs, cs_utf8);
	}
	else
	{
		return local_string;
	}
}

VALUE exticonv_utf8_to_local(VALUE utf8_string)
{
	char* cs = get_charset_name();
	if(cs)
	{
		return exticonv_cc(utf8_string, cs_utf8, cs);
	}
	else
	{
		return utf8_string;
	}
}

VALUE exticonv_cc(VALUE original_string, const char* from, const char* to)
{
	return exticonv_vv(original_string, rb_str_new2(from), rb_str_new2(to));
}

VALUE exticonv_vv(VALUE original_string, VALUE from, VALUE to)
{
	if(has_exticonv())
	{
		ID id_Iconv = rb_intern("Iconv");
		ID id_iconv = rb_intern("iconv");
		VALUE Iconv = rb_const_get(rb_cObject, id_Iconv);
		return rb_ary_entry(rb_funcall(Iconv, id_iconv, 3, to, from, original_string), 0);
	}
	else
	{
		//iconvがなければそのまま返す
		return original_string;
	}
}
