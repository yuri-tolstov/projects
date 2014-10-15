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
// USB device tile-side example: processing loop.
//
// WARNING: before using this code as the basis for any production
// application, take special note of the caveats in this example's
// README.txt file.
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arch/cycle.h>

#include <gxio/common.h>
#include <gxio/usb_dev.h>

#include <linux/usb/ch9.h>

#include <tmc/ipi.h>

#include "ud.h"
#include "ud_intf.h"
#include "util.h"


#define PKT_SIZE 512

/** Endpoint state. */
static struct ep_state
{
  int fd;               /**< Currently open file descriptor, or -1 if none. */
  int fn;               /**< Last requested file index. */
  uint8_t is_in;        /**< Nonzero if this is an IN endpoint. */
  uint8_t ep;           /**< This endpoint's number. */
  uint8_t flushing_tx;  /**< Nonzero if we're flushing the TX FIFO. */
}
ep_states[7] = { { 0 } };

/** String descriptor 0. */
uint8_t str_desc0[] = { 4, USB_DT_STRING, 0x09, 0x04 };

/** Table of string descriptors. */
struct
{
  uint8_t* data;
  int len;
}
string_table[10] = { { .data = str_desc0, .len = sizeof (str_desc0) } };

/** Device descriptor. */
static struct usb_device_descriptor dev_desc =
{
  .bLength = sizeof (struct usb_device_descriptor),
  .bDescriptorType = USB_DT_DEVICE,
  .bcdUSB = 0x200,
  .bDeviceClass = USB_CLASS_VENDOR_SPEC,
  .bDeviceSubClass = USB_SUBCLASS_VENDOR_SPEC,
  .bDeviceProtocol = 0xFF,
  .bMaxPacketSize0 = 64,
  .idVendor = UD_DEV_VENDOR,
  .idProduct = UD_DEV_PRODUCT,
  .bcdDevice = 0,
  .iManufacturer = 1,
  .iProduct = 2,
  .iSerialNumber = 3,
  .bNumConfigurations = 1,
};

/** Endpoint descriptor.  This definition is copied/changed from usb/ch9.h
 *  since the one in that file has extra garbage at the end; not good if
 *  you're trying to cat them together. */
struct usb_std_endpoint_descriptor {
        __u8  bLength;
        __u8  bDescriptorType;

        __u8  bEndpointAddress;
        __u8  bmAttributes;
        __le16 wMaxPacketSize;
        __u8  bInterval;
} __attribute__ ((packed));

/** Configuration descriptor, and all of the following interface/endpoint
 *  descriptors. */
static struct
{
  struct usb_config_descriptor config;
  struct usb_interface_descriptor intf0;
  struct usb_std_endpoint_descriptor ep1;
  struct usb_interface_descriptor intf1;
  struct usb_std_endpoint_descriptor ep2;
  struct usb_interface_descriptor intf2;
  struct usb_std_endpoint_descriptor ep3;
  struct usb_interface_descriptor intf3;
  struct usb_std_endpoint_descriptor ep4;
}
config_desc =
{
  .config =
  {
    .bLength = sizeof (struct usb_config_descriptor),
    .bDescriptorType = USB_DT_CONFIG,
    .bNumInterfaces = 4,
    .bConfigurationValue = 1,
    .iConfiguration = 4,
    .bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
    .bMaxPower = 0,
  },
  .intf0 =
  {
    .bLength = sizeof (struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
    .iInterface = 5,
  },
  .ep1 =
  {
    .bLength = sizeof (struct usb_std_endpoint_descriptor),
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 1 | USB_ENDPOINT_DIR_MASK,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = PKT_SIZE,
    .bInterval = 1,
  },
  .intf1 =
  {
    .bLength = sizeof (struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
    .iInterface = 6,
  },
  .ep2 =
  {
    .bLength = sizeof (struct usb_std_endpoint_descriptor),
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 2,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = PKT_SIZE,
    .bInterval = 1,
  },
  .intf2 =
  {
    .bLength = sizeof (struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 2,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
    .iInterface = 7,
  },
  .ep3 =
  {
    .bLength = sizeof (struct usb_std_endpoint_descriptor),
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 3 | USB_ENDPOINT_DIR_MASK,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = PKT_SIZE,
    .bInterval = 1,
  },
  .intf3 =
  {
    .bLength = sizeof (struct usb_interface_descriptor),
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 3,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
    .iInterface = 8,
  },
  .ep4 =
  {
    .bLength = sizeof (struct usb_std_endpoint_descriptor),
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 4,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = PKT_SIZE,
    .bInterval = 1,
  },
};

//
// This is used when we need a non-stack-allocated source of null data.
//
uint8_t dummy_buffer[PKT_SIZE];


/** Start output.
 * @param uc USB device context.
 * @param eps Endpoint state pointer.
 */
void
output(gxio_usb_dev_context_t* uc, struct ep_state* eps)
{
  uint8_t in_buf[PKT_SIZE];

  if (eps->fd < 0)
  {
    //
    // If we don't have an open file, then we just return zeroes.
    //
    gxio_usb_dev_write_tx_fifo(uc, eps->ep, in_buf, PKT_SIZE);
  }
  else
  {
    //
    // Otherwise, try to get a packet's worth of data from the file, and
    // send it.  Note that at EOF, we'll write a 0-length packet, which
    // will tell the reader that there's no more data.
    //
    int len = read(eps->fd, in_buf, PKT_SIZE);

    if (len >= 0)
    {
      gxio_usb_dev_write_tx_fifo(uc, eps->ep, in_buf, len);
    }
    else
    {
      fprintf(stderr, "%s: file read error: %s, ep %d fn %d @%lld\n",
              myname, strerror(errno), eps->ep, eps->fn,
              (long long)lseek(eps->fd, 0, SEEK_CUR));
      gxio_usb_dev_write_tx_fifo(uc, eps->ep, in_buf, 0);
    }
  }
}


/** Handle an interrupt on an IN endpoint.
 * @param uc USB device context.
 * @param ep Endpoint.
 */
void
handle_in_ep(gxio_usb_dev_context_t* uc, int ep)
{
  struct ep_state* eps = &ep_states[ep];

  gxio_usb_dev_set_ep_intr(uc, 1 << ep);
  int in_stat = gxio_usb_dev_get_in_sts(uc, ep);
  TRACE("  ep %d, intr in, in stat: %8x\n", ep, in_stat);

  if (in_stat & 0x1000000)
  {
    //
    // Transmit complete.  If this is a regular endpoint, we do more output
    // if possible.
    //
    gxio_usb_dev_set_in_sts(uc, ep, 0x1000000);
    TRACE("    TX completed\n");

    if (ep != 0)
      output(uc, eps);
  }

  if (in_stat & 0x40)
  {
    //
    // IN recevied.  We don't do anything here, unless we were flushing
    // the TX FIFO; in that case we clear the flush bit, and start output.
    //
    TRACE("    IN pending\n");

    if (eps->flushing_tx)
    {
      eps->flushing_tx = 0;
      int in_ctrl = gxio_usb_dev_get_in_ctrl(uc, ep);
      gxio_usb_dev_set_in_ctrl(uc, ep, in_ctrl & ~2);
      output(uc, eps);
    }

    gxio_usb_dev_set_in_sts(uc, ep, 0x40);
  }
}


/** Handle an interrupt on an OUT endpoint.
 * @param uc USB device context.
 * @param ep Endpoint.
 */
void
handle_out_ep(gxio_usb_dev_context_t* uc, int ep)
{
  struct ep_state* eps = &ep_states[ep];

  gxio_usb_dev_set_ep_intr(uc, 0x10000 << ep);
  int stat = gxio_usb_dev_get_out_sts(uc, ep);
  TRACE("  ep %d, intr out, out stat: %8x\n", ep, stat);

  //
  // Switch on packet type.
  //
  int out_pkt = (stat >> 4) & 3;
  if (out_pkt == 1)
  {
    //
    // OUT packet.
    //
    int pkt_sz = (stat >> 11) & 0xFFF;
    TRACE("    received data, %d bytes\n", pkt_sz);

    uint8_t out_buf[PKT_SIZE];

    if (eps->fd < 0)
    {
      //
      // We don't have an open file, so throw away the data.
      //
      gxio_usb_dev_read_rx_fifo(uc, ep, out_buf, pkt_sz);
    }
    else
    {
      //
      // Write to our output file.
      //
      gxio_usb_dev_read_rx_fifo(uc, ep, out_buf, pkt_sz);
      int len = write(eps->fd, out_buf, pkt_sz);
      if (len != pkt_sz)
      {
        if (len < 0)
          fprintf(stderr, "%s: file write error: %s", myname, strerror(errno));
        else
          fprintf(stderr, "%s: short file write, %d != %d\n", myname, len,
                  pkt_sz);
      }
    }
  }
  else if (out_pkt == 2)
  {
    //
    // SETUP packet.
    //
    TRACE("    received setup, config %d intf %d setting %d\n",
          (stat >> 19) & 0xF, (stat >> 15) & 0xF, (stat >> 11) & 0xF);

    struct usb_ctrlrequest setup;

    gxio_usb_dev_read_rx_fifo(uc, ep, &setup, sizeof(setup));

    TRACE("    bRequestType %d bRequest %d wValue 0x%x "
           "wIndex 0x%x wLength %d\n",
           setup.bRequestType, setup.bRequest,
           setup.wValue, setup.wIndex, setup.wLength);

    switch (setup.bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
    {
      int desctype = setup.wValue >> 8;
#ifndef NDEBUG
      int descindex = setup.wValue & 0xFF;

      static const char* desc_types[] =
      {
        "",
        "DEVICE",
        "CONFIGURATION",
        "STRING",
        "INTERFACE",
        "ENDPOINT",
        "DEVICE_QUALIFIER",
        "OTHER_SPEED_CONFIGURATION",
        "INTERFACE_POWER",
      };
#endif

      TRACE("    GET_DESCRIPTOR, type %d (%s), index %d\n",
            desctype, desc_types[desctype], descindex);

      switch (desctype)
      {
      case USB_DT_DEVICE:
        gxio_usb_dev_write_tx_fifo(uc, eps->ep, &dev_desc,
                                   min(sizeof (dev_desc), setup.wLength));
        break;

      case USB_DT_CONFIG:
        gxio_usb_dev_write_tx_fifo(uc, eps->ep, &config_desc,
                                   min(sizeof (config_desc), setup.wLength));
        break;

      case USB_DT_STRING:
      {
        int index = setup.wValue & 0xFF;
        if (index >= sizeof (string_table) / sizeof (*string_table))
        {
          fprintf(stderr, "%s: illegal string descriptor index %d, "
                  "returning 0-length\n", myname, index);
          gxio_usb_dev_write_tx_fifo(uc, eps->ep, dummy_buffer, 0);
        }
        else
        {
          gxio_usb_dev_write_tx_fifo(uc, eps->ep, string_table[index].data,
                                     min(string_table[index].len,
                                         setup.wLength));
        }
        break;
      }

      default:
        fprintf(stderr, "%s: unrecognized descriptor type %d, "
                "returning 0-length\n", myname, desctype);
        gxio_usb_dev_write_tx_fifo(uc, eps->ep, dummy_buffer, 0);
      }
      break;
    }
    case UD_SET_FILE:
    {
      //
      // This is our vendor-specific request that opens a local file
      // associated with a particular endpoint.
      //
      int tgt_ep = setup.wIndex & 0x7F;
      int fn = setup.wValue;

      TRACE("    Open file %d for ep %d\n", fn, tgt_ep);

      if (tgt_ep >= 1 && tgt_ep <= 4)
      {
        struct ep_state* tgt_eps = &ep_states[tgt_ep];
        tgt_eps->fn = fn;

        char fname[80];
        sprintf(fname, "/tmp/usb_tmp_%d", tgt_eps->fn);

        if (tgt_eps->fd >= 0)
          close(tgt_eps->fd);

        if (tgt_eps->is_in)
        {
          tgt_eps->fd = open(fname, O_RDONLY, 0);

          //
          // We need to flush the TX FIFO in case a previous transfer was
          // aborted.
          //
          int in_ctrl = gxio_usb_dev_get_in_ctrl(uc, tgt_ep);
          gxio_usb_dev_set_in_ctrl(uc, tgt_ep, in_ctrl | 2);

          tgt_eps->flushing_tx = 1;
        }
        else
        {
          tgt_eps->fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        }

      }

      if (setup.wLength > 0 && (setup.bRequestType & 0x80))
        gxio_usb_dev_write_tx_fifo(uc, eps->ep, dummy_buffer, setup.wLength);
    }
    }

    //
    // SETUP requests set a NAK bit on the endpoint, which we need to
    // clear.
    //
    int out_ctrl = gxio_usb_dev_get_out_ctrl(uc, ep);
    if (out_ctrl & 0x40)
    {
      gxio_usb_dev_set_out_ctrl(uc, ep, out_ctrl | 0x100);
      TRACE("    ep %d: OUT cleared out nak\n", ep);
    }

    int in_ctrl = gxio_usb_dev_get_in_ctrl(uc, ep);
    if (in_ctrl & 0x40)
    {
      gxio_usb_dev_set_in_ctrl(uc, ep, in_ctrl | 0x100);
      TRACE("    ep %d: OUT cleared in nak\n", ep);
    }
  }
}


/** Handle device interrupts.
 * @param uc USB device context.
 * @param dev_intr Contents of device interrupt register.
 */
static void
handle_dev_intrs(gxio_usb_dev_context_t* uc, int dev_intr)
{
  if (dev_intr)
  {
    gxio_usb_dev_set_d_intr(uc, dev_intr);

#ifndef NDEBUG
    int dev_stat = gxio_usb_dev_get_d_sts(uc);
#endif
    TRACE("dev intrs: %2x dev stat %8x\n", dev_intr, dev_stat);

    //
    // For SET CONFIGURATION or SET INTERFACE commands, we need to set
    // CSR_DONE in the device control register.
    //
    if (dev_intr & 0x1)
    {
      TRACE("dev, set configuration %d\n", dev_stat & 0xF);

      int d_ctrl = gxio_usb_dev_get_d_ctrl(uc);
      gxio_usb_dev_set_d_ctrl(uc, d_ctrl | (1 << 13));
    }

    if (dev_intr & 0x2)
    {
      TRACE("dev, set interface %d\n", (dev_stat >> 4) & 0xF);

      int d_ctrl = gxio_usb_dev_get_d_ctrl(uc);
      gxio_usb_dev_set_d_ctrl(uc, d_ctrl | (1 << 13));
    }
  }
}


/** Handle endpoint interrupts.
 * @param uc USB device context.
 * @param ep_intr Contents of endpoint interrupt register.
 */
static void
handle_ep_intrs(gxio_usb_dev_context_t* uc, int ep_intr)
{
  //
  // Note that the interrupt bits are reset when the individual endpoints
  // are processed.
  //
  while (ep_intr)
  {
    //
    // We process endpoints in reverse order so that if a control on ep 0
    // and an OUT on ep N show up at the same time, we do the OUT first.
    //
    int ep_bit = (8 * sizeof (int)) - 1 - __builtin_clz(ep_intr);
    ep_intr -= 1 << ep_bit;

    if (ep_bit < 16)
      handle_in_ep(uc, ep_bit);
    else
      handle_out_ep(uc, ep_bit - 16);
  }
}


/** Process an interrupt.
 * @param arg USB device context.
 */
static void
proc_intr(void *arg)
{
  gxio_usb_dev_context_t* uc = arg;

  //
  // First, we check for and handle interrupts until there are no more
  // pending.
  //
  int dev_intr;
  int ep_intr;

  while ((dev_intr = gxio_usb_dev_get_d_intr(uc)) != 0)
    handle_dev_intrs(uc, dev_intr);

  while ((ep_intr = gxio_usb_dev_get_ep_intr(uc)) != 0)
    handle_ep_intrs(uc, ep_intr);

  //
  // Then, we can enable further interrupts.
  //
  gxio_usb_dev_reset_interrupt(uc);

  //
  // Finally, we have to make one more pass through, just in case events
  // happened in between the last time we checked, and when we reenabled
  // interrupts.  We actually only need to do this once, but the while
  // loops don't cost anything extra, so why not process all we can?
  //
  while ((dev_intr = gxio_usb_dev_get_d_intr(uc)) != 0)
    handle_dev_intrs(uc, dev_intr);

  while ((ep_intr = gxio_usb_dev_get_ep_intr(uc)) != 0)
    handle_ep_intrs(uc, ep_intr);
}


/** Translate an ASCII string to a UTF-16-encoded string descriptor.
 * @param str String to translate.
 * @param data On return, the value this points to will be filled in with
 *  the dynamically-allocated descriptor.
 * @param len On return, the value this points to will be filled in with
 *  the length of the descriptor.
 */
void
encode_string_desc(char* str, uint8_t** data, int* len)
{
  *len = 2 + 2 * strlen(str);
  uint8_t* rv = *data = malloc(*len);

  *rv++ = *len;
  *rv++ = USB_DT_STRING;

  while (*str)
  {
    *rv++ = *str++;
    *rv++ = '\0';
  }
}


/** USB processing loop.
 * @param cpu CPU number we're running on.
 */
void
proc(int cpu)
{
  //
  // Initialize our endpoint state table.
  //
  for (int i = 0; i < sizeof (ep_states) / sizeof(*ep_states); i++)
  {
    ep_states[i].fd = -1;
    ep_states[i].ep = i;
  }

  ep_states[1].is_in = 1;
  ep_states[3].is_in = 1;

  //
  // Complete the length computation for the config descriptor, and
  // encode our string descriptors.
  //
  config_desc.config.wTotalLength = sizeof (config_desc);

  encode_string_desc("Tilera Corporation",
                     &string_table[1].data, &string_table[1].len);
  encode_string_desc("TILE-Gx Device Mode Example",
                     &string_table[2].data, &string_table[2].len);
  encode_string_desc("0",
                     &string_table[3].data, &string_table[3].len);
  encode_string_desc("The One and Only",
                     &string_table[4].data, &string_table[4].len);
  encode_string_desc("File download",
                     &string_table[5].data, &string_table[5].len);
  encode_string_desc("File upload",
                     &string_table[6].data, &string_table[6].len);
  encode_string_desc("File download",
                     &string_table[7].data, &string_table[7].len);
  encode_string_desc("File upload",
                     &string_table[8].data, &string_table[8].len);

  //
  // Open a USB context.
  //
  gxio_usb_dev_context_t uc;

  int rv = gxio_usb_dev_init(&uc, 0);
  if (rv != 0)
    bomb("gxio_usb_dev_init failed: %s", gxio_strerror(rv));

  //
  // Do register setup.  We have to do this as soon as possible after
  // gxio_usb_dev_init() returns; if we take more than 6 ms, it's possible
  // that the PHY will have gone into suspend mode and the accesses to the
  // registers which are in the PHY clock domain will hang.
  //

  // Logical endpoint 0 (control, so in/out)

  // Control endpoint
  gxio_usb_dev_set_in_ctrl(&uc, 0, 0);
  // 1024 bytes in TX FIFO
  gxio_usb_dev_set_in_bufsize(&uc, 0, 1024 / 4);
  // 64 byte packet size
  gxio_usb_dev_set_in_mpkt_sz(&uc, 0, 64);
  gxio_usb_dev_set_out_ctrl(&uc, 0, 0);
  // 1024 bytes in RX FIFO, 64-byte packets
  gxio_usb_dev_set_out_bufsize(&uc, 0, 0x1000040);

  // Logical endpoint 1 (in)

  // Bulk endpoint
  gxio_usb_dev_set_in_ctrl(&uc, 1, 0x20);
  // 1024 bytes in TX FIFO
  gxio_usb_dev_set_in_bufsize(&uc, 1, 1024 / 4);
  // 512 byte packet size
  gxio_usb_dev_set_in_mpkt_sz(&uc, 1, 512);

  // Logical endpoint 2 (out)

  // Bulk endpoint
  gxio_usb_dev_set_out_ctrl(&uc, 2, 0x20);
  // 1024 bytes in RX FIFO, 512-byte packets
  gxio_usb_dev_set_out_bufsize(&uc, 2, 0x1000200);

  // Logical endpoint 3 (in)

  // Bulk endpoint
  gxio_usb_dev_set_in_ctrl(&uc, 3, 0x20);
  // 1024 bytes in TX FIFO
  gxio_usb_dev_set_in_bufsize(&uc, 3, 1024 / 4);
  // 512 byte packet size
  gxio_usb_dev_set_in_mpkt_sz(&uc, 3, 512);

  // Logical endpoint 4 (out)

  // Bulk endpoint
  gxio_usb_dev_set_out_ctrl(&uc, 4, 0x20);
  // 1024 bytes in RX FIFO, 512-byte packets
  gxio_usb_dev_set_out_bufsize(&uc, 4, 0x1000200);

  // Dev config: self-powered, 8-bit UTMI, dynamic CSR programming
  gxio_usb_dev_set_d_cfg(&uc, 0x20028);
  gxio_usb_dev_set_d_ctrl(&uc, 0);

  // ep 0, out, control, config 0; 64-byte packets
  gxio_usb_dev_set_udc_ep(&uc, 0, 0x02000000);
  // ep 1, in, bulk, config 1, intf 0; 512-byte packets
  gxio_usb_dev_set_udc_ep(&uc, 1, 0x100000d1);
  // ep 2, out, bulk, config 1, intf 1; 512-byte packets
  gxio_usb_dev_set_udc_ep(&uc, 2, 0x100008c2);
  // ep 3, in, bulk, config 1, intf 2; 512-byte packets
  gxio_usb_dev_set_udc_ep(&uc, 3, 0x100010d3);
  // ep 4, out, bulk, config 1, intf 3, 512-byte packets
  gxio_usb_dev_set_udc_ep(&uc, 4, 0x100018c4);

  //
  // Now allocate an event number for the device interrupt, install its
  // handler, and enable it.
  //
  int ipi_level = 0;
  int64_t ipi_event = tmc_ipi_event_alloc_first_available(ipi_level);
  if (ipi_event < 0)
    pbomb("ipi_alloc failed");

  rv = tmc_ipi_event_install(ipi_level, ipi_event, proc_intr, &uc);
  if (rv != 0)
    pbomb("event_install failed");

  gxio_usb_dev_cfg_interrupt(&uc, cpu, ipi_event);

  //
  // Unmask all of the interrupts, except for start-of-frame; since we
  // aren't doing any isochronous stuff we don't need to pay attention to
  // frames.
  //
  gxio_usb_dev_set_d_intr_msk(&uc, 0x20);
  gxio_usb_dev_set_ep_intr_msk(&uc, 0);
  gxio_usb_dev_reset_interrupt(&uc);

  //
  // Since this is an example, we don't have any other work to do, so we
  // just nap while waiting for interrupts to happen.
  //
  while (1)
    __insn_nap();

  //
  // Clean up.  Note that currently, this code never runs; we'd have to
  // provide some way to break out of the nap loop above.
  //
  rv = gxio_usb_dev_destroy(&uc);
  if (rv != 0)
    bomb("gxio_usb_dev_destroy failed: %s", gxio_strerror(rv));
}
