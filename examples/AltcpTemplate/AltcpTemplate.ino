// SPDX-FileCopyrightText: (c) 2023 Shawn Silverman <shawn@pobox.com>
// SPDX-License-Identifier: MIT

// AltcpTemplate shows how to use the altcp framework for creating
// custom connections. It defines a function that returns a TLS
// configuration.
//
// Prerequisites: Enable the LWIP_ALTCP and optionally the
//                LWIP_ALTCP_TLS lwIP options.
// Big caveat: This example will only do TLS if there's an available
//             TLS implementation.
//
// This file is part of the QNEthernet library.

#include <QNEthernet.h>

using namespace qindesign::network;

constexpr uint32_t kDHCPTimeout = 15000;  // 15 seconds

// Connection information
constexpr char kHost[]{"www.google.com"};
constexpr char kRequest[]{
    "HEAD / HTTP/1.1\r\n"
    "Host: www.google.com\r\n"
    "Connection: close\r\n"
    "\r\n"
};
constexpr uint16_t kPort = 80;   // TLS generally uses port 443

// The qnethernet_allocator_arg() function returns a pointer to
// a 'struct altcp_tls_config'. On failure, altcp_tls_free_config()
// will be called on the returned value.
#if LWIP_ALTCP && LWIP_ALTCP_TLS
std::function<void *(const ip_addr_t *, uint16_t)> qnethernet_allocator_arg =
    [](const ip_addr_t *ipaddr, uint16_t port) {
      printf("[[qnethernet_allocator_arg(%s, %u): %s]]\r\n",
             ipaddr_ntoa(ipaddr), port,
             ipaddr == NULL ? "Listen" : "Connect");
      return nullptr;
    };
#endif  // LWIP_ALTCP && LWIP_ALTCP_TLS

EthernetClient client;

bool disconnectedPrintLatch = false;  // Print "disconnected" only once
size_t dataCount = 0;

// Program setup.
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
    // Wait for Serial
  }
  printf("Starting...\r\n");

  uint8_t mac[6];
  Ethernet.macAddress(mac);  // This is informative; it retrieves, not sets
  printf("MAC = %02x:%02x:%02x:%02x:%02x:%02x\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Get an IP address
  printf("Starting Ethernet with DHCP...\r\n");
  if (!Ethernet.begin()) {
    printf("Failed to start Ethernet\r\n");
    return;
  }
  if (!Ethernet.waitForLocalIP(kDHCPTimeout)) {
    printf("Failed to get IP address from DHCP\r\n");
    return;
  }

  IPAddress ip = Ethernet.localIP();
  printf("    Local IP    = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.subnetMask();
  printf("    Subnet mask = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.gatewayIP();
  printf("    Gateway     = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);
  ip = Ethernet.dnsServerIP();
  printf("    DNS         = %u.%u.%u.%u\r\n", ip[0], ip[1], ip[2], ip[3]);

  // Connect and send the request
  printf("Connecting and sending request...\r\n");
  if (client.connect(kHost, kPort) != 1) {
    printf("Failed to connect\r\n");
    disconnectedPrintLatch = true;
  } else {
    client.writeFully(kRequest);
    client.flush();
    dataCount = 0;
    printf("[Awaiting response...]\r\n");
  }
}

// Main program loop.
void loop() {
  // Read the response
  if (client.connected()) {
    int avail = client.available();
    if (avail > 0) {
      dataCount += avail;
      for (int i = 0; i < avail; i++) {
        putc(client.read(), stdout);
      }
    }
  } else {
    if (!disconnectedPrintLatch) {
      disconnectedPrintLatch = true;
      printf("[Client disconnected]\r\n"
             "[Data count = %zu]\r\n", dataCount);
    }
  }
}
