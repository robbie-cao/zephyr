/** @file
 @brief Ethernet

 This is not to be included by the application.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ETHERNET_H
#define __ETHERNET_H

#include <stdint.h>

#include <net/net_ip.h>
#include <net/nbuf.h>

#define NET_ETH_PTYPE_ARP  0x0806
#define NET_ETH_PTYPE_IP   0x0800
#define NET_ETH_PTYPE_IPV6 0x86dd

struct net_eth_addr {
	uint8_t addr[6];
};

struct net_eth_hdr {
	struct net_eth_addr dst;
	struct net_eth_addr src;
	uint16_t type;
} __packed;

#endif /* __ETHERNET_H */
