/*
 *  Copyright (c) 2015 Contributors as noted in the AUTHORS file
 *
 *  This file is part of binson-c, BINSON serialization format library in C.
 *
 *  binson-c is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU Lesser General Public License (LGPL) as published
 *  by the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, the Contributors give you permission to link
 *  this library with independent modules to produce an executable,
 *  regardless of the license terms of these independent modules, and to
 *  copy and distribute the resulting executable under terms of your choice,
 *  provided that you also meet, for each linked independent module, the
 *  terms and conditions of the license of that module. An independent
 *  module is a module which is not derived from or based on this library.
 *  If you modify this library, you must extend this exception to your
 *  version of the library.
 *
 *  binson-c is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/********************************************//**
 * \file binson_util.h
 * \brief Utility functions header file
 *
 * \author Alexander Reshniuk
 * \date 20/11/2015
 *
 ***********************************************/

#ifndef BINSON_UTIL_H_INCLUDED
#define BINSON_UTIL_H_INCLUDED

#include <stddef.h>

#include "binson_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Useful macros missing in C89
 */
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define UNUSED(x) (void)(x)   /* for unused variable suppression */

/**
 *  Argument stringization
 */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/**
 *  Conversion helpers (binson raw <-> C style)
 */
uint8_t   binson_util_get_significant_bytes( int64_t i );
size_t    binson_util_pack_integer( int64_t val, uint8_t *bbuf, bool expand_to_next_int );
size_t    binson_util_pack_double( double val, uint8_t *bbuf );

#ifdef __cplusplus
}
#endif

#endif /* BINSON_UTIL_H_INCLUDED */
