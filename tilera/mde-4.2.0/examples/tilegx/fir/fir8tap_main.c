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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arch/cycle.h>
#include "fir8tap.h"

// C reference code
void fir8Tap_reference(const short *x,
                       const short *h,
                       short       *r,
                       int n)
{                                                                   
  int i, j, sum;
  
  for (j = 0; j < n; j++)
  {                                                               
    sum = 0;
    for (i = 0; i < 8; i++)
      sum += x[i + j] * h[i];
    r[j] = (short)(sum >> 15);
  }
}                       

#define N (1024*2)

// HACK: align to 8 bytes so we can manipulate 64 bits a time.
int16_t input[N+8] __attribute__((aligned(8)));
int16_t output[N] __attribute__((aligned(8)));
int16_t output2[N] __attribute__((aligned(8)));

int main()
{
  int i;
  int scale = 32768; //2^15
  uint64_t cycle_start, cycle_end;
 
  // Create 8 taps as a filter
  short taps[] = { scale/2, scale/3, scale/4, scale/5,
                   scale/6, scale/7, scale/8, scale/9 };

  // Make some data up
  for(i =0; i < N+8;i++)
    input[i] = 219*((float) rand()/RAND_MAX) + 16;

  // Check for correctness
  fir8Tap_reference(input, taps, output, N);

  fir8Tap(input, taps, output2, N);

  for(i=0; i < N; i++)
  {
    if(output2[i] != output[i])
    {
      printf("MISTAKE i=%i, output=%i, output2=%i \n",
             i, output[i], output2[i]);
      return 1;
    }
  }

  printf("The Fir filter is bit exact to the reference c-code \n");

  // Warm the cache
  fir8Tap(input, taps, output2, N);

  // Check performance
  cycle_start = get_cycle_count();
  fir8Tap(input, taps, output2, N);
  cycle_end = get_cycle_count();

  printf("Total cycles is %i cycles for N=%i point 8 tap filter \n",   
         ((int)(cycle_end - cycle_start)), N);

  printf("The average cycles for an 8-tap output  is %i cycles\n",
         ((int)(cycle_end - cycle_start)) / N);

  return 0;
}
