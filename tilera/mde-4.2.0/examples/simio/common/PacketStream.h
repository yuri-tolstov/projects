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

#ifndef TOOLS_SIMULATOR_GSIM_SIMIO_PACKET_STREAM_H
#define TOOLS_SIMULATOR_GSIM_SIMIO_PACKET_STREAM_H

#include <vector>
#include <string>

/** A structure detailing the head of a TCPDump file. */
struct TCPDumpHeader
{
  /** Magic number -- should be TCPDUMP_MAGIC */
  unsigned int magic;
  /** Major version number -- should be PCAP_VERSION_MAJOR */
  unsigned short version_major;
  /** Major version number -- should be PCAP_VERSION_MINOR */
  unsigned short version_minor;
  /** Timezone offset, usually zero */
  unsigned int time_offset;
  /** Timestamp accuracy, usually zero */
  unsigned int timestamp_accuracy;
  /** Snapshot length (SNAPSHOT_MAX if we're capturing full packets) */
  unsigned int snapshot_length;
  /** Media type */
  unsigned int media_type;

  /** Magic number for TCPdump files */
  static const unsigned int TCPDUMP_MAGIC = 0xa1b2c3d4;
    /** Major version number  for TCPdump files */
  static const unsigned short PCAP_VERSION_MAJOR = 2;
  /** Minor version number for TCPdump files */
  static const unsigned short PCAP_VERSION_MINOR = 4;
  /** Maximum snapshot size */
  static const unsigned int SNAPSHOT_MAX = 0xffff;
  /** Magic number for TCPdump files */
  static const unsigned int MEDIA_ETHERNET = 1;
};

/** A structure detailing the header TCPDump puts on the front of each
    packet. */
struct TCPDumpPacketHeader
{
  /** The number of seconds (we interpret as milliseconds) from the
   * epoch until the moment of transmission of this packet */
  unsigned int seconds;
  /** The number of microseconds (we interpret as nanoseconds) */
  unsigned int microseconds;
  /** The actual length of the packet */
  unsigned int packet_length;
  /** The amount we captured */
  unsigned int packet_captured;
};


/** A stream of packets for a MAC. */
class PacketStream
{
public:
  /** Construct a PacketStream. */
  PacketStream(int fd);

  /** Factory that connects to a "name", as handled by the simulator's
   *  MAC parameters.
   *
   * @param argument A string containing either an integer port number
   * or a string to be used as the base of a unix domain socket.
   */
  static PacketStream* connect(const std::string& argument);

  /** Destroy a PacketStream. */
  ~PacketStream();

  /** Returns the next packet by reference.  Returns true if one is
   * available, otherwise returns false.
   */
  bool get_next_packet(TCPDumpPacketHeader* header,
                       std::vector<unsigned char>& payload);

  /** Write a packet over this stream. */
  void write_packet(const TCPDumpPacketHeader* header,
                    const std::vector<unsigned char>& payload);

protected:
  /** Our file descriptor. */
  int m_fd;

  /** Have we read the pcap header yet? */
  bool m_seen_pcap_header;
};

#endif // TOOLS_SIMULATOR_GSIM_SIMIO_PACKET_STREAM_H
