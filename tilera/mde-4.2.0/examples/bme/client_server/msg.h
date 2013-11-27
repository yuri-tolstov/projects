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
// Message definitions for the BME client-server example.  The example code
// uses the tmc_udn_send_<n>() routines to send messages built from individual
// words; an alternate approach might be to define a structure type for each
// message and then use tmc_udn_send_buffer().
//

//
// Mapping message.  This requests that the server tile map a page of the
// client's memory into its address space.  Contents:
//
// Word 0: EX_MSG_MAPPING
// Word 1: UDN address of the requesting tile
// Word 2: Physical address to map (low 32 bits)
// Word 3: Physical address to map (high 32 bits)
// Word 4: Hypervisor PTE for page (low 32 bits)
// Word 5: Hypervisor PTE for page (high 32 bits)
//
// Return message:
//
// Word 0: Zero if no error, else an error code
//
#define EX_MSG_MAPPING  1

//
// Processing message.  Contents:
//
// Word 0: EX_MSG_PROCESS
// Word 1: UDN address of the requesting tile
// Word 2: Offset within the previously mapped page of the input string
// Word 3: Length of the input string
// Word 4: Offset within the previously mapped page of the output string
//
// Return message:
//
// Word 0: Number of bytes processed, or a negative error code
//
#define EX_MSG_PROCESS  2
