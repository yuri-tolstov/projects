// Copyright 2013 Tilera Corporation. All Rights Reserved.
//
//   The source code contained or described herein and all documents
//   related to the source code ("Material") are owned by Tilera
//   Corporation or its suppliers or licensors.  Title to the Material
//   remains with Tilera Corporation or its suppliers and licensors. The
//   software is licensed under the Tilera MDE License.
//
//   Unless otherwise agreed by Tilera in writing, you may not remove or
//   alter this notice or any other notice embedded in Materials by Tilera
//   or Tilera's suppliers or licensors in any way.
//
//
//

#ifndef _FFT_1024_H_
#define _FFT_1024_H_

#include <stdint.h>

void fft_1024_hack(uint32_t* restrict input,
                   uint32_t* restrict output,
                   int32_t * restrict twiddles,
                   uint32_t* restrict input_plus_256,
                   uint32_t* restrict input_plus_512,
                   uint32_t* restrict input_plus_768,
                   uint32_t* restrict output_plus_256,
                   uint32_t* restrict output_plus_512,
                   uint32_t* restrict output_plus_768,
                   int32_t * restrict twiddles_plus_249);

static inline
void fft_1024(uint32_t* restrict input,
              uint32_t* restrict output,
              int32_t * restrict twiddles) {
  // Currently the compiler treats pointers that are large offsets from a base
  // value sub-optimally.  If such a pointer is used in a loop, the compiler
  // generates add of large constants that is better done outside the loop.  To
  // work around this issue, we pass in the pointers with large offsets as
  // separate pointers.
  //
  fft_1024_hack(input, output, twiddles,
                input+256, input+512, input+768,
                output+256, output+512, output+768,
                twiddles+249);
}
 
#endif
