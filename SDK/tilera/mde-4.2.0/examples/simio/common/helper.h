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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define sanity(x) assert(x)


#ifdef __cplusplus
extern "C" {
#endif

extern void
punt(const char* format, ...)
  __attribute__((format(printf, 1, 2), noreturn));
  
extern void
punt_with_errno(const char* format, ...)
  __attribute__((format(printf, 1, 2), noreturn));
  
extern int
atoi_or_die(const char* str);

extern int
simple_connect(const char* host, uint16_t port);

extern void
set_blocking_or_die(int fd, bool flag);
  
extern void
write_all_bytes_or_die(int fd, const void* buf, size_t count);

extern ssize_t
read_some_bytes_or_die(int fd, void* buf, size_t count);

extern void
read_all_bytes_or_die(int fd, void* buf, size_t count);
  
#ifdef __cplusplus
}
#endif
