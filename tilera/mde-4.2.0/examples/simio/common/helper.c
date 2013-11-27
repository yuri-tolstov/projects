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

// NOTE: Adapted from "tools/handy/...".

//! Enable various extensions.
#define _GNU_SOURCE 1

#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/time.h>

#include <netdb.h>
#include <sys/socket.h>

#include <fcntl.h>

#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>


#ifdef __NEWLIB__

// HACK: Normally in "<netinit/tcp.h>".
#define TCP_NODELAY 1

#else

#include <arpa/inet.h>
#include <netinet/tcp.h>

#endif

extern void
punt(const char* format, ...)
  __attribute__((format(printf, 1, 2), noreturn));

void
punt(const char* format, ...)
{
  fprintf(stderr, "ERROR: ");
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(128);
}


extern void
punt_with_errno(const char* format, ...)
  __attribute__((format(printf, 1, 2), noreturn));

void
punt_with_errno(const char* format, ...)
{
  int err = errno;
  fprintf(stderr, "ERROR: ");
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, ": (%d) %s.", err, strerror(err));
  exit(128);
}


extern void
warn(const char* format, ...)
  __attribute__((format(printf, 1, 2)));

void
warn(const char* format, ...)
{
  fprintf(stderr, "WARNING: ");
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");
}



int
atoi_or_die(const char* str)
{
  char* end;
  errno = 0;
  long val = strtol(str, &end, 10);
  if (*end != '\0' || end == str || errno == ERANGE ||
      (sizeof(long) > sizeof(int) && (val > INT_MAX || val < INT_MIN)))
  {
    punt("Cannot parse int from '%s'.", str);
  }
  return val;
}


void
set_close_on_exec_or_die(int fd, bool flag)
{
  int flags = fcntl(fd, F_GETFD, 0);
  if (flags != -1)
  {
    flags = flag ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC);
    if (fcntl(fd, F_SETFD, flags) == 0)
      return;
  }
  punt_with_errno("Failure in 'set_close_on_exec_or_die(%d, %d)'", fd, flag);
}


void
set_blocking_or_die(int fd, bool flag)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags != -1)
  {
    flags = !flag ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags) == 0)
      return;
  }
  punt_with_errno("Failure in 'set_blocking_or_die(%d, %d)'", fd, flag);
}


void
set_delaying_or_die(int fd, bool flag)
{
  int optname = IPPROTO_TCP;

  struct protoent* proto = getprotobyname("tcp");
  if (proto != NULL)
  {
    optname = proto->p_proto;
  }

  int val = !flag;
  if (setsockopt(fd, optname, TCP_NODELAY, (void*)&val, sizeof(val)) != 0)
  {
    punt_with_errno("Failure in 'set_delaying_or_die(%d, %d)'", fd, flag);
  }
}



int
mkstemp_boldly(char* path_template)
{
  while (true)
  {
    int fd = mkstemp(path_template);
    if (fd >= 0 || errno != EINTR)
      return fd;
  }
}


int
mkstemp_or_die(char* path_template)
{
  int fd = mkstemp_boldly(path_template);
  if (fd >= 0)
    return fd;

  punt_with_errno("Failure in 'mkstemp(\"%s\")'", path_template);
}


void
close_or_die(int fd)
{
  if (close(fd) == 0)
    return;

  punt_with_errno("Failure in 'close(%d)'", fd);
}


ssize_t
write_some_bytes_or_die(int fd, const void* buf, size_t count)
{
  size_t total = 0;

  while (total < count)
  {
    ssize_t n = write(fd, buf + total, count - total);
    if (n > 0)
    {
      total += n;
    }
    else if (errno == EPIPE ||
             errno == ECONNRESET ||
             // HACK: This happens when the other end of a PTY exits.
             // ISSUE: Actually, this has only been observed for "read()".
             (errno == EIO && isatty(fd)))
    {
      // Treat as EOF.
      if (total != 0)
        break;
      return -1;
    }
    else if (errno == EINTR)
    {
      // Try again.
    }
    else if (errno == EAGAIN)
    {
      // Nothing available.
      break;
    }
    else
    {
      punt_with_errno("Failure in 'write(%d, ..., %u)'", fd, (uint)count);
    }
  }

  return total;
}


void
write_all_bytes_or_die(int fd, const void* buf, size_t count)
{
  if (write_some_bytes_or_die(fd, buf, count) != count)
  {
    punt("Unexpected EOF (or EAGAIN).");
  }
}


ssize_t
read_some_bytes_or_die(int fd, void* buf, size_t count)
{
  size_t total = 0;

  while (total < count)
  {
    ssize_t n = read(fd, buf + total, count - total);
    if (n > 0)
    {
      total += n;
    }
    else if (n == 0)
    {
      // Actual EOF.
      if (total != 0)
        break;
      errno = EPIPE;
      return -1;
    }
    else if (errno == ECONNRESET ||
             // HACK: This happens when the other end of a PTY exits.
             (errno == EIO && isatty(fd)))
    {
      // Treat like EOF.
      if (total != 0)
        break;
      return -1;
    }
    else if (errno == EINTR)
    {
      // Try again.
    }
    else if (errno == EAGAIN)
    {
      // Nothing available.
      break;
    }
    else
    {
      punt_with_errno("Failure in 'read(%d, ..., %u)'", fd, (uint)count);
    }
  }

  return total;
}


void
read_all_bytes_or_die(int fd, void* buf, size_t count)
{
  if (read_some_bytes_or_die(fd, buf, count) != count)
  {
    punt("Unexpected EOF (or EAGAIN).");
  }
}



int
simple_listen(uint16_t* port, int backlog)
{
  // Create a listener.
  int one = 1;
  int listener_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if ((listener_fd < 0) ||
      setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR,
                 (void*)&one, sizeof(one)) != 0)
  {
    punt_with_errno("Failure in 'simple_listen(%d)'", *port);
  }

  // Listen on the given port.
  struct sockaddr_in addr = { 0 };
  addr.sin_family = PF_INET;
  addr.sin_port = htons(*port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(listener_fd, (struct sockaddr*) &addr, sizeof(addr)) != 0 ||
      listen(listener_fd, backlog) != 0)
  {
    punt_with_errno("Failure in 'simple_listen(%d)'", *port);
  }

  // Acquire the actual port being used.
  if (*port == 0)
  {
    socklen_t addrlen = sizeof(addr);
    if (getsockname(listener_fd, (struct sockaddr*) &addr, &addrlen) != 0)
      punt_with_errno("Failure in 'simple_listen(%d)'", *port);
    if (addrlen != sizeof(addr))
      punt("Failure in 'simple_listen(%d)'.", *port);
    *port = ntohs(addr.sin_port);
  }

  // Default to "close_on_exec".
  set_close_on_exec_or_die(listener_fd, true);

  return listener_fd;
}


int
simple_accept(int listener_fd)
{
  int fd;

  while (true)
  {
    fd = accept(listener_fd, NULL, NULL);

    if (fd >= 0)
      break;

    if (errno == EINTR)
      continue;

    punt_with_errno("Failure in 'accept()'");
  }

  set_blocking_or_die(fd, false);
  set_delaying_or_die(fd, false);

  return fd;
}


int
simple_connect_aux(const char* host, uint16_t port)
{
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = PF_INET;
  addr.sin_port = htons(port);

  if (host != NULL)
  {

#ifdef __NEWLIB__

    punt("Failure in 'gethostbyname(\"%s\")': %s.",
         host, strerror(ENOSYS));

#else

    struct hostent* hostent = gethostbyname(host);
    if (hostent == NULL)
      punt("Failure in 'gethostbyname(\"%s\")'.", host);
    //--assert(hostent->h_length == sizeof(struct in_addr));
    memcpy(&addr.sin_addr.s_addr, hostent->h_addr, hostent->h_length);

#endif

  }
  else
  {
    // Basically "localhost", but optimized.
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  }

  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    punt_with_errno("Failure in 'socket()");

  while (true)
  {
    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) == 0)
      break;

    if (errno == EINTR)
      continue;

    close_or_die(fd);
    return -1;
  }

  set_blocking_or_die(fd, false);
  set_delaying_or_die(fd, false);

  return fd;
}


int
simple_connect(const char* host, uint16_t port)
{
  int fd = simple_connect_aux(host, port);

  if (fd < 0)
    punt_with_errno("Failure in 'simple_connect(\"%s\", %u)'", host, port);

  return fd;
}


int
simple_connect_string_aux(const char* portspec)
{
  // Assume "port" notation.
  char* host = NULL;
  const char* port = portspec;

  // Accept "host:port" notation.
  const char* colon = strrchr(port, ':');
  if (colon != NULL)
  {
    size_t len = colon - port;
    host = alloca(len + 1);
    strncpy(host, port, len);
    host[len] = '\0';
    port = colon + 1;
  }

  // Analyze the port (ala "atou16_or_die()").
  char* end;
  unsigned long val = strtoul(port, &end, 10);
  if (*end != '\0' || end == port || val > 65535 || val == 0)
    punt("Invalid network specifier '%s' (use '[HOST:]PORT').", portspec);

  return simple_connect_aux(host, (uint16_t)val);
}


int
simple_connect_string(const char* portspec)
{
  int fd = simple_connect_string_aux(portspec);

  if (fd < 0)
    punt_with_errno("Failure in 'simple_connect_string(\"%s\")'", portspec);

  return fd;
}


void
timeval_now(struct timeval* now)
{
  (void)gettimeofday(now, NULL);
}


void
timeval_diff(struct timeval* diff,
             const struct timeval* x,
             const struct timeval* y)
{
  if (x->tv_sec > y->tv_sec && x->tv_usec < y->tv_usec)
  {
    diff->tv_sec = x->tv_sec - 1 - y->tv_sec;
    diff->tv_usec = 1000000 + x->tv_usec - y->tv_usec;
  }
  else if (x->tv_sec > y->tv_sec ||
           (x->tv_sec == y->tv_sec && x->tv_usec > y->tv_usec))
  {
    diff->tv_sec = x->tv_sec - y->tv_sec;
    diff->tv_usec = x->tv_usec - y->tv_usec;
  }
  else
  {
    diff->tv_sec = 0;
    diff->tv_usec = 0;
  }
}


void
timeval_elapsed(struct timeval* elapsed, struct timeval* since)
{
  struct timeval now;
  timeval_now(&now);
  timeval_diff(elapsed, &now, since);
}
