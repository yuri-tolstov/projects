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
#include "fircplx.h"

#define N (2048*2)    // length of the complex input vector
int16_t input  [2*N]; // complex input
int16_t output1[2*N]; // complex output
int16_t output2[2*N]; // complex output

void fircplx_reference(
  const short *restrict x, // complex int
  const short *restrict h, // complex int
  short *restrict r,       // output
  short nh,
  short nr)       
{                                                                   
  short i,j;                                                      
  int imag, real;                                                 
  
  for (i = 0; i < 2*nr; i += 2)                                   
  {                                                               
    imag = 0;                                                   
    real = 0;                                                   
    for (j = 0; j < 2*nh; j += 2)                               
    {                                                           
      real += h[j] * x[i-j]   - h[j+1] * x[i+1-j];            
      imag += h[j] * x[i+1-j] + h[j+1] * x[i-j];                  
    }                                                           

    r[i]   = (real >> 15);                                      
    r[i+1] = (imag >> 15);                                      
  }                                                               
}          
  
int main()
{

  int i;
  uint64_t cycle_start, cycle_end;
  const int s = 32768; //2^15
  const int ntaps = 64;

  uint16_t real, imag;

  // Make some data up
  for(i = 0; i < N; i++)
  {
    real = 219 * ((float) rand() / RAND_MAX) + 16;
    imag = 219 * ((float) rand() / RAND_MAX) + 16;

    input[2 * i]     = real;
    input[2 * i + 1] = imag;
  
  }

  // Complex filter -> 64 taps altogther here
  short h1[] = {
    s/2,  s/3,  s/4,  s/5,  s/6,  s/7,  s/8,  s/9,
    s/10, s/11, s/12, s/13, s/14, s/15, s/16, s/17,
    s/18, s/19, s/20, s/21, s/22, s/23, s/24, s/25,
    s/26, s/27, s/28, s/29, s/30, s/31, s/32, s/33,
    s/34, s/35, s/36, s/37, s/38, s/39, s/40, s/41,
    s/42, s/43, s/44, s/45, s/46, s/47, s/48, s/49,
    s/50, s/51, s/52, s/53, s/54, s/55, s/56, s/57,
    s/58, s/59, s/60, s/61, s/62, s/63, s/64, s/65,
    s/2,  s/3,  s/4,  s/5,  s/6,  s/7,  s/8,  s/9,
    s/10, s/11, s/12, s/13, s/14, s/15, s/16, s/17,
    s/18, s/19, s/20, s/21, s/22, s/23, s/24, s/25,
    s/26, s/27, s/28, s/29, s/30, s/31, s/32, s/33,
    s/34, s/35, s/36, s/37, s/38, s/39, s/40, s/41,
    s/42, s/43, s/44, s/45, s/46, s/47, s/48, s/49,
    s/50, s/51, s/52, s/53, s/54, s/55, s/56, s/57,
    s/58, s/59, s/60, s/61, s/62, s/63, s/64, s/65,
  };

  // Check for correctness
  fircplx_reference(input, h1, output1, ntaps, N);

  // Tilera function
  fircplx(input, h1, output2, ntaps, N);

  for(i =0 ; i < 2 * N; i += 1)
  {
    if(output1[i] != output2[i])
    {
      printf("MISTAKE i=%i  output1=0x%0x, output2=0x%0x \n",
             i, output1[i], output2[i]);
      return 1;
    }
  }

  printf("The fircplx is bit exact to the reference c-code \n");

  // Warm the cache
  fircplx(input, h1, output1, ntaps, N);

  // Check performance
  cycle_start = get_cycle_count();
  fircplx(input, h1, output1, ntaps, N);
  cycle_end = get_cycle_count();

  printf("The cycles for an  N=%i %d-tap fircplx is %i cycles\n",
         N, ntaps, ((int)(cycle_end - cycle_start))); 

  printf("The average cycles for an %d-tap output  is %i cycles\n",
         ntaps, ((int)(cycle_end - cycle_start)) / N);


  return 0;
}
