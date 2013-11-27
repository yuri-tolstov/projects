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
// USB device example: data shared between host side and tile side
//

/** Vendor ID. */
#define UD_DEV_VENDOR  0x22dc  // Tilera Corporation

/** Product ID. */
#define UD_DEV_PRODUCT 0x1234

/** Our special control request. */
#define UD_SET_FILE    100
