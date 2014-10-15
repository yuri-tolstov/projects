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
// This is an example of a fft coded for the tilegx architecture.
//

#include <stdint.h>
#include <stdio.h>
#include <arch/cycle.h>
#include "fft_1024.h"

//
// This is a radix 4 kernel
//
// a = x0;
// b = w1*x1;
// c = w2*x2;
// d = w3*x3;
// 
// r0 = a+c;
// r1 = a-c;
// r2 = b+d;
// r3 = b-d;
// 
// x0 = ro+r2;
// x1 = r1-i*r3;
// x2 = r0-r2;
// x3 = r1+i*r3;
//

//    3 of these macros are called in a row
// -> 3 instructions for 2 CMPLY mults pipelined
#define TWIDDLE_MULT_TILEGX(A, B, C)       \
      tmp2 = __insn_cmulf(A >> 32, C);       \
      A    = __insn_cmulf(A, B);             \
      A    = __insn_bfins(A, tmp2, 32, 63);

#ifdef CONFIG_TIME
#define REPORT_TIME(ctr, cycle) \
  printf("Layer %d Cycles %lld\n", ctr++, get_cycle_count() - cycle); \
  cycle = get_cycle_count();
#else
#define REPORT_TIME(ctr, cycle)
#endif

//this should be 6-7 packed VLIW instructions
#define RADIX4_KERNEL_TILEGX(a, b, c, d)                \
       r0 = __insn_v2add(a,c);                            \
       r1 = __insn_v2sub(a,c);                            \
       r2 = __insn_v2add(b,d);                            \
       r3 = __insn_v2sub(b,d);                            \
       r3_swap_lower = __insn_cmul(r3,cmplx_i);           \
       r3_swap_upper = __insn_cmul(r3>>32,cmplx_i);       \
       ir3 = __insn_v4packsc(r3_swap_upper,r3_swap_lower); \
       a = __insn_v2add(r0,r2);                           \
       b = __insn_v2sub(r1,ir3);                          \
       c = __insn_v2sub(r0,r2);                           \
       d = __insn_v2add(r1,ir3);

void fft_1024_hack(uint32_t* restrict input,
                   uint32_t* restrict output,
                   int32_t * restrict twiddles,
                   uint32_t* restrict input_plus_256,
                   uint32_t* restrict input_plus_512,
                   uint32_t* restrict input_plus_768,
                   uint32_t* restrict output_plus_256,
                   uint32_t* restrict output_plus_512,
                   uint32_t* restrict output_plus_768,
                   int32_t * restrict twiddles_plus_249)
{
  uint64_t * restrict in  = (uint64_t *)input; 
  uint32_t * restrict out32 = (uint32_t *)output;
  uint64_t * restrict in2, * restrict in3, * restrict in4;
  uint64_t * restrict out, * restrict out2, * restrict out3, * restrict out4;

  uint64_t a, b, c, d;
  uint64_t r0, r1, r2, r3;
  uint64_t r3_swap_lower;
  uint64_t r3_swap_upper;
  uint64_t tmp1, tmp2;
  uint64_t ir3;

  long k;
  long j;
  long idx;
  uint32_t cmplx_i = 0x00010000; // i

  // Twiddles for level 2 of the fft
  int32_t w1, w2, w3, w4, w5, w6, w7, w8, w9;

#ifdef CONFIG_TIME
  uint64_t cycle = get_cycle_count();
  int ctr = 0;
#endif

  //*************************************************************
  // Layer 1 of the 1024 point fft
  // All the twiddle factors are 1 so we do not need them.
  // We are going to do 256 4-point ffts.

  REPORT_TIME(ctr, cycle);

  for (k = 0; k < 256; k++)
  {
    
    a = *in++; //[b|a]
    b = *in++; //[d|c]    

    tmp1 = __insn_v2add(a, b); //[ (b+d)|(a+c) ] [r2 | r0]
    tmp2 = __insn_v2sub(a, b); //[ (b-d)|(a-c) ] [r3 | r1]

    // Make ir3
    r3_swap_upper =  __insn_cmul(tmp2>>32, cmplx_i); 
    ir3           =  __insn_v4packsc(0, r3_swap_upper);

    r2 = tmp1 >> 32;

    // Store the 4 output samples 
    *out32++ = __insn_v2add(tmp1, r2); 
    *out32++ = __insn_v2sub(tmp2, ir3);      
    *out32++ = __insn_v2sub(tmp1, r2); 
    *out32++ = __insn_v2add(tmp2, ir3);      
  }

  REPORT_TIME(ctr, cycle);

  //*************************************************************
  // Layer 2
  // layer 2 64 - 16 point ffts q=2 L=16 r=64 L_=4

  // Twiddles for level 2 of the fft
  w1 = *twiddles++; 
  w2 = *twiddles++; 
  w3 = *twiddles++; 
  w4 = *twiddles++; 
  w5 = *twiddles++; 
  w6 = *twiddles++; 
  w7 = *twiddles++; 
  w8 = *twiddles++; 
  w9 = *twiddles++; 

  in  = (uint64_t *)output; 
  in2 = (uint64_t *)&output[4];
  in3 = (uint64_t *)&output[8];
  in4 = (uint64_t *)&output[12];

  out  = (uint64_t *)input; 
  out2 = (uint64_t *)&input[4];
  out3 = (uint64_t *)&input[8];
  out4 = (uint64_t *)&input[12];

  // Do 64 16-point FFTS 
  for( k =0; k < 64; k++)
  {
    uint64_t aa, bb, cc, dd;
    uint64_t aa2, bb2, cc2, dd2;
    //unroll 0 ----------------------

    //load the samples
    aa = *in;
    bb = *in2;
    cc = *in3;
    dd = *in4;
      
    //load the values
    aa2 = *(in+1);
    bb2 = *(in2+1);
    cc2 = *(in3+1);
    dd2 = *(in4+1);

    tmp2 = __insn_cmulf(bb>>32, w1);
    bb   = __insn_bfins(bb, tmp2, 32, 63);

    tmp2 = __insn_cmulf(cc>>32, w2);
    cc   = __insn_bfins(cc, tmp2, 32, 63);

    tmp2 = __insn_cmulf(dd>>32, w3);
    dd   = __insn_bfins(dd, tmp2, 32, 63);

    RADIX4_KERNEL_TILEGX(aa, bb, cc, dd);
   
    //unroll 1 ----------------------

    TWIDDLE_MULT_TILEGX(bb2, w4, w7);
    TWIDDLE_MULT_TILEGX(cc2, w5, w8);
    TWIDDLE_MULT_TILEGX(dd2, w6, w9);
        
    RADIX4_KERNEL_TILEGX(aa2, bb2, cc2, dd2);
    
    //store the output
    *out  = aa; 
    *out2 = bb;
    *out3 = cc;
    *out4 = dd;  

    //store the output samples
    *(out+1)  = aa2; 
    *(out2+1) = bb2;
    *(out3+1) = cc2;
    *(out4+1) = dd2;  
      
    //need to move over 16 complex samples per loop iteration
    in   += 8; 
    in2  += 8;
    in3  += 8;
    in4  += 8;
    out  += 8; 
    out2 += 8;
    out3 += 8;
    out4 += 8;
  }
   
  REPORT_TIME(ctr, cycle);

  //*************************************************************
  // Layer 3
  // layer 3 16 - 64 point ffts L=64 r=16 L_= 16
  in  = (uint64_t *) input; 
  in2 = (uint64_t *)&input[16];
  in3 = (uint64_t *)&input[32];
  in4 = (uint64_t *)&input[48];
  
  for(j=0; j< 16/2; j++)
  {   

    idx = 0;
    w1 = *twiddles++; 
    w2 = *twiddles++; 
    w3 = *twiddles++; 
    
    w4 = *twiddles++; 
    w5 = *twiddles++; 
    w6 = *twiddles++; 
      
    for(k=0; k < 16; k++)
    {
      a =  in[idx + j];
      b = in2[idx + j];
      c = in3[idx + j];
      d = in4[idx + j];

      TWIDDLE_MULT_TILEGX(b, w1, w4);
      TWIDDLE_MULT_TILEGX(c, w2, w5);
      TWIDDLE_MULT_TILEGX(d, w3, w6);

      RADIX4_KERNEL_TILEGX(a, b, c, d);

      in [idx + j] = a;
      in2[idx + j] = b;
      in3[idx + j] = c;
      in4[idx + j] = d;
      
      idx += 32; //64 samples
    }   
  }


  REPORT_TIME(ctr, cycle);

  //*************************************************************
  // Layer 4
  in   = (uint64_t *)input; 
  in2  = (uint64_t *)&input[64];
  in3  = (uint64_t *)&input[128];
  in4  = (uint64_t *)&input[192];
  
  for(j=0; j< 64/2; j++)
  {        
    w1 = *twiddles++; 
    w2 = *twiddles++; 
    w3 = *twiddles++;     
    w4 = *twiddles++; 
    w5 = *twiddles++; 
    w6 = *twiddles++;

    //----------------------------  
    a=in[j];
    b=in2[j];
    c=in3[j];
    d=in4[j];

    TWIDDLE_MULT_TILEGX(b, w1, w4);
    TWIDDLE_MULT_TILEGX(c, w2, w5);
    TWIDDLE_MULT_TILEGX(d, w3, w6);

    RADIX4_KERNEL_TILEGX(a, b, c, d);
      
    in[j]  = a;
    in2[j] = b;
    in3[j] = c;
    in4[j] = d;
    
    //----------------------------  
    a= in[256/2 +j];
    b=in2[256/2 +j];
    c=in3[256/2 +j];
    d=in4[256/2 +j];  

    TWIDDLE_MULT_TILEGX(b, w1, w4);
    TWIDDLE_MULT_TILEGX(c, w2, w5);
    TWIDDLE_MULT_TILEGX(d, w3, w6);

    RADIX4_KERNEL_TILEGX(a, b, c, d);
  
    in [256/2 + j] = a;
    in2[256/2 + j] = b;
    in3[256/2 + j] = c;
    in4[256/2 + j] = d;        
    
    //----------------------------  
    a= in[512/2 +j];
    b=in2[512/2 +j];
    c=in3[512/2 +j];
    d=in4[512/2 +j];   

    TWIDDLE_MULT_TILEGX(b, w1, w4);
    TWIDDLE_MULT_TILEGX(c, w2, w5);
    TWIDDLE_MULT_TILEGX(d, w3, w6);

    RADIX4_KERNEL_TILEGX(a, b, c, d);

    in [512/2 + j] = a;
    in2[512/2 + j] = b;
    in3[512/2 + j] = c;
    in4[512/2 + j] = d;
    
    //----------------------------  
    a =  in[768/2 + j];
    b = in2[768/2 + j];
    c = in3[768/2 + j];
    d = in4[768/2 + j];

    TWIDDLE_MULT_TILEGX(b, w1, w4);
    TWIDDLE_MULT_TILEGX(c, w2, w5);
    TWIDDLE_MULT_TILEGX(d, w3, w6);

    RADIX4_KERNEL_TILEGX(a, b, c, d);

    in [768/2 + j] = a;
    in2[768/2 + j] = b;
    in3[768/2 + j] = c;
    in4[768/2 + j] = d;        
  }
  
  REPORT_TIME(ctr, cycle);

  //*************************************************************
  // Layer 5
  // Glue the last 4 256-point FFTS into the final 1024 point FFT

  in  = (uint64_t *)input; 
  in2 = (uint64_t *)input_plus_256;
  in3 = (uint64_t *)input_plus_512;
  in4 = (uint64_t *)input_plus_768;

  out  = (uint64_t *)output; 
  out2 = (uint64_t *)output_plus_256;
  out3 = (uint64_t *)output_plus_512;
  out4 = (uint64_t *)output_plus_768;

  twiddles = twiddles_plus_249;

  for(j=0; j< 256/2; j++)
  {    
    w1 = *twiddles++; 
    w2 = *twiddles++; 
    w3 = *twiddles++; 
    w4 = *twiddles++; 
    w5 = *twiddles++; 
    w6 = *twiddles++; 

    a = *in++;
    b = *in2++;
    c = *in3++;
    d = *in4++;      

    TWIDDLE_MULT_TILEGX(b, w1, w4);
    TWIDDLE_MULT_TILEGX(c, w2, w5);
    TWIDDLE_MULT_TILEGX(d, w3, w6);

    RADIX4_KERNEL_TILEGX(a, b, c, d);

    *out++ = a; 
    *out2++ = b;
    *out3++ = c;
    *out4++ = d; 
  }
   
  REPORT_TIME(ctr, cycle);
}
 
