/*
 * Copyright 2013 Tilera Corporation. All Rights Reserved.
 *
 *   The source code contained or described herein and all documents
 *   related to the source code ("Material") are owned by Tilera
 *   Corporation or its suppliers or licensors.  Title to the Material
 *   remains with Tilera Corporation or its suppliers and licensors. The
 *   software is licensed under the Tilera MDE License.
 *
 *   Unless otherwise agreed by Tilera in writing, you may not remove or
 *   alter this notice or any other notice embedded in Materials by Tilera
 *   or Tilera's suppliers or licensors in any way.
 */

/**
 * @file
 *
 * Structures and enums for defining the various PCIE packet formats.
 */

#ifndef __ARCH_PCIE_PACKETS_H__
#define __ARCH_PCIE_PACKETS_H__

#include <stdint.h>

/** PCIE transaction types. */
enum PCIEHeaderType {
  PCIE_TYPE_REQ_MEM = 0,
  PCIE_TYPE_REQ_MEM_LOCK = 1,
  PCIE_TYPE_REQ_IO = 2,
  PCIE_TYPE_REQ_CONFIG0 = 4,
  PCIE_TYPE_REQ_CONFIG1 = 5,
  PCIE_TYPE_CPL = 0xA,
  PCIE_TYPE_CPL_LOCK = 0xB,
  PCIE_TYPE_MSG_ROUTE_TO_ROOT = 0x10,
  PCIE_TYPE_MSG_ROUTE_BY_ADDRESS = 0x11,
  PCIE_TYPE_MSG_ROUTE_BY_ID = 0X12,
  PCIE_TYPE_MSG_ROUTE_BROADCAST = 0x13,
  PCIE_TYPE_MSG_ROUTE_LOCAL = 0x14,
  PCIE_TYPE_MSG_ROUTE_GATHER = 0x15
};


/** PCIE packet formats. */
enum PCIEHeaderFormat {
  PCIE_FORMAT_3DW = 0,
  PCIE_FORMAT_4DW = 1,
  PCIE_FORMAT_3DW_DATA = 2,
  PCIE_FORMAT_4DW_DATA = 3
};


/** PCIE completion status. */
enum PCIECompletionStatus {
  PCIE_CPL_SUCCESS = 0,
  PCIE_CPL_UNSUPPORTED = 1,
  PCIE_CPL_CONFIG_RETRY = 2,
  PCIE_CPL_ABORT = 4
};

/** PCIE interrupt message codes. */
enum PCIEMessageCode {
  PCIE_MSG_ASSERT_INTA = 0x20,
  PCIE_MSG_ASSERT_INTB = 0x21,
  PCIE_MSG_ASSERT_INTC = 0x22,
  PCIE_MSG_ASSERT_INTD = 0x23,
  PCIE_MSG_DEASSERT_INTA = 0x24,
  PCIE_MSG_DEASSERT_INTB = 0x25,
  PCIE_MSG_DEASSERT_INTC = 0x26,
  PCIE_MSG_DEASSERT_INTD = 0x27
};


/** The first word of PCIE headers (only fields we care about are included). */
typedef struct
{
  uint32_t length:10;        /**< Number of data words. */
  uint32_t pad0:14;          /**< Padding. */
  uint32_t type:5;           /**< The transaction type. */
  uint32_t format:2;         /**< The packet format. */
  uint32_t pad1:1;           /**< Padding. */
}
PCIEHeaderWord;


/** A PCIE configuration header. */
typedef struct
{
  PCIEHeaderWord header;     /**< Generic header word. */

  uint32_t byte_enables:4;   /**< Byte enable bits (high order = high byte). */
  uint32_t pad0:4;           /**< Padding. */
  uint32_t tag:8;            /**< A tag identifying this transaction. */
  uint32_t requester_id:16;  /**< Provided for use in response packet. */

  uint32_t pad1:2;           /**< Padding. */
  uint32_t reg_num:10;       /**< Which register word is being accessed. */
  uint32_t pad2:4;           /**< Padding. */
  uint32_t target_id:16;     /**< Identifies the targetted device. */
}
PCIEConfigHeader;


/** A PCIE completion header. */
typedef struct
{
  PCIEHeaderWord header;     /**< Generic header word. */

  uint32_t byte_count:12;    /**< Remaining bytes until request is satisfied.*/
  uint32_t bc_modified:1;    /**< PCI-X junk. */
  uint32_t status:3;         /**< Completion status. */
  uint32_t completer_id:16;  /**< Identifies the requestee. */

  uint32_t lower_address:7;  /**< Low address bits of returning data. */
  uint32_t pad0:1;           /**< Padding. */
  uint32_t tag:8;            /**< A tag copied from the request. */
  uint32_t requester_id:16;  /**< Destination for this packet. */
}
PCIECplHeader;
  

/** A PCIE Memory request header (same as I/O, actually). */
typedef struct
{
  PCIEHeaderWord header;     /**< Generic header word. */

  uint32_t first_be:4;       /**< Byte enable bits (high order = high byte). */
  uint32_t last_be:4;        /**< Last word byte enables. */
  uint32_t tag:8;            /**< A tag identifying this transaction. */
  uint32_t requester_id:16;  /**< Provided for use in response packet. */
}
PCIEReqHeader;


/** A PCIE Message header. */
typedef struct
{
  PCIEHeaderWord header;     /**< Generic header word. */

  uint32_t message_code:8;   /**< Code specifying the type of message. */
  uint32_t tag:8;            /**< Tag value. */
  uint32_t requester_id:16;  /**< Bus, Device, Function. */
}
PCIEMsgHeader;

/**
 * The MAC prepends a header word to each ingress packet.  One of the
 * fields contains a "packet type" identifier with the following
 * values.  These numbers do not correspond to VirtualChannel numbers;
 * those are defined by ::ChannelNum.
 */
enum PCIEIngressType {
  PCIE_INGRESS_POSTED = 0,
  PCIE_INGRESS_NON_POSTED = 1,
  PCIE_INGRESS_CPL = 2
};

/** The header word prepended to ingress packets by the PCIE MAC. */
typedef union
{
  uint32_t word;             /**< For copying / comparison. */
  struct {
    uint32_t bar_match:3;      /**< Which BAR was matched? */
    uint32_t pad0:3;           /**< Padding. */
    uint32_t trans_type:2;     /**< posted, non-posted, or completion? */
    uint32_t ecrc_error:1;     /**< Packed failed ECRC check. */
  }
  bits;                      /**< For direct bitfield access. */
}
PCIEIngressHeader;

#endif // __ARCH_PCIE_PACKETS_H__
