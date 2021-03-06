/* Copyright 2013-14 JP Norair
  *
  * Licensed under the OpenTag License, Version 1.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.indigresso.com/wiki/doku.php?id=opentag:license_1_0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  */
/**
  * @file       /include/platform/memcpy.h
  * @author     JP Norair
  * @version    R101
  * @date       27 Mar 2014
  * @brief      Memcpy, Memset interfaces from the Platform
  * @ingroup    Platform
  *
  * The user should include <otlib/memcpy.h> which will selectively use the
  * platform implementation or the string.h implementation based on the build
  * parameters.
  *
  ******************************************************************************
  */


#ifndef __PLATFORM_MEMCPY_H
#define __PLATFORM_MEMCPY_H

#include <otsys/types.h>
#include <otsys/config.h>
#include <app/build_config.h>


/** @brief platform-specific memcpy, in some cases wraps to OS-level memcpy
  * @param  dest        (void*) destination memory address
  * @param  src         (void*) source memory address
  * @param  length      (ot_uint) number of bytes to transfer/copy
  * @retval None
  * @ingroup Platform
  * @sa ot_memcpy2()
  * @sa ot_memcpy4()
  * @sa ot_memset()
  *
  * ot_memcpy() is the generic implementation of memcpy, which must
  * handle byte-aligned memory copies.
  *
  * ot_memcpy2() and ot_memcpy4() are two and four byte aligned
  * variants of ot_memcpy()
  */
void ot_memcpy(ot_u8* dst, ot_u8* src, ot_uint length);
void ot_memcpy2(ot_u16* dst, ot_u16* src, ot_uint length);
void ot_memcpy4(ot_u32* dst, ot_u32* src, ot_uint length);



/** @brief platform-specific memset, in some cases wraps to OS-level memset
  * @param  dest        (void*) destination memory address
  * @param  value       (ot_u8) byte to put into memory (repeatedly)
  * @param  length      (ot_uint) number of bytes to set
  * @retval None
  * @ingroup Platform
  * @sa memcpy()
  */
void ot_memset(ot_u8* dst, ot_u8 value, ot_uint length);
void ot_memset2(ot_u16* dst, ot_u16 value, ot_uint length);
void ot_memset4(ot_u32* dst, ot_u32 value, ot_uint length);



#endif
