// SPDX-FileCopyrightText: (c) 2024 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// PrintfChecked.h defines a printf with format string checking. As of this
// writing, format string checking for Print::printf isn't enabled on some
// platforms, which is why this is defined here.
// This file is part of the QNEthernet library.

#pragma once

#include <cstdarg>
#include <cstdio>

namespace qindesign {
namespace network {
namespace internal {

class PrintfChecked {
 public:
  // Define a format-checked printf.
  __attribute__((format(printf, 2, 3)))
  int printf(const char *format, ...) {
    std::va_list args;
    va_start(args, format);
    int retval = vdprintf(reinterpret_cast<int>(this), format, args);
    va_end(args);
    return retval;
  }
};

}  // namespace internal
}  // namespace network
}  // namespace qindesign
