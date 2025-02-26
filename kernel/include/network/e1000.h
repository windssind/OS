#ifndef __E1000_H__
#define __E1000_H__

#include "pci.h"
#include "klib.h"

/* PCI Device IDs */
#define E1000_DEV_ID_82540EM             0x100E

/* Register Set. (82543, 82544)
 *
 * RW - register is both readable and writable
 * RO - register is read only
 * WO - register is write only
 * R/clr - register is read only and is cleared when read
 * A - register array
 */

/* Registers */
#define E1000_CTL      (0x00000/4)  /* Device Control Register - RW */
#define E1000_ICR      (0x000C0/4)  /* Interrupt Cause Read - R */
#define E1000_ICS      (0x000C8/4)  /* Interrupt Cause Set - R */
#define E1000_IMS      (0x000D0/4)  /* Interrupt Mask Set - RW */
#define E1000_IMC      (0x000D8/4)  /* Interrupt Mask Clear - RW */
#define E1000_RCTL     (0x00100/4)  /* RX Control - RW */
#define E1000_TCTL     (0x00400/4)  /* TX Control - RW */
#define E1000_TIPG     (0x00410/4)  /* TX Inter-packet gap -RW */
#define E1000_RDBAL    (0x02800/4)  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    (0x02804/4)  /* RX Descriptor Base Address Low - RW */
#define E1000_RDTR     (0x02820/4)  /* RX Delay Timer */
#define E1000_RADV     (0x0282C/4)  /* RX Interrupt Absolute Delay Timer */
#define E1000_RDH      (0x02810/4)  /* RX Descriptor Head - RW */
#define E1000_RDT      (0x02818/4)  /* RX Descriptor Tail - RW */
#define E1000_RDLEN    (0x02808/4)  /* RX Descriptor Length - RW */
#define E1000_RSRPD    (0x02C00/4)  /* RX Small Packet Detect Interrupt */
#define E1000_TDBAL    (0x03800/4)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    (0x03804/4)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    (0x03808/4)  /* TX Descriptor Length - RW */
#define E1000_TDH      (0x03810/4)  /* TX Descriptor Head - RW */
#define E1000_TDT      (0x03818/4)  /* TX Descripotr Tail - RW */
#define E1000_TADV     (0x0382C/4)  /* TX Interrupt Absolute Delay Timer */
#define E1000_MTA      (0x05200/4)  /* Multicast Table Array - RW Array */
#define E1000_RAL       (0x05400/4)  /* Receive Address - RW Array */
#define E1000_RAH       (0x05404/4)  /* Receive Address - RW Array */

/* Device Control */
#define E1000_CTL_SLU     0x00000040    /* set link up */
#define E1000_CTL_FRCSPD  0x00000800    /* force speed */
#define E1000_CTL_FRCDPLX 0x00001000    /* force duplex */
#define E1000_CTL_RST     0x00400000    /* full reset */

/* Transmit Control */
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_COLD_SHIFT 12
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* Receive Control */
#define E1000_RCTL_RST            0x00000001    /* Software reset */
#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_SBP            0x00000004    /* store bad packet */
#define E1000_RCTL_UPE            0x00000008    /* unicast promiscuous enable */
#define E1000_RCTL_MPE            0x00000010    /* multicast promiscuous enab */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC loopback mode */
#define E1000_RCTL_LBM_SLP        0x00000080    /* serial link loopback mode */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* tcvr loopback mode */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* Descriptor type mask */
#define E1000_RCTL_DTYP_PS        0x00000400    /* Packet Split descriptor */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* rx desc min threshold size */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* rx desc min threshold size */
#define E1000_RCTL_MO_SHIFT       12            /* multicast offset shift */
#define E1000_RCTL_MO_0           0x00000000    /* multicast offset 11:0 */
#define E1000_RCTL_MO_1           0x00001000    /* multicast offset 12:1 */
#define E1000_RCTL_MO_2           0x00002000    /* multicast offset 13:2 */
#define E1000_RCTL_MO_3           0x00003000    /* multicast offset 15:4 */
#define E1000_RCTL_MDR            0x00004000    /* multicast desc ring 0 */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 0 */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SZ_1024        0x00010000    /* rx buffer size 1024 */
#define E1000_RCTL_SZ_512         0x00020000    /* rx buffer size 512 */
#define E1000_RCTL_SZ_256         0x00030000    /* rx buffer size 256 */
/* these buffer sizes are valid if E1000_RCTL_BSEX is 1 */
#define E1000_RCTL_SZ_16384       0x00010000    /* rx buffer size 16384 */
#define E1000_RCTL_SZ_8192        0x00020000    /* rx buffer size 8192 */
#define E1000_RCTL_SZ_4096        0x00030000    /* rx buffer size 4096 */
#define E1000_RCTL_VFE            0x00040000    /* vlan filter enable */
#define E1000_RCTL_CFIEN          0x00080000    /* canonical form enable */
#define E1000_RCTL_CFI            0x00100000    /* canonical form indicator */
#define E1000_RCTL_DPF            0x00400000    /* discard pause frames */
#define E1000_RCTL_PMCF           0x00800000    /* pass MAC control frames */
#define E1000_RCTL_BSEX           0x02000000    /* Buffer size extension */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* Flexible buffer size */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* Flexible buffer shift */

 /* TX Inter-packet gap bit definitions */
#define E1000_TIPG_IPGT      0x000003FF
#define E1000_TIPG_IPGR1     0x000FFA00
#define E1000_TIPG_IPGR2     0x3FF00000

#define DATA_MAX 1518

/* Transmit Descriptor command definitions [E1000 3.3.3.1] */
/*When set, indicates the last descriptor making up the packet. One or many 
descriptors can be used to form a packet*/
#define E1000_TXD_CMD_EOP    0x01 /* End of Packet */

/* When set, the Ethernet controller needs to report the status information. This ability 
may be used by software that does in-memory checks of the transmit descriptors to 
determine which ones are done and packets have been buffered in the transmit 
FIFO. Software does it by looking at the descriptor status byte and checking the 
Descriptor Done (DD) bit. */
#define E1000_TXD_DTYP_D     0x10 /* Data Descriptor */
#define E1000_TXD_DTYP_C     0x00 /* Context Descriptor */

#define E1000_TXD_CMD_TSE    0x04 /* TCP Seg enable */
#define E1000_TXD_CMD_RS     0x08 /* Report Status */
#define E1000_TXD_CMD_DEXT   0x20 /* Descriptor extension (0 = legacy) */
#define E1000_TXD_CMD_IDE    0x80 /* Interrupt Delay Enable */

#define E1000_TXD_POPTS_IXSM 0x01       /* Insert IP checksum */
#define E1000_TXD_POPTS_TXSM 0x02       /* Insert TCP/UDP checksum */

/* Transmit Descriptor status definitions [E1000 3.3.3.2] */
// Indicates that the descriptor is finished and is written back 
// either after the descriptor has been processed (with RS set) 
// or for the 82544GC/EI, after the packet has been transmitted on the wire 
#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */


/* Receive Descriptor bit definitions [E1000 3.2.3.1] */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */

#define E1000_RAH_AV  0x80000000        /* Receive descriptor valid */

// a strange problem to solve in the future: 
// if set TXRING_LEN to 64 and RXRING_LEN to 128, the kernel will reach 0022b200, 
// and the system will crash in init_page. 

#define TXRING_LEN 32

#define RXRING_LEN 64

//#define TXDATA_SIZE 2048
#define DATA_SIZE 4096

////

/* Receive Descriptor */
struct e1000_rx_desc {
    uint32_t buffer_addr; // Address of the transmit descriptor in the host memory.
    uint32_t addr_padding;
    uint16_t length;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};

///

struct e1000_tx_desc {
    uint32_t buffer_addr; // Address of the transmit descriptor in the host memory.
    uint32_t addr_padding;
    uint16_t length;
    uint8_t cso; // Checksum Offset, indicates where to insert a TCP checksum if this mode is enabled
    uint8_t cmd; // Command field
    uint8_t status;
    uint8_t css; // Checksum Start, where to begin computing the checksum
    uint16_t special;
};

/* Data */
struct e1000_data {
    uint8_t data[DATA_SIZE];
};

#define VALUEATMASK(value, mask) value * ((mask) & ~((mask) << 1))



#define STATUS   0x00008  /* Device Status - RO */

int e1000_attach(struct pci_func *pcif);
int e1000_transmit(const char *buf, unsigned int len);
int e1000_receive(char *buf, unsigned int len);

void e1000_emulator_handle();


#endif	// JOS_KERN_E1000_H