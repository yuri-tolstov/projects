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
// This is an example of a complex fir filter coded for the tilegx
// architecture.
//

#include <stdint.h>
#include "fircplx.h"
#include "stdio.h"

void fircplx(                                                                   
  const short *restrict x, // really complex input
  const short *restrict h, // complex int intput
  short *restrict r,       // complex output
  short nh,                // number of taps
  short nr                 // number of complex output samples
  )       
{ 
  long  i, j;
  
  uint32_t A;  // complex int
  uint64_t h0; // complex int to hold tap
  uint32_t AA;
  uint64_t hh0;
  int32_t *in32;  
  int64_t *taps;
  int64_t  sum;

  // We need to do 64-bit loads which is 4-16 bit quantities = 2 complex-int
  // per loop iteration.

  // Note: nr is in terms of shorts which is confusing here
  for (i = 0; i < 2*nr; i += 2)
  {                                                               
    sum = 0;                                                                    
    in32 = (int32_t *)&x[i];
    taps = (int64_t *)&h[0];

    // This is for generic 32-bit aligned case the taps are scaled up by 2^15.
    for (j = 0; j < nh; j += 2)
    {                 
      h0 = *taps++; // 2 complex int values
      A  = *in32--; // 1 complex-int values
      AA = *in32--;   
      hh0 = h0 >> 32; 
      
      //do the 2-tap complex FIR filter
      sum = __insn_cmula(sum, A, h0);
      sum = __insn_cmula(sum, AA, hh0);
    } 

    // The cmula puts the real part as 32-bits and imaginary part as 32-bits.
    // We need scale both parts by 2^15 and then pack the result into a
    // complex-int.
    sum >>= 15;

    r[i]   =  sum; 
    r[i+1] =  sum >> (32);
  }
}
