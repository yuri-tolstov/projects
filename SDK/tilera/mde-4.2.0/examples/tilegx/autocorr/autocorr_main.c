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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <arch/cycle.h>
#include "autocorr.h"

// Reference c-code
void autocorr_reference(short ac[],short sd[], int N, int M)
{
  int i,k,sum;
  
  for (i = 0; i < M; i++)
  {
    sum = 0;
    for (k = M; k < N+M; k++)
      sum += sd[k]*sd[k-i];
    ac[i] = (sum >> 15);
  }
}

#define N (1024*2) //length of the input vector
#define M 64       //number of autocorrelation points

// HACK: align to 8 bytes so we can manipulate 64 bits a time.
int16_t input[N] __attribute__((aligned(8)));
int16_t output[M] __attribute__((aligned(8)));
int16_t output2[M] __attribute__((aligned(8)));

int main()
{
  int i;
  uint64_t cycle_start, cycle_end;
  
  // Make some data up
  for(i =0; i < N;i++)
    input[i] = 219*((float) rand()/RAND_MAX) + 16;

  // Generate reference result
  autocorr_reference(output, input, N - M, M);

  // Generate tilera result
  autocorr(output2, input, N - M, M);

  // Test for correctness
  for(i=0; i < M; i++)
  {
    if(output2[i] != output[i])
    {
      printf("MISTAKE i=%i, output=%i, output2=%i \n",
             i, output[i], output2[i]);
      return 1;
    }
  }

  printf("The autocorrelation is bit exact to the reference c-code \n");

  // Warm the cache
  autocorr(output2, input, N - M, M);

  // Check performance
  cycle_start =  get_cycle_count();
  autocorr(output2, input, N - M, M);
  cycle_end =  get_cycle_count();

  printf("The cycles for an M=%i N=%i autocorretation is %i cycles\n",
         M, N, ((int)(cycle_end - cycle_start)));

  return 0;
}
