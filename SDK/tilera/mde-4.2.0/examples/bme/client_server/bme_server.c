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
// BME server side of the BME client-server example.  This program waits for
// UDN messages from the client, running under Linux, and acts upon them.
//

#include <stdint.h>
#include <stdio.h>

#include <arch/spr.h>

#include <bme/tlb.h>
#include <bme/tte.h>
#include <bme/types.h>

#include <tmc/udn.h>

#include "msg.h"

//
//  Process an input string.  The processing for this example is rot-47
//  encryption; each printable ASCII character is replaced by the character
//  47 slots later in the ASCII table, while other characters are unchanged.
//
// @param out Pointer to the output string.
// @param in Pointer to the input string.
// @param len Length of the input string.
// @return Number of bytes processed.
//
int
process(char* out, char* in, int len)
{
  int retlen = len;

  while (len--)
    if (*in >= '!' && *in <= '~')
      *out++ = '!' + (*in++ - '!' + 47) % 94;
    else
      *out++ = *in++;

  return (retlen);
}


//
// Main routine.
//
int
main(int argc, char** argv)
{
  //
  // Announce ourselves.  tprintf() takes printf-style arguments but also
  // prints the calling tile's coordinates at the beginning of its output.
  //
  tprintf("server: my UDN coordinates are (%d,%d)\n",
          udn_tile_coord_x(), udn_tile_coord_y());

#if !CHIP_HAS_REV1_XDN()
  //
  // Configure the UDN tags so that we'll get the messages intended for us.
  //
  __insn_mtspr(SPR_UDN_TAG_0, UDN0_DEMUX_TAG);
#endif

  //
  // We pick a random virtual address at which to map the shared page.  This
  // must not conflict with any of the application's other code or data
  // segments, and must be aligned so that we can map whatever page size the
  // client gives us.
  //
  VA va = 0x40000000;
  char* buf_va = (char*) va;
  int buf_mapped = 0;

  //
  // Process messages forever.
  //
  while (1)
  {
    //
    // Get the first word of the message, which tells us the type, then
    // receive the remainder and process it depending upon the type.
    //
    uint32_t msg = udn0_receive();

    switch (msg)
    {
    case EX_MSG_MAPPING:
    {
      //
      // This is a request to map the shared buffer into our address space.
      // Extract the parameters from the message.  Note that some of the
      // parameters are 64-bit values; we must receive their two halves
      // separately and reassemble them.
      //
      DynamicHeader from = { .word = udn0_receive() };
      PA pa_lo = udn0_receive();
      PA pa_hi = udn0_receive();
      PA pa = (pa_hi << 32) | pa_lo;
      uint64_t pte_lo = udn0_receive();
      uint64_t pte_hi = udn0_receive();
      HV_PTE pte = hv_pte((pte_hi << 32) | pte_lo);

      //
      // Dump out the parameters we got.  The ' in a %x format is a Tilera
      // extension which separates groups of 4 digits with an underscore; it
      // is particularly useful in making 64-bit values more readable.
      //
      tprintf("server: request from (%d,%d): map PA %#'llx PTE %#'llx\n",
              from.bits.dest_x, from.bits.dest_y,
              pa, hv_pte_val(pte));

      //
      // Translate the data we got into a translation table entry, then
      // install it into the DTLB so that we can load from and store to the
      // shared page.
      //
      tte_t tte;

      int rv = bme_pte2tte(va, pa, pte, 0, &tte);
      if (rv)
      {
        //
        // We couldn't translate the data into a tte.
        //
        tmc_udn_send_1(from, UDN0_DEMUX_TAG, rv);
      }
      else
      {
        rv = bme_install_dtte(&tte, BME_TTE_INDEX_WIRED);
        if (rv < 0)
        {
          //
          // We couldn't install the tte into the DTLB.
          //
          tmc_udn_send_1(from, UDN0_DEMUX_TAG, 1);
        }
        else
        {
          //
          // Everything was successful.
          //
          tmc_udn_send_1(from, UDN0_DEMUX_TAG, 0);
          buf_mapped = 1;
        }
      }

      break;
    }

    case EX_MSG_PROCESS:
    {
      //
      // This is a request to process a string which is resident within the
      // shared buffer.  Extract the parameters from the message.
      //
      DynamicHeader from = { .word = udn0_receive() };
      intptr_t src_off = udn0_receive();
      int len = udn0_receive();
      intptr_t dst_off = udn0_receive();

      //
      // Dump out the parameters we got.
      //
      tprintf("server: request from (%d,%d): process %d bytes at offset "
              "%#lx to offset %#lx\n",
              from.bits.dest_x, from.bits.dest_y,
              len, src_off, dst_off);

      //
      // If we haven't mapped the buffer, we can't proceed.
      //
      if (!buf_mapped)
      {
        tprintf("server: buffer not mapped\n");
        tmc_udn_send_1(from, UDN0_DEMUX_TAG, -1);
        break;
      }

      //
      // Process the message and send a reply.
      //
      int outlen = process(buf_va + dst_off, buf_va + src_off, len);

      tmc_udn_send_1(from, UDN0_DEMUX_TAG, outlen);
      break;
    }

    default:
      tprintf("server: unknown message type %#x\n", msg);
    }
  }
}
