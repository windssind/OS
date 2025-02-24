#ifndef __NIC_H__
#define __NIC_H__
/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *load device drivers for different NICs
 */

#include "klib.h"
#include "arp_frame.h"
#include <stdint.h>

//Generic NIC device driver container
struct nic_device {
  //void *driver;
  uint8_t mac_addr[6];
  //void (*send_packet) (void *driver, uint8_t* pkt, uint16_t length);
  //void (*recv_packet) (void *driver, uint8_t* pkt, uint16_t length);
};

//Holds the instances of nic_devices for loaded devices
//Lets say for now there can't be more than 1 loaded NIC device
extern struct nic_device nic_devices[1];

void register_device(struct nic_device nd);
int get_device(char* interface, struct nic_device** nd);

#endif
