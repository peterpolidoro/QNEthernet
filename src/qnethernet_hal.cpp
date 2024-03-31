// SPDX-FileCopyrightText: (c) 2021-2024 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: AGPL-3.0-or-later

// qnethernet_hal.cpp implements the hardware abstraction layer (HAL).
// This file is part of the QNEthernet library.

// C includes
#include <unistd.h>

#include "qnethernet_opts.h"

// C++ includes
#if QNETHERNET_CUSTOM_WRITE
#include <cerrno>
#endif  // QNETHERNET_CUSTOM_WRITE
#include <cstdint>
#include <cstdlib>

#include <Arduino.h>  // For Serial, noInterrupts(), interrupts()
#include <Print.h>

// Processor-specific include
#if defined(TEENSYDUINO)
#if defined(__IMXRT1062__)
#include <imxrt.h>
#elif defined(ARDUINO_TEENSY30) || \
      defined(ARDUINO_TEENSY32) || defined(ARDUINO_TEENSY31) || \
      defined(ARDUINO_TEENSYLC) || \
      defined(ARDUINO_TEENSY35) || \
      defined(ARDUINO_TEENSY36)
#include <kinetis.h>
#endif  // Teensy type
#endif  // defined(TEENSYDUINO)

#include "lwip/arch.h"
#include "lwip/prot/ethernet.h"
#include "security/RandomDevice.h"

// --------------------------------------------------------------------------
//  Time
// --------------------------------------------------------------------------

extern "C" {

// Returns the current time in milliseconds.

#if defined(TEENSYDUINO)

extern volatile uint32_t systick_millis_count;

[[gnu::weak]]
uint32_t qnethernet_hal_millis() {
  return systick_millis_count;
}

#else

unsigned long millis();

[[gnu::weak]]
uint32_t qnethernet_hal_millis() {
  return millis();
}

#endif  // defined(TEENSYDUINO)

}  // extern "C"

// --------------------------------------------------------------------------
//  stdio
// --------------------------------------------------------------------------

#if QNETHERNET_CUSTOM_WRITE

// The user program can set these to something initialized. For example,
// `&Serial`, after `Serial.begin(speed)`.
namespace qindesign {
namespace network {

Print *volatile stdoutPrint = nullptr;
Print *volatile stderrPrint = nullptr;

}  // namespace network
}  // namespace qindesign

#endif  // QNETHERNET_CUSTOM_WRITE

extern "C" {

// Gets the Print* for the given file descriptor.
static inline Print *getPrint(int file) {
  switch (file) {
#if QNETHERNET_CUSTOM_WRITE
    case STDOUT_FILENO:
      return ::qindesign::network::stdoutPrint;
    case STDERR_FILENO:
      return ::qindesign::network::stderrPrint;
#else
    case STDOUT_FILENO:
    case STDERR_FILENO:
      return &Serial;
#endif  // QNETHERNET_CUSTOM_WRITE
    case STDIN_FILENO:
      return nullptr;
    default:
      return reinterpret_cast<Print *>(file);
  }
}

#if QNETHERNET_CUSTOM_WRITE

// Define this function to provide expanded stdio output behaviour. This should
// work for Newlib-based systems.
// See: https://forum.pjrc.com/threads/28473-Quick-Guide-Using-printf()-on-Teensy-ARM
// Note: Can't define as weak by default because we don't know which `_write`
//       would be chosen by the linker, this one or the one defined elsewhere
//       (Print.cpp, for example)
int _write(int file, const void *buf, size_t len) {
  Print *out = getPrint(file);
  if (out == nullptr) {
    errno = EBADF;
    return -1;
  }

  return out->write(static_cast<const uint8_t *>(buf), len);
}

#endif  // QNETHERNET_CUSTOM_WRITE

// Ensures the Print object is flushed because fflush() just flushes by writing
// to the FILE*. This doesn't necessarily send all the bytes right away. For
// example, Serial/USB output behaves this way.
void qnethernet_hal_stdio_flush(int file) {
  Print *p = getPrint(file);
  if (p != nullptr) {
    p->flush();
  }
}

}  // extern "C"

// --------------------------------------------------------------------------
//  Core Locking
// --------------------------------------------------------------------------

extern "C" {

// Asserts if this is called from an interrupt context.
[[gnu::weak]]
void qnethernet_hal_check_core_locking(const char *file, int line,
                                       const char *func) {
#if defined(TEENSYDUINO) && defined(__IMXRT1062__)
  uint32_t ipsr;
  __asm__ volatile("mrs %0, ipsr\n" : "=r" (ipsr) ::);
  if (ipsr != 0) {
    printf("%s:%d:%s()\r\n", file, line, func);
    LWIP_PLATFORM_ASSERT("Function called from interrupt context");
  }
#else
  LWIP_UNUSED_ARG(file);
  LWIP_UNUSED_ARG(line);
  LWIP_UNUSED_ARG(func);
#endif  // defined(TEENSYDUINO) && defined(__IMXRT1062__)
}

}  // extern "C"

// --------------------------------------------------------------------------
//  Randomness
// --------------------------------------------------------------------------

extern "C" {

// Initializes randomness. This is called in the EthernetClass constructor.
[[gnu::weak]]
void qnethernet_hal_init_rand() {
  // Example seed:
  // std::srand(std::time(nullptr));
// #warning "Need srand() initialization somewhere"
  std::srand(qnethernet_hal_millis());
}

// Gets a 32-bit random number for LWIP_RAND().
[[gnu::weak]]
uint32_t qnethernet_hal_rand() {
  return qindesign::security::RandomDevice::instance()();
}

}  // extern "C"

// --------------------------------------------------------------------------
//  Interrupts
// --------------------------------------------------------------------------

extern "C" {

// Disables interrupts.
[[gnu::weak]]
void qnethernet_hal_disable_interrupts() {
  noInterrupts();
}

// Enables interrupts.
[[gnu::weak]]
void qnethernet_hal_enable_interrupts() {
  interrupts();
}

}  // extern "C"

// --------------------------------------------------------------------------
//  MAC Address
// --------------------------------------------------------------------------

extern "C" {

#if !defined(TEENSYDUINO)
static const uint8_t kDefaultMACAddress[ETH_HWADDR_LEN] = {
    QNETHERNET_DEFAULT_MAC_ADDRESS,
};
#endif  // !defined(TEENSYDUINO)

// Gets the system MAC address. This will either be some platform-specific value
// or a predefined value.
void qnethernet_hal_get_system_mac_address(uint8_t mac[ETH_HWADDR_LEN]) {
  if (mac == nullptr) {
    return;
  }

#if defined(TEENSYDUINO) && defined(__IMXRT1062__)
  uint32_t m1 = HW_OCOTP_MAC1;
  uint32_t m2 = HW_OCOTP_MAC0;
  mac[0] = m1 >> 8;
  mac[1] = m1 >> 0;
  mac[2] = m2 >> 24;
  mac[3] = m2 >> 16;
  mac[4] = m2 >> 8;
  mac[5] = m2 >> 0;
#elif defined(ARDUINO_TEENSY30) || \
      defined(ARDUINO_TEENSY32) || defined(ARDUINO_TEENSY31) || \
      defined(ARDUINO_TEENSYLC)
  // usb_desc.c:usb_init_serialnumber()
  __disable_irq();
  FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
  FTFL_FCCOB0 = 0x41;
  FTFL_FCCOB1 = 15;
  FTFL_FSTAT = FTFL_FSTAT_CCIF;
  while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {
    // Wait
  }
  uint32_t num = *(uint32_t *)&FTFL_FCCOB7;
  __enable_irq();
  mac[0] = 0x04;
  mac[1] = 0xE9;
  mac[2] = 0xE5;
  mac[3] = num >> 16;
  mac[4] = num >> 8;
  mac[5] = num;
#elif defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY36)
  // usb_desc.c:usb_init_serialnumber()
  __disable_irq();
  kinetis_hsrun_disable();
  FTFL_FSTAT = FTFL_FSTAT_RDCOLERR | FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL;
  *(uint32_t *)&FTFL_FCCOB3 = 0x41070000;
  FTFL_FSTAT = FTFL_FSTAT_CCIF;
  while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {
    // Wait
  }
  uint32_t num = *(uint32_t *)&FTFL_FCCOBB;
  kinetis_hsrun_enable();
  __enable_irq();
  mac[0] = 0x04;
  mac[1] = 0xE9;
  mac[2] = 0xE5;
  mac[3] = num >> 16;
  mac[4] = num >> 8;
  mac[5] = num;
#else
  memcpy(mac, kDefaultMACAddress, ETH_HWADDR_LEN);
#endif  // Board type
}

}  // extern "C"
