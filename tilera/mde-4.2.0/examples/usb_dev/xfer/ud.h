//
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
// USB device tile-side example.
//

#include <gxio/usb_dev.h>

//
// You can define NDEBUG to disable the debug feature.
//
#ifdef NDEBUG
#define TRACE(...) do {} while(0)
#else
extern int debug;
#define TRACE(...)  do { if (debug) printf(__VA_ARGS__); } while (0)
#endif

void proc(int cpu);
