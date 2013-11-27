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


/**
 * @file
 *
 * Generic TRIO packet.
 */

#ifndef TOOLS_SIMULATOR_GSIM_TRIO_PACKET_H
#define TOOLS_SIMULATOR_GSIM_TRIO_PACKET_H

#include <stdint.h>
#include <vector>
#include <iostream>





/** Maximum number of bytes in a streamio packet. */
#define STREAMIO_MAX_BYTES 1024

namespace trio {

/** An abstract representation of a TRIO packet.  Contains all
    possible fields of a StreamIO or PCIE packet in a more convenient
    form.  Can be generate or be constructed from StreamIO or PCIE
    packets. */
class Packet
{
public:
  /** Operations that packets can encode. */
  typedef enum {
    COMPLETION,
    MEM_WRITE,
    MEM_READ,
    IO_WRITE,
    IO_READ,
    CFG_WRITE,
    CFG_READ,
  } PacketOpcode;

  /** Construct with packet parameters. */
  Packet(PacketOpcode opcode, uint64_t address,
         const void* data, size_t size, int tag, int mac);
  
  /** Construct from a raw packet. */
  Packet(std::vector<unsigned char>& packet, int mac, bool is_pcie);






  /** Construct a raw packet from this generic packet object. */
  void to_pcap_bytes(std::vector<unsigned char>& bytes, bool is_pcie);










  
  /** What type of operation does this packet represent? */
  PacketOpcode m_opcode;

  /** Target address (or just low bits for completions). */
  uint64_t m_bus_addr;

  /** Packet data buffer, also encodes read or write size. */
  std::vector<unsigned char> m_data;

  /** Tag value. */
  int m_tag;

  /** Which MAC index is this packet going to (or coming from). */
  int m_mac_index;

  /** A write sequence number. */
  uint64_t m_sqn;

  /** PCIe config space 0 vs. 1. */
  uint8_t m_config_space;

  /** PCIe bus / dev / fn. */
  uint16_t m_config_id;

  /** Pack bus / dev / fn into 16 bits. */
  static uint16_t pack_pcie_id(int bus, int dev, int fn)
  {
    return ((bus & 0xff) << 8) | ((dev & 0x1f) << 3) | (fn & 0x7);
  }

  /** Unpack bus / dev / fn. */
  static void unpack_pcie_id(uint16_t id, int& bus, int& dev, int& fn)
  {
    bus = (id >> 8) & 0xff;
    dev = (id >> 3) & 0x1f;
    fn = id & 0x7;
  }





  
private:
  /** Convert a generic packet to streamio bytes. */
  void to_streamio_bytes(std::vector<unsigned char>& bytes);
  
  /** Convert packet bytes into generic packet fields. */
  void from_streamio_bytes(std::vector<unsigned char>& bytes, int mac);
  
  /** Convert a generic packet to pcie bytes. */
  void to_pcie_bytes(std::vector<unsigned char>& bytes);
  
  /** Convert packet bytes into generic packet fields. */
  void from_pcie_bytes(std::vector<unsigned char>& bytes, int mac);




};






}; // namespace trio

/** For pretty printing Packets. */
std::ostream& operator << (std::ostream& outs, const trio::Packet& packet);

#endif // TOOLS_SIMULATOR_GSIM_TRIO_PACKET_H
