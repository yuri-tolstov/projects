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
// Utilities for USB device example.
//

/** Find the minimum of two values.
 * @param a First value.
 * @param b Second value.
 * @return Minimum of a and b. */
#define min(a, b) \
  ({ \
    typeof (a) _a = (a); \
    typeof (b) _b = (b); \
    _a < _b ? _a : _b; \
  })

/** Find the maximum of two values.
 * @param a First value.
 * @param b Second value.
 * @return Maximum of a and b. */
#define max(a, b) \
  ({ \
    typeof (a) _a = (a); \
    typeof (b) _b = (b); \
    _a > _b ? _a : _b; \
  })

//
// Program name, for error messages; must be supplied by program that
// utilities are linked into.
//
extern const char* myname;

void bomb(const char* fmt, ...) __attribute__((format(printf, 1, 2),
                                               noreturn));
void pbomb(const char* fmt, ...) __attribute__((format(printf, 1, 2),
                                                noreturn));
long long strtol_with_suffix(const char* s);
