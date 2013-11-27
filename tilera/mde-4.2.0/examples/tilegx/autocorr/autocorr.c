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
// This is an example of an autocorrelation fir filter coded for the tilegx
// architecture.
//

#include <stdint.h>
#include "autocorr.h"

void autocorr(short *output,short *input, int N, int M)
{
  // Variables to hold input vector elements
  int64_t A, B, C;
  int64_t sum0, sum1, sum2, sum3;

  // Pointers to make loads 64-bit
  int64_t *in;
  int64_t *in2;

  // Output is 32-bit
  int32_t *out = (int32_t *)output;

  // Loop counters
  long i, k;

  // Offset the input pointer
  input += M; // Initial offset passed in


  for (i = 0; i < M; i+=4) // M output postions
  {
    sum0 = 0;
    sum1 = 0;
    sum2 = 0;
    sum3 = 0;

    in   = (int64_t *)input;
    in2  = (int64_t *)&input[-i-4]; // i is always double word aligned

    C = *in2++; //[input[k-i-1], input[k-i-2], input[k-i-3], input[k-i-4]]
    A = *in++;  //[input[k+3]  , input[k+2]  , input[k+1]  , input[k]]
    B = *in2++; //[input[k-i+3], input[k-i+2], input[k-i+1], input[k-i]]

    // N - 16 bit compares starting at M
    for (k = 0; k < N; k+=4) // Do N 16-bit dot products
    {
      // Position x[i] -> input[k-i]
      sum0 = __insn_v2dotpa(sum0, A, B);

      // Position x[i+1] -> input[k-i-1]
      sum1 = __insn_v2dotpa(sum1, A, __insn_dblalign6(B, C));

      // Position x[i+2] -> input[k-i-2]
      sum2 = __insn_v2dotpa(sum2, A, __insn_dblalign4(B, C));

      // Position x[i+3] -> input[k-i-3]
      sum3 = __insn_v2dotpa(sum3, A, __insn_dblalign2(B, C));

      // For next loop iteration
      C = B;
      A = *in++;
      B = *in2++;
    }

    // Scale down the data
    sum0 >>=15;
    sum1 >>=15;
    sum2 >>=15;
    sum3 >>=15;

    // Write the 2 words out which is 4 16-bit samples
    *out++ = __insn_v2int_l(sum1, sum0);
    *out++ = __insn_v2int_l(sum3, sum2);
  }
}

