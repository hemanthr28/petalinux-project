

#ifndef NALLA_385_ASOC_DEVICE_H
#define NALLA_385_ASOC_DEVICE_H

#include <linux/version.h>

void nalla_250_soc_device_cleanup(void);

int nalla_250_soc_device_init(void* privateData);



struct nalla_250_soc_int_wait
{
  unsigned int interrupt;           /*!<\brief interrupt to wait on. */
  unsigned int timeout;             /*!<\brief timeout in ms. */
};

#define NALLA_250_SOC_IRQ_VALUE_BUFFER_SENT_TO_SOC    (0x01)
#define NALLA_250_SOC_IRQ_VALUE_BUFFER_READ_FROM_SOC  (0x02)
#define NALLA_250_SOC_IRQ_VALUE_RING_BUFFER_INTERRUPT (0x08)

#define NALLA_250_SOC_IOCTL_MAGIC_NUM  'Y'
#define NALLA_250_SOC_IOCTL_INTERRUPT_WAIT         _IOWR(NALLA_250_SOC_IOCTL_MAGIC_NUM,8,int)
#define NALLA_250_SOC_IOCTL_INTERRUPT_GEN          _IOW(NALLA_250_SOC_IOCTL_MAGIC_NUM,9,int)


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
    #define access_ok_handler(a,b,c) access_ok(b,c)
#else
    #define access_ok_handler(a,b,c) access_ok(a,b,c)
#endif


#endif /* ifndef NALLA_385_ASOC_DEVICE_H */



/* nalla_250_soc_device.h */
