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
// This is an example of a fir filter coded for the tilegx architecture.
//

#include <stdio.h>
#include <stdint.h>
#include "fir8tap.h"

void fir8Tap(const short *x,
             const short *h,
             short       *r,
             int n)
{ 
  // Input an output pointers
  int64_t *in  = (int64_t *) x;
  int64_t *out = (int64_t *) r;

  // Variables to hold input vector elements
  int64_t A, B, C, CC;

  int64_t sum0, sum1, sum2, sum3;

  long j;

  // We have a 64 bit processor which means 4 16bit samples in a register to
  // handle unalignment for the out[i+1],out[i+2], out[i+3] cases we make
  // copies of the filters with an offset of 1,2,3 16-bit values.

#define COMBINE4(a,b,c,d)                            \
  (((uint64_t) (a))       | ((uint64_t) (b) << 16) | \
   ((uint64_t) (c) << 32) | ((uint64_t) (d) << 48))

  //for the aligned x[i] case
  const int64_t t3t2t1t0 = COMBINE4(h[0], h[1], h[2], h[3]);
  const int64_t t7t6t5t4 = COMBINE4(h[4], h[5], h[6], h[7]);

  //for the unaligned case x[i+1]
  const int64_t t2t1t0_0 = COMBINE4(   0, h[0], h[1], h[2]);
  const int64_t t6t5t4t3 = COMBINE4(h[3], h[4], h[5], h[6]);
  const int64_t       t7 = COMBINE4(h[7],    0,    0,    0);

  //for the unaligned case x[i+2]
  const int64_t t1t0_0_0 = COMBINE4(   0,    0, h[0], h[1]);
  const int64_t t5t4t3t2 = COMBINE4(h[2], h[3], h[4], h[5]);
  const int64_t     t7t6 = COMBINE4(h[6], h[7],    0,    0);

  //for the unaligned case x[i+3]
  const int64_t t0_0_0_0 = COMBINE4(   0,    0,    0, h[0]);
  const int64_t t4t3t2t1 = COMBINE4(h[1], h[2], h[3], h[4]);
  const int64_t   t7t6t5 = COMBINE4(h[5], h[6], h[7],    0);

  A = *in++; // [ x3, x2,x1,x0]
  B = *in++; // [ x7, x6,x5,x4]
  C = *in++; // [x11,x10,x9,x8]

  // Get 4 16-bit output samples per loop iteration
  for (j = 0; j < (n>>2); j++)   
  {
    // Load the next word for the next loop iteration so we do not get a stall.
    CC = *in++; // [x15,x14,x13,x12]

    //sample x[i]
    sum0 = __insn_v2dotp(       A, t3t2t1t0);
    sum0 = __insn_v2dotpa(sum0, B, t7t6t5t4);

    //sample x[i+1]
    sum1 = __insn_v2dotp(       A, t2t1t0_0);
    sum1 = __insn_v2dotpa(sum1, B, t6t5t4t3);
    sum1 = __insn_v2dotpa(sum1, C,       t7);

    //sample x[i+2]
    sum2 = __insn_v2dotp(       A, t1t0_0_0);
    sum2 = __insn_v2dotpa(sum2, B, t5t4t3t2);
    sum2 = __insn_v2dotpa(sum2, C,     t7t6);

    //sample x[i+3]
    sum3 = __insn_v2dotp(       A, t0_0_0_0);
    sum3 = __insn_v2dotpa(sum3, B, t4t3t2t1);
    sum3 = __insn_v2dotpa(sum3, C,   t7t6t5);

    // Combine all 4 samples into 1 64-bit word and store it taps are scaled-up
    // by 2^15.
    sum0 >>= 15;
    sum1 >>= 15;
    sum2 >>= 15;
    sum3 >>= 15;

    sum0 = __insn_v2int_l(sum1, sum0);
    sum2 = __insn_v2int_l(sum3, sum2);
    *out++ = sum0 | sum2 << 32;

    // Next loop iteration has sample x[i+4] = x[4], no need to load the data
    // again since it is in registers.
    A = B;
    B = C;
    C = CC;
  }
}                       

