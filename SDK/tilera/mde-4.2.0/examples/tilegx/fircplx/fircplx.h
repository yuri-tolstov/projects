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

#ifndef _FIRCPLX_H_
#define _FIRCPLX_H_ 

void fircplx(
  const short *restrict x, // really complex input
  const short *restrict h, // complex int intput
  short *restrict r,       // complex output
  short nh,                // number of taps
  short nr                 // number of complex output samples
  );       

#endif
