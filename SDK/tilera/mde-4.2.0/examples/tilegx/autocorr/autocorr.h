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

#ifndef _AUTOCORR_H_
#define _AUTOCORR_H_ 

// output --- Resulting array of autocorrelation 
// input  --- Input array of autocorrelation
// N      --- Length of input (MULTIPLE of 8)
// M      --- Length of autocorrelation (MULTIPLE of 2)
void autocorr(short * output,short * input, int N, int M); 

#endif
