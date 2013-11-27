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

#include "PacketStream.h"
#include "helper.h"

#include <errno.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>


PacketStream::PacketStream(int fd)
  : m_fd(fd),
    m_seen_pcap_header(false)
{
  set_blocking_or_die(fd, true);

  // Write the file header.
  TCPDumpHeader header = {
    TCPDumpHeader::TCPDUMP_MAGIC,
    TCPDumpHeader::PCAP_VERSION_MAJOR,
    TCPDumpHeader::PCAP_VERSION_MINOR,
    0, 0, TCPDumpHeader::SNAPSHOT_MAX,
    TCPDumpHeader::MEDIA_ETHERNET
  };
  write_all_bytes_or_die(fd, &header, sizeof(header));  
}


//! Returns true iff str is a string consisting of only 1 or more digits.
static bool
is_number(const std::string& str)
{
  if (str.empty())
    return false;

  for (size_t i = 0; i < str.size(); i++)
  {
    if (!isdigit(str[i]))
      return false;
  }
  return true;
}


/** Initialize an abstract unix domain socket for the given name.
 * If the name is too long, only the prefix and suffix are used.
 */
static size_t
init_abstract_socket(struct sockaddr_un* addr, const std::string& name)
{
  memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_UNIX;

  // Use a prefix specific to this program so we don't conflict with
  // other unrelated programs running on the host.
  static const char prefix[] = "\0tile-sim-packets:";
  const size_t prefix_size = sizeof(prefix) - 1; /* Skip '\0' at the end. */

  const size_t max_len = sizeof(addr->sun_path) - prefix_size;
  size_t len = name.size();
  if (len > max_len)
  {
    punt("Socket name '%s' is too long; max length %zu characters.",
         name.c_str(), max_len);
  }

  memcpy(addr->sun_path, prefix, prefix_size);
  memcpy(&addr->sun_path[prefix_size], &name[0], len);

  return sizeof(addr->sun_family) + prefix_size + len;
}


PacketStream*
PacketStream::connect(const std::string& argument)
{
  int port = -1;
  int fd;
  
  if (is_number(argument))
  {
    port = atoi_or_die(argument.c_str());
    if (port <= 0)
      punt("Invalid port number '%d'.", port);

    fd = simple_connect(NULL, port);
  }
  else if (!argument.empty())
  {
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      punt_with_errno("Failure in 'socket()");

    struct sockaddr_un addr;
    size_t addr_size = init_abstract_socket(&addr, argument.c_str());
    
    for (int i = 60; i >= 0; i--)
    {
      if (::connect(fd, (struct sockaddr*) &addr, addr_size) == 0)
        break;

      if (errno == EINTR)
        continue;

      if (i == 0)
        punt_with_errno("Unable to connect to unix domain socket");

      // Retry for a while.
      sleep(1);
    }
  }
  else if (argument.empty())
  {
    punt("Invalid empty string for connect port.");
  }
  
  return new PacketStream(fd);
}


bool
PacketStream::get_next_packet(TCPDumpPacketHeader* header,
                              std::vector<unsigned char>& payload)
{
  // Check whether there's something to read.
  
  // Each connection starts with a pcap header.
  if (!m_seen_pcap_header)
  {
    TCPDumpHeader file_header;

    read_all_bytes_or_die(m_fd, &file_header, sizeof(file_header));

    if (file_header.magic != TCPDumpHeader::TCPDUMP_MAGIC ||
        file_header.version_major != TCPDumpHeader::PCAP_VERSION_MAJOR ||
        file_header.version_minor != TCPDumpHeader::PCAP_VERSION_MINOR ||
        file_header.media_type != TCPDumpHeader::MEDIA_ETHERNET)
    {
      punt("PCAP header is not in little-endian Ethernet TCPdump 2.4 format");
    }
    m_seen_pcap_header = true;
  }

  // Read a packet header.
  read_all_bytes_or_die(m_fd, header, sizeof(*header));

  // Read the packet.
  payload.resize(header->packet_captured);
  read_all_bytes_or_die(m_fd, &payload[0], header->packet_captured);

  return true;
}


void
PacketStream::write_packet(const TCPDumpPacketHeader* header,
                           const std::vector<unsigned char>& payload)
{
  write_all_bytes_or_die(m_fd, header, sizeof(*header));
  write_all_bytes_or_die(m_fd, &payload[0], payload.size());
}
