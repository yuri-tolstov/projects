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
// Utility routines for USB device example.
//

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/** Print our name, followed by an error message, and exit.
 * @param fmt Message in printf-style format, followed by arguments.
 */
void
bomb(const char* fmt, ...)
{
  va_list ap;

  fprintf(stderr, "%s: ", myname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(1);
}


/** Print our name, followed by an error message, and a string describing
 *  the last encountered error, then exit.
 * @param fmt Message in printf-style format, followed by arguments.
 */
void
pbomb(const char* fmt, ...)
{
  int incoming_errno = errno;
  va_list ap;

  fprintf(stderr, "%s: ", myname);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, ":%s\n", strerror(incoming_errno));
  exit(1);
}


/** Convert a string to a long integer.  Supports a trailing "k", "m",
 *  or "g", which multiplies the result by 2^10, 2^20, or 2^30, respectively.
 * @param s String to convert.
 * @return Parsed value.  Exits on failure.
 */
long long
strtol_with_suffix(const char* s)
{
  char* endptr;
  long long val = strtol(s, &endptr, 0);

  if (*endptr == 'k' || *endptr == 'K')
  {
    val <<= 10;
    endptr++;
  }
  else if (*endptr == 'm' || *endptr == 'M')
  {
    val <<= 20;
    endptr++;
  }
  else if (*endptr == 'g' || *endptr == 'G')
  {
    val <<= 30;
    endptr++;
  }

  if (*endptr)
    bomb("improperly formatted numerical argument %s", s);

  return val;
}
