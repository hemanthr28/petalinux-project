#ifndef NALLA_250_SOC_EP_NETWORK_H
#define NALLA_250_SOC_EP_NETWORK_H


/*
 * Macros to help debugging
 */

#undef PDEBUG             /* undef it, just in case */
#ifdef SNULL_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "snull: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */



/* Default timeout period */
#define NALLA_250_SOC_NETWORK_TIMEOUT_JIFFIES  (5)

#define NALLA_250_SOC_NETWORK_TRANSMIT_TIMEOUT_MILISECONDS  (1000)

#define NALLA_250_SOC_NETWORK_TRANSMIT_TIMEOUT_JIFFIES  ((NALLA_250_SOC_NETWORK_TRANSMIT_TIMEOUT_MILISECONDS * HZ) / 1000)


extern struct net_device *nalla_250_soc_net_device;


void nalla_250_soc_network_cleanup(void);
int nalla_250_soc_network_init(void* privateData);

void nalla_250_soc_network_packet_rx(struct net_device *dev, uint8_t* buffer,
                                     int bufferLength);


#endif /* ifndef NALLA_250_SOC_EP_NETWORK_H */


/* EOF - nalla_250_soc_ep_network.h */
