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

#include "common.h"
#include "helper.h"
#include "Packet.h"
#include "PacketStream.h"

#include <iostream>
#include <vector>

/* Convenience wrappers for handling incoming StreamIO packets and
   generating egress packets. */
class StreamIO
{
public:
  StreamIO(PacketStream* socket)
    :m_socket(socket),
     m_next_tag(0),
     m_read_pending(false),
     m_read_dest(NULL),
     m_read_size(0)
  {
  }

  virtual ~StreamIO()
  {}

  /* Wait for at least one ingress packet to arrive. */
  void wait_for_ingress()
  {
    TCPDumpPacketHeader header;
    std::vector<unsigned char> data;

    // FIXME: poll
    while(!m_socket->get_next_packet(&header, data))
      sleep(1);

    handle_ingress_packet(data);
  }

  /* Override this function to do something when a read arrives. */
  virtual void handle_read(uint64_t bus_address,
                           std::vector<unsigned char> &data) = 0;

  /* Override this function to do something when a write arrives. */
  virtual void handle_write(uint64_t bus_address,
                            std::vector<unsigned char> &data) = 0;

  /* Generate a StreamIO write packet. */
  void do_write(uint64_t bus_address, const void *data, size_t size)
  {
    trio::Packet packet(trio::Packet::MEM_WRITE, bus_address, data, size,
                        0, 0);
    write_packet(packet);
  }

  /** Perform a blocking StreamIO read. */
  void do_read(void *dest, uint64_t bus_address, size_t size)
  {
    // Record information needed by completion handler.
    m_read_pending = true;
    m_read_dest = dest;
    m_read_size = size;

    // Send the request packet.
    trio::Packet request(trio::Packet::MEM_READ, bus_address, NULL, size,
                         m_next_tag++, 0);
    write_packet(request);

    // Run the ingress handler until the read completes.
    while (m_read_pending)
      wait_for_ingress();
  }

private:

  /* Parse an ingress packet and invoke the appropriate handler. */
  void handle_ingress_packet(std::vector<unsigned char> &data)
  {
    trio::Packet packet(data, 0, false);

    if (packet.m_opcode == trio::Packet::MEM_READ)
    {
      // Turn the packet into a response and have the client fill in
      // the data.
      packet.m_opcode = trio::Packet::COMPLETION;
      handle_read(packet.m_bus_addr, packet.m_data);

      // Send the response.
      write_packet(packet);
    }
    else if (packet.m_opcode == trio::Packet::MEM_WRITE)
    {
      handle_write(packet.m_bus_addr, packet.m_data);
    }
    else if (packet.m_opcode == trio::Packet::COMPLETION)
    {
      sanity(m_read_pending);
      sanity(m_read_size == packet.m_data.size());
      
      memcpy(m_read_dest, &packet.m_data[0], m_read_size);
      m_read_pending = false;
    }
    else
      punt("Unexpected ingress packet type");
  }

  /* Turn an abstract packet into bytes and send it. */
  void write_packet(trio::Packet &packet)
  {
    std::vector<unsigned char> egress_bytes;
    packet.to_pcap_bytes(egress_bytes, false);
    unsigned int size = egress_bytes.size();
    TCPDumpPacketHeader header = { 0, 0, size, size };
    m_socket->write_packet(&header, egress_bytes);
  }

  PacketStream* m_socket;
  unsigned char m_next_tag;
  bool m_read_pending;
  void* m_read_dest;
  size_t m_read_size;
};


/* A StreamIO object that handles reads and writes by targeting them
   at a memory buffer. */
class MemoryBackedStreamIO: public StreamIO
{
public:
  MemoryBackedStreamIO(PacketStream* socket, uint64_t window_start,
                       uint64_t window_size)
    : StreamIO(socket),
      m_start(window_start),
      m_size(window_size)
  {
    m_data = new unsigned char[m_size];
    memset(m_data, 0, m_size);
  }

  virtual void handle_read(uint64_t bus_address,
                           std::vector<unsigned char> &data)
  {
    unsigned size = data.size();
    if (bus_address < m_start ||
        bus_address + size < m_start ||
        bus_address + size > m_start + m_size)
    {
      punt("Bad read target address %lld, size %lld",
           (unsigned long long)bus_address,
           (unsigned long long)size);
    }
    memcpy(&data[0], m_data + (bus_address - m_start), size);
  }
  
  virtual void handle_write(uint64_t bus_address,
                            std::vector<unsigned char> & data)
  {
    uint64_t size = data.size();
    if (bus_address < m_start ||
        bus_address + size < m_start ||
        bus_address + size > m_start + m_size)
    {
      punt("Bad write target address %lld, size %lld",
           (unsigned long long)bus_address,
           (unsigned long long)size);
    }
    memcpy(m_data + (bus_address - m_start), &data[0], size);
  }

private:
  uint64_t m_start;
  uint64_t m_size;
  unsigned char* m_data;
};


void write_read(StreamIO& streamio, unsigned offset, unsigned size)
{
  static unsigned char seed = 0;
  unsigned char data[size];

  for (unsigned i = 0; i < size; i++)
  {
    data[i] = (unsigned char)(seed + i);
  }

  streamio.do_write(TILE_WINDOW_START + offset, data, size);
  memset(data, 0, size);
  streamio.do_read(data, TILE_WINDOW_START + offset, size);
  
  for (unsigned i = 0; i < size; i++)
  {
    if (data[i] != ((seed + i) & 0xff))
      punt("Bad read back: offset=%u, size=%u, byte=%u, found=%u, expected=%u",
           offset, size, i, data[i], ((seed + i) & 0xff));
  }

  seed++;
}
  

int main(int argc, char** argv)
{
  if (argc < 2)
    punt("Usage: remote connection_name");
  
  PacketStream* socket = PacketStream::connect(argv[1]);

  std::cout << "Connected to simulator\n";

  MemoryBackedStreamIO streamio(socket, REMOTE_WINDOW_START,
                                REMOTE_WINDOW_SIZE);

  // Wait for the first two ingress packets to arrive.  This way we
  // know that the simulator's driver is up and running.
  streamio.wait_for_ingress();
  streamio.wait_for_ingress();

  // Try various size / alignment cross-products.
  for (unsigned offset = 0; offset < 16; offset++)
  {
    std::cout << "offset = " << offset << "\n";
    for (unsigned size = 1; size <= 16; size++)
    {
      write_read(streamio, offset, size);
    }
  }

  // Try spanning two cachelines.
  std::cout << "Testing unaligned 64 byte accesses\n";
  for (unsigned offset = 0; offset < 64; offset++)
  {
    write_read(streamio, offset, 64);
  }
  
  // Try spanning three cachelines.
  std::cout << "Testing unaligned 128 byte accesses\n";
  for (unsigned offset = 0; offset < 64; offset++)
  {
    write_read(streamio, offset, 128);
  }

  // Test all possible StreamIO transaction sizes.
  std::cout << "Testing StreamIO sizes\n";
  for (unsigned size = 1; size <= 1024; size++)
  {
    write_read(streamio, 0, size);
  }

  // Make sure byte masking works.
  std::cout << "Testing byte masks\n";
  unsigned char expected[64];
  unsigned char actual[64];
  uint64_t line_offset = TILE_WINDOW_START + (2 << 10);
  memset(expected, 0, sizeof(expected));
  for (unsigned i = 0; i < 64; i++)
  {
    unsigned char val = i + 1;
    streamio.do_write(line_offset + i, &val, sizeof(val));
    expected[i] = val;
    
    streamio.do_read(actual, line_offset, sizeof(actual));
    if (memcmp(actual, expected, sizeof(expected)) != 0)
      punt("Byte masking failure at byte %u\n", i);
  }

  // Make sure accessing an unmapped address returns -1.
  std::cout << "Testing panic PA\n";
  long long temp;
  streamio.do_read(&temp, TILE_WINDOW_START + TILE_WINDOW_SIZE, sizeof(temp));
  if (temp != -1)
    punt("Expected -1 for unmapped access, found %lld", temp);

  // Write a flag so the other side knows it can exit.
  unsigned char one = 1;
  streamio.do_write(TILE_WINDOW_START + FLAG_OFFSET, &one, sizeof(one));

  std::cout << "Remote side exiting\n";
  return 0;
}
