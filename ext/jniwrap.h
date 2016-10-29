/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2006 Kuwashima
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
 * $Id: jniwrap.h 181 2012-01-28 05:28:41Z arton $
 */
#ifndef _Included_jniwrap
#define _Included_jniwrap
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && (defined(__CYGWIN32__) || defined(__MINGW32__))
 #if !defined(__int64)
  typedef long long __int64;
 #endif
#endif

#include <jni.h>

#ifdef __cplusplus
}
#endif
#endif                    
