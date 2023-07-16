

#ifndef NALLA_250_SOC_END_POINT_H
#define NALLA_250_SOC_END_POINT_H


typedef struct IrqGeneratorRegisters
{
   uint32_t initiate;
   uint32_t padding;
   uint32_t mask;
   uint32_t reason;
} IrqGeneratorRegisters_t;


typedef struct CirularBuffer
{
   uint8_t* data;
   uint32_t head;
   uint32_t tail;
   uint32_t sizeBytes;
} CircularBuffer_t;

/* Function prototypes. */
int pcHostDriverUp(void);
int canSendPacketToPcHost(void);
void sendPacketToPcHost(uint8_t* buffer, int lengthBytes);
void flushSocToPcPacketBuffer(void);
int nalla250_SocIoctl(struct file *file, unsigned int cmd, unsigned long inArg,uint32_t mode);
int nalla250_Soc_ioctl_int_wait(unsigned long pArg, uint32_t int_mask);

#define PCIE_BAR_BASE_ADDRESS  (0x80000000)
#define PCIE_SHARED_MEMORY (PCIE_BAR_BASE_ADDRESS + 0x10000)
#define PCIE_CONTROL_REGISTERS (PCIE_BAR_BASE_ADDRESS + 0x20000)

#define LOOPBACK_BUFFER_SIZE_BYTES  (1024*1024)

#define IRQ_GENERATOR_BASE_ADDRESS  (PCIE_CONTROL_REGISTERS + 0x20)
#define IRQ_GENERATOR_LENGTH_BYTES  (16)

#define IRQ_PC_HOST_SENT_BUFFER_TO_SOC        (0x1)
#define IRQ_PC_HOST_PROCESSED_BUFFER_FROM_SOC (0x2)
#define IRQ_PC_HOST_RING_BUFFER_INTERRUPT     (0x8)
#define IRQ_GENERATOR_MASK  (IRQ_PC_HOST_RING_BUFFER_INTERRUPT |	\
			     IRQ_PC_HOST_SENT_BUFFER_TO_SOC |		\
                             IRQ_PC_HOST_PROCESSED_BUFFER_FROM_SOC)


/* #define PCIE_IRQ_GENERATOR_BASE_ADDRESS  (0xFF200310) */
#define PCIE_IRQ_GENERATOR_LENGTH_BYTES  (16)

#define PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST  (0x1)

#define PCIE_IRQ_GENERATOR_MASK  (PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST)

/* #define FIRMWARE_VERSION_ADDRESS  (0xFF200010) */

#define FIRMWARE_TYPE_MASK  (0xFFFF0000)
/* #define EP_FIRMWARE_TYPE_CODE  (0xBABE0000) */

#define DATA_FROM_PC_HOST_BUFFER_ADDRESS       (PCIE_SHARED_MEMORY)
#define DATA_FROM_PC_HOST_BUFFER_LENGTH_BYTES  (2000)

#define PC_HOST_DRIVER_UP_ADDRESS  (PCIE_SHARED_MEMORY + 0x000007D0)
#define PC_HOST_MAC_ADDRESS        (PCIE_SHARED_MEMORY + 0x000007E0)
#define PC_HOST_MAC_LENGTH_BYTES   (8)


#define DATA_TO_PC_HOST_BUFFER_ADDRESS       (PCIE_SHARED_MEMORY + 0x00001000)
#define DATA_TO_PC_HOST_BUFFER_LENGTH_BYTES  (2000)

#define SOC_DRIVER_UP_ADDRESS  (PCIE_SHARED_MEMORY + 0x000017D0)
#define SOC_MAC_ADDRESS        (PCIE_SHARED_MEMORY + 0x000017E0)
#define SOC_MAC_LENGTH_BYTES   (8)

#define DRIVER_UP_SIGIL  (0x00C0FFEE)

#define INGRESS_PACKET_BUFFER_LENGTH_BYTES (4000)


#endif /* ifndef NALLA_250_SOC_END_POINT_H */


/* EOF - nalla_250_soc_end_point.h */
