// SPDX-FileCopyrightText: (c) 2023-2024 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// RandomDevice.cpp implements RandomDevice.
// This file is part of the QNEthernet library.

#include "RandomDevice.h"

#include "adapters/pgmspace.h"

#if (defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)) && \
    !QNETHERNET_USE_ENTROPY_LIB

#include "entropy.h"

namespace qindesign {
namespace security {

STATIC_INIT_DEFN(RandomDevice, randomDevice);

RandomDevice &RandomDevice::instance() {
  return randomDevice;
}

FLASHMEM RandomDevice::RandomDevice() {
  if (!trng_is_started()) {
    trng_init();
  }
}

RandomDevice::result_type RandomDevice::operator()() {
  return entropy_random();
}

}  // namespace security
}  // namespace qindesign

#elif defined(__has_include) && __has_include(<Entropy.h>)

#include <Entropy.h>
#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
#include <imxrt.h>
#endif  // defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)

namespace qindesign {
namespace security {

STATIC_INIT_DEFN(RandomDevice, randomDevice);

RandomDevice &RandomDevice::instance() {
  return randomDevice;
}

FLASHMEM RandomDevice::RandomDevice() {
#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
  bool doEntropyInit = ((CCM_CCGR6 & CCM_CCGR6_TRNG(CCM_CCGR_ON_RUNONLY)) !=
                        CCM_CCGR6_TRNG(CCM_CCGR_ON_RUNONLY)) ||
                       ((TRNG_MCTL & TRNG_MCTL_TSTOP_OK) != 0);
#else
  bool doEntropyInit = true;
#endif  // defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
  if (doEntropyInit) {
    Entropy.Initialize();
  }
}

RandomDevice::result_type RandomDevice::operator()() {
  return Entropy.random();
}

}  // namespace security
}  // namespace qindesign

#else

#include <cstdlib>

namespace qindesign {
namespace security {

STATIC_INIT_DEFN(RandomDevice, randomDevice);

RandomDevice &RandomDevice::instance() {
  return randomDevice;
}

FLASHMEM RandomDevice::RandomDevice() {
}

RandomDevice::result_type RandomDevice::operator()() {
  return std::rand();
}

}  // namespace security
}  // namespace qindesign

#endif  // Which implementation
