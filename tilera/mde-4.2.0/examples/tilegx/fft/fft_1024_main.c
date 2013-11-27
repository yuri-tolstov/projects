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

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <arch/cycle.h>
#include <arch/sim.h>

#include "fft_1024.h"
#include "fft_1024_twiddles.h"

// To handle digit reveral (also know as bit-reversal), one usually stores the
// input for the previous routine in permuted order.  Or if one is doing
// convolution between vector, reversal is not need.  If reversal is needed,
// here is the reversal input address in the header.  There are many ways of
// handling this in order to get digit reversal for free.
#include "fft_1024_input_rev.h"

/* HACK: align to 8 for fft_1024_hack() */
short fft_output[2048] __attribute__((aligned(8)));

int check_results_partial(short* ptr)
{
  short result[] = {
    -4571, 13354,
    6439, 12877,
    665, -20945,
    -30552, 24345,
    7938, -3610,
    32394, -1772,
    -5616, -15760,
    32488, 4459,
    20101, -2249,
    -6684, 25435,
    -6234, 6784,
    58, -6619,
    -23770, 5535,
    32266, -6551,
    8220, 23819,
    7192, -21683,
    -19948, -20875,
    -2239, 5311,
    12898, -1665,
    -19369, -14412,
    11688, 840,
    23177, -25643,
    5267, -13380,
    -8996, 20015,
    12344, -31927,
    493, -15089,
    8026, 3499,
    -9418, 21479,
    -31154, 15711,
    -22379, 26485,
    -11125, -28144,
    5811, 6663 };

  int failed = 0;
  for (int i = 0; i < 64; i ++)
  {
    if (ptr[i] != result[i]) {
      printf("mismatch index %d expected %d actual %d\n",
             i, ptr[i], result[i]);
      failed = 1;
    }
  }

  return failed;
}

int main()
{
  uint64_t cycle_start, cycle_end;
  int i = 0;

  // Make a 12-bit signed input ..... current input is 16-bit with lots off
  // Values close to MAX
  for(i=0; i < 1024 * 2; i++)
    fft_input_rev[i] >>= 4;

  // Compute the fft and check the result.
  fft_1024((uint32_t*) fft_input_rev, (uint32_t*) fft_output, twiddles);

  if (check_results_partial((short*)fft_output))
    return 1;

  // Warm the cache
  fft_1024((uint32_t*) fft_input_rev, (uint32_t*) fft_output, twiddles);

  // Measure the performance
  cycle_start = get_cycle_count();
  fft_1024((uint32_t*) fft_input_rev, (uint32_t*) fft_output, twiddles);
  cycle_end =  get_cycle_count();

  printf("The cycles for a 1024 point fft is %i cycles\n",
         ((int)(cycle_end - cycle_start)));

  return 0;
}


