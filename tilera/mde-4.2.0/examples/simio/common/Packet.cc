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





#include "Packet.h"
#include "helper.h"


#include "pcie/pcie_packets.h"


struct StreamIOHeader
{
  uint16_t type:2;
  uint16_t size:10;
  uint16_t reserved:1;
  uint16_t vc:3;

  union
  {
    uint64_t address;
    struct
    {
      uint64_t addr_lsbs:7;
      uint64_t sts:1;
      uint64_t tag:8;
    };
  };
} __attribute__((__packed__));


#define STREAMIO_SOP 0xFB
#define STREAMIO_EOP 0xFD
#define STREAMIO_TYPE_READ 0
#define STREAMIO_TYPE_WRITE 1
#define STREAMIO_TYPE_CPL 2

#define PCIE_STP 0xFB
#define PCIE_END 0xFD
#define PCIE_MAX_BYTES 4096

trio::Packet::Packet(PacketOpcode opcode, uint64_t address,
                     const void* data, size_t size, int tag, int mac)
  : m_opcode(opcode),
    m_bus_addr(address),
    m_data(size),
    m_tag(tag),
    m_mac_index(mac),
    m_sqn(0)
{
  if (data)
    memcpy(&m_data[0], data, size);
}

trio::Packet::Packet(std::vector<unsigned char>& bytes, int mac, bool is_pcie)
{
  if (is_pcie)
    from_pcie_bytes(bytes, mac);
  else
    from_streamio_bytes(bytes, mac);
}

void
trio::Packet::to_pcap_bytes(std::vector<unsigned char>& bytes, bool is_pcie)
{
  if (is_pcie)
    to_pcie_bytes(bytes);
  else
    to_streamio_bytes(bytes);
}














void
trio::Packet::to_streamio_bytes(std::vector<unsigned char>& bytes)
{
  StreamIOHeader header = { 0, 0, 0, 0, { 0 } };
  bool has_data;
  if (m_opcode == MEM_WRITE)
  {
    header.type = STREAMIO_TYPE_WRITE;
    header.address = m_bus_addr;
    has_data = true;
  }
  else if (m_opcode == MEM_READ)
  {
    header.type = STREAMIO_TYPE_READ;
    header.address = m_bus_addr;
    has_data = false;
  }
  else if (m_opcode == COMPLETION)
  {
    header.type = STREAMIO_TYPE_CPL;
    header.addr_lsbs = m_bus_addr;
    header.sts = 0;
    header.tag = m_tag;
    has_data = true;
  }
  else
  {
    punt("Illegal opcode for streamio MAC.");
  }

  // The header size is the 4-byte alignment plus read/data size.  The
  // total number of data bytes is either 4 bytes for read requests
  // or, if data is present, header.size rounded up to a word
  // boundary.
  sanity(m_data.size() > 0 && m_data.size() <= STREAMIO_MAX_BYTES);
  size_t front_pad = m_bus_addr & 0x3;
  header.size = front_pad + m_data.size(); // wraps 1024 to 0.
  size_t data_size = has_data ? ((front_pad + m_data.size() + 3) & -4) : 4;
  
  // Size = SOP + header + data/read_tag + EOP
  size_t total_bytes = 1 + sizeof(header) + data_size + 1;
  sanity((total_bytes & 0x3) == 0);
  bytes.resize(total_bytes);
  
  // Fill in the data bytes.
  unsigned char* ptr = &bytes[0];
  *ptr++ = STREAMIO_SOP;
  memcpy(ptr, &header, sizeof(header));
  ptr += sizeof(header);

  if (has_data)
  {
    memset(ptr, 0, front_pad);
    ptr += front_pad;
    
    memcpy(ptr, &m_data[0], m_data.size());
    ptr += m_data.size();
    
    size_t tail_pad = data_size - (front_pad + m_data.size());
    memset(ptr, 0, tail_pad);
    ptr += tail_pad;
  }
  else
  {
    // Read "data" is a one byte tag plus 3 pad bytes.
    *ptr++ = m_tag;
    memset(ptr, 0, 3);
    ptr += 3;
  }

  // We don't bother with CRC.  Just put in the EOP and return.
  *ptr++ = STREAMIO_EOP;
  sanity(ptr == &bytes[bytes.size()]);
}

void
trio::Packet::from_streamio_bytes(std::vector<unsigned char>& bytes,
                                  int mac)
{
  m_mac_index = mac;
  unsigned char* ptr = &bytes[0];

  if (*ptr != STREAMIO_SOP)
    punt("Streamio packet without SOP.");
  ptr++;

  // Decode the header.
  bool has_data;
  StreamIOHeader* header = (StreamIOHeader*)ptr;
  ptr += sizeof(*header);
  if (header->type == STREAMIO_TYPE_READ)
  {
    m_opcode = MEM_READ;
    m_bus_addr = header->address;
    m_tag = *ptr;
    has_data = false;
  }
  else if (header->type == STREAMIO_TYPE_WRITE)
  {
    m_opcode = MEM_WRITE;
    m_bus_addr = header->address;
    m_tag = 0;
    has_data = true;
  }
  else if (header->type == STREAMIO_TYPE_CPL)
  {
    m_opcode = COMPLETION;
    m_bus_addr = header->addr_lsbs;
    sanity(header->sts == 0);
    m_tag = header->tag;
    has_data = true;
  }
  else
    punt("Unexpected StreamIO packet type.");

  // Read/write/completion size is always header.size - start_pad.
  size_t data_size = header->size;
  if (data_size == 0)
    data_size = STREAMIO_MAX_BYTES;
  
  size_t start_pad = m_bus_addr & 0x3;
  m_data.resize(data_size - start_pad);
  if (has_data)
  {
    ptr += start_pad;
    memcpy(&m_data[0], ptr, m_data.size());
    ptr += m_data.size();
    
    size_t end_pad = ((data_size + 3) & -4) - data_size;
    ptr += end_pad;
  }
  else
  {
    ptr += 4;
  }

  if (*ptr++ != STREAMIO_EOP)
    punt("StreamIO packet without EOP.");

  sanity((size_t)(ptr - &bytes[0]) == bytes.size());
}

// Taken from the TILE-Pro PCIE driver.
static uint32_t
calc_byte_mask(uint32_t bytes, uint64_t bus_address)
{
  uint32_t zeros = bus_address & 0x3;
  uint32_t shift = bytes + zeros;
  if (shift > 8)
    shift = ((shift - 1) % 4) + 5;
  return ((1 << shift) - 1) & ~((1 << zeros) - 1);
}

typedef union {
  char bytes[16];
  uint32_t words[4];
  PCIEHeaderWord generic;
  PCIECplHeader cpl;
  PCIEReqHeader req;
  PCIEConfigHeader cfg;
} pci_header_t; 

void
trio::Packet::to_pcie_bytes(std::vector<unsigned char>& bytes)
{
  // Start by constructing transaction-layer headers (could be 12 or
  // 16 bytes).  We don't necessarily set 'length' fields correctly
  // here; instead, we'll do that when we fill in the data block.
  //
  // Also, we don't actually set all fields to their correct value.
  // Instead, we fill in only what is required for two simulators to
  // talk to each other.  Bogus fields are marked as 'ignored'.
  bool is_64bit = (m_bus_addr >> 32) != 0;
  bool has_data;
  pci_header_t header;
  memset(&header, 0, sizeof(header));
    
  size_t header_size;
  if (m_opcode == COMPLETION)
  {
    has_data = (m_data.size() > 0);
    
    header.cpl.header.type = PCIE_TYPE_CPL;
    header.cpl.header.format =
      has_data ? PCIE_FORMAT_3DW_DATA : PCIE_FORMAT_3DW;
    header.cpl.byte_count = m_data.size();
    header.cpl.bc_modified = 0;
    header.cpl.status = PCIE_CPL_SUCCESS;
    header.cpl.completer_id = 0; // ignored
    header.cpl.lower_address = m_bus_addr;
    header.cpl.tag = m_tag;
    header.cpl.requester_id = 0; // ignored

    header_size = sizeof(header.cpl);
  }
  else if (m_opcode == MEM_READ || m_opcode == MEM_WRITE ||
           m_opcode == IO_READ || m_opcode == IO_WRITE)
  {
    if (m_opcode == MEM_READ || m_opcode == MEM_WRITE)
      header.req.header.type = PCIE_TYPE_REQ_MEM;
    else
      header.req.header.type = PCIE_TYPE_REQ_IO;
    
    if (m_opcode == MEM_READ || m_opcode == IO_READ)
    {
      has_data = false;
      if (is_64bit)
        header.req.header.format = PCIE_FORMAT_4DW;
      else
        header.req.header.format = PCIE_FORMAT_3DW;
    }
    else
    {
      has_data = true;
      if (is_64bit)
        header.req.header.format = PCIE_FORMAT_4DW_DATA;
      else
        header.req.header.format = PCIE_FORMAT_3DW_DATA;
    }

    uint32_t byte_mask = calc_byte_mask(m_data.size(), m_bus_addr);
    header.req.first_be = byte_mask;
    header.req.last_be = byte_mask >> 4;
    header.req.tag = m_tag;
    header.req.requester_id = 0; // ignored

    // IO and mem requests include extra address words.
    if (is_64bit)
    {
      header.words[2] = (uint32_t) (m_bus_addr >> 32);
      header.words[3] = (uint32_t) (m_bus_addr & ~0x3);
      header_size = sizeof(header.req) + sizeof(uint64_t);
    }
    else
    {
      header.words[2] = (uint32_t) (m_bus_addr & ~0x3);
      header_size = sizeof(header.req) + sizeof(uint32_t);
    }
  }
  else if (m_opcode == CFG_READ || m_opcode == CFG_WRITE)
  {
    if (m_config_space == 0)
      header.cfg.header.type = PCIE_TYPE_REQ_CONFIG0;
    else
      header.cfg.header.type = PCIE_TYPE_REQ_CONFIG1;
    
    if (m_opcode == CFG_READ)
    {
      has_data = false;
      header.cfg.header.format = PCIE_FORMAT_3DW;
    }
    else
    {
      has_data = true;
      header.cfg.header.format = PCIE_FORMAT_3DW_DATA;
    }

    header.cfg.byte_enables = calc_byte_mask(m_data.size(), m_bus_addr);
    header.cfg.tag = m_tag;
    header.cfg.requester_id = 0; // ignored
    header.cfg.reg_num = m_bus_addr >> 2;
    header.cfg.target_id = m_config_id;

    header_size = sizeof(header.cfg);
  }
  else
  {
    punt("Illegal opcode for PCIE MAC.");
  }

  // The length field always encodes the number of data words in the request.
  sanity(m_data.size() <= PCIE_MAX_BYTES);
  size_t front_pad = m_bus_addr & 0x3;
  size_t data_size = (front_pad + m_data.size() + 3) & -4;
  header.generic.length = data_size >> 2;
  
  // Size = SOP + 2-byte sequence number + header + data + crc + EOP
  size_t total_bytes = 1 + 2 + header_size +
    (has_data ? data_size : 0) + 4 + 1;
  sanity((total_bytes & 0x3) == 0);
  bytes.resize(total_bytes);
  
  // Fill in the data bytes.
  unsigned char* ptr = &bytes[0];
  *ptr++ = PCIE_STP;
  *ptr++ = 0; // ignored sequence number
  *ptr++ = 0; // ignored sequence number
  memcpy(ptr, &header, header_size);
  ptr += header_size;

  if (has_data)
  {
    memset(ptr, 0, front_pad);
    ptr += front_pad;
    
    memcpy(ptr, &m_data[0], m_data.size());
    ptr += m_data.size();
    
    size_t tail_pad = data_size - (front_pad + m_data.size());
    memset(ptr, 0, tail_pad);
    ptr += tail_pad;
  }

  // A stub CRC and the EOP.
  *(uint32_t*)ptr = 0;
  ptr += sizeof(uint32_t);
  *ptr++ = PCIE_END;
  sanity(ptr == &bytes[bytes.size()]);
}

void
trio::Packet::from_pcie_bytes(std::vector<unsigned char>& bytes,
                              int mac)
{
  m_mac_index = mac;
  unsigned char* ptr = &bytes[0];

  // Make sure it's at least a 3DW packet (i.e. min size TLP).
  sanity(bytes.size() >= 1 + 2 + 12 + 4 + 1);

  // Skip over phy- and link-layer headers.
  if (*ptr != PCIE_STP)
    punt("PCIe packet without STP.");
  ptr += 1 + 2;
  
  // Decode the header.
  pci_header_t header;
  memcpy(&header, ptr, sizeof(header));
  size_t start_pad = 0;
  bool has_data;
  if (header.generic.type == PCIE_TYPE_CPL)
  {
    // Extract fields.
    m_opcode = COMPLETION;
    m_data.resize(header.cpl.byte_count);
    m_bus_addr = header.cpl.lower_address;
    m_tag = header.cpl.tag;

    // Move pointer to start of data, note any masked bytes.
    ptr += sizeof(header.cpl);
    start_pad = header.cpl.lower_address & 0x3;
    has_data = m_data.size() > 0;
  }
  else if (header.generic.type == PCIE_TYPE_REQ_MEM ||
           header.generic.type == PCIE_TYPE_REQ_IO)
  {
    m_tag = header.req.tag;
    
    if (header.req.header.format == PCIE_FORMAT_4DW ||
        header.req.header.format == PCIE_FORMAT_3DW)
    {
      // Read requests.
      has_data = false;
      if (header.generic.type == PCIE_TYPE_REQ_MEM)
        m_opcode = MEM_READ;
      else
        m_opcode = IO_READ;
    }
    else
    {
      // Write requests.
      has_data = true;
      if (header.generic.type == PCIE_TYPE_REQ_MEM)
        m_opcode = MEM_WRITE;
      else
        m_opcode = IO_WRITE;
    }

    // Decode size and byte enables.  The packet length is measured in
    // words, and byte enables drop data at the beginning or end of
    // those words.
    size_t words = header.generic.length;
    if (words == 0)
      words = 1024;
    
    sanity(header.req.first_be != 0);
    start_pad = __builtin_ctz(header.req.first_be);
    uint32_t last_be;
    if (header.generic.length == 1)
      last_be = header.req.first_be;
    else
      last_be = header.req.last_be;
    size_t skip_end = __builtin_clz(last_be) - 28;

    m_data.resize(words * sizeof(uint32_t) - start_pad - skip_end);

    // Decode the address, add back any pad bytes, and advance ptr.
    if (header.generic.format == PCIE_FORMAT_4DW ||
        header.generic.format == PCIE_FORMAT_4DW_DATA)
    {
      m_bus_addr = ((uint64_t)header.words[2] << 32) | header.words[3];
      ptr += sizeof(uint32_t) * 4;
    }
    else
    {
      m_bus_addr = header.words[2];
      ptr += sizeof(uint32_t) * 3;
    }
    m_bus_addr += start_pad;
  }
  else if (header.generic.type == PCIE_TYPE_REQ_CONFIG0 ||
           header.generic.type == PCIE_TYPE_REQ_CONFIG1)
  {
    m_tag = header.cfg.tag;

    if (header.generic.type == PCIE_TYPE_REQ_CONFIG0)
      m_config_space = 0;
    else
      m_config_space = 1;
    
    if (header.generic.format == PCIE_FORMAT_3DW)
    {
      m_opcode = CFG_READ;
      has_data = false;
    }
    else
    {
      m_opcode = CFG_WRITE;
      has_data = true;
    }

    // Decode size and byte enables.  All config accesses are a single
    // word, plus byte enables.
    sanity(header.generic.length == 1);    
    sanity(header.cfg.byte_enables != 0);
    start_pad = __builtin_ctz(header.cfg.byte_enables);
    size_t skip_end = __builtin_clz((uint32_t)header.cfg.byte_enables) - 28;

    m_data.resize(sizeof(uint32_t) - start_pad - skip_end);

    // Extract the register address and bus/dev/fn.
    m_bus_addr = (header.cfg.reg_num << 2) + start_pad;
    m_config_id = header.cfg.target_id;

    // Advance the data pointer.
    ptr += sizeof(header.cfg);
  }
  else
  {
    punt("Unknown PCIe packet type.");
  }

  // Copy out the data, if any.
  if (has_data)
    memcpy(&m_data[0], ptr + start_pad, m_data.size());
}


std::ostream&
operator << (std::ostream& outs, const trio::Packet& packet)
{
  static const char* opcode_names[] = {
    "completion",
    "mem_write",
    "mem_read",
    "io_write",
    "io_read",
    "cfg_write",
    "cfg_read"
  };
  
  outs << "(opcode=" << opcode_names[packet.m_opcode] <<
    ", address=0x" << std::hex << packet.m_bus_addr << std::dec <<
    ", size=" << packet.m_data.size() <<
    ", tag=" << packet.m_tag <<
    ", mac=" << packet.m_mac_index;

  if (packet.m_opcode == trio::Packet::CFG_READ ||
      packet.m_opcode == trio::Packet::CFG_WRITE)
  {
    int bus, dev, fn;
    trio::Packet::unpack_pcie_id(packet.m_config_id, bus, dev, fn);
    outs << ", bus = " << bus <<
      ", dev = " << dev <<
      ", fn = " << fn;
  }

  outs << ")";

  return outs;
}






