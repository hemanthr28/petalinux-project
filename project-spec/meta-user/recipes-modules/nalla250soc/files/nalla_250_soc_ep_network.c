#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/interrupt.h> /* mark_bh */

#include <linux/in.h>
#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */
#include <linux/skbuff.h>

#include "nalla_250_soc_ep_network.h"
#include "nalla_250_soc_end_point.h"

#include <linux/in6.h>
#include <asm/checksum.h>


struct net_device *nalla_250_soc_net_device;

static void nalla_250_soc_network_tx_timeout(struct net_device *dev);


static int timeout = NALLA_250_SOC_NETWORK_TIMEOUT_JIFFIES;

/* Mapped locations of hardware address of the SoC board and PC Host, as far as
   Ethernet-over-PCIe link is concerned. */
static u8* pcHostMacRegs = NULL;
static u8* socMacRegs    = NULL;


/*
 * This structure is private to each device. It is used to track statistics
 * relevant to the network device.
 */

typedef struct nalla_250_soc_network_private {
   struct net_device_stats stats;

   struct net_device *dev;

   uint64_t lastPacketTxTime;

   void* privateData;

} nalla_250_soc_network_private_t;



/*
 * Open and close
 */

int nalla_250_soc_network_open(struct net_device *dev)
{
  char mac[6];
  mac[0] = socMacRegs[0];
  mac[1]=socMacRegs[1];
  mac[2]=socMacRegs[2];
  mac[3]=socMacRegs[3];
  mac[4]=socMacRegs[4];
  mac[5]=socMacRegs[5];
   /* Assign hardware address of board. */
   memcpy(dev->dev_addr, mac, 6);
   netif_start_queue(dev);
   return 0;
}

int nalla_250_soc_network_release(struct net_device *dev)
{
   /* release ports, irq and such -- like fops->close */

   netif_stop_queue(dev); /* can't transmit any more */
   return 0;
}

/*
 * Configuration changes (passed on by ifconfig)
 */
int nalla_250_soc_network_config(struct net_device *dev, struct ifmap *map)
{
   if (dev->flags & IFF_UP) /* can't act on a running interface */
      return -EBUSY;

   /* Don't allow changing the I/O address */
   if (map->base_addr != dev->base_addr)
   {
      printk(KERN_WARNING "nalla_250_soc_network: Can't change I/O address\n");
      return -EOPNOTSUPP;
   }

   /* Allow changing the IRQ */
   if (map->irq != dev->irq)
   {
      dev->irq = map->irq;
      /* request_irq() is delayed to open-time */
   }

   /* ignore other fields */
   return 0;
}

/* This is called when PC Host sends the received packet to the Kernel. */
void nalla_250_soc_network_packet_rx(struct net_device *dev, uint8_t* buffer,
                                     int bufferLength)
{
   struct sk_buff *skb;
   nalla_250_soc_network_private_t* priv = netdev_priv(nalla_250_soc_net_device);
   struct iphdr *ih;
   struct ethhdr* ethhdr;
   u32 *saddr, *daddr;
   int i;
   unsigned char* kernelPacketBuffer;
   int netif_rx_retval = 0;

   struct net_device *destDev = nalla_250_soc_net_device;

   //printk("nalla_250_soc_network_packet_rx()\n");

   /* The PC Host has sent a packet to us via the PCIe BAR.  We need to allocate
      an skb and wrap it arround the packet, so that upper layers in the kernel
      can handle it. */
   skb = dev_alloc_skb(bufferLength + 2);

   if (skb == NULL)
   {
      if (printk_ratelimit())
         printk(KERN_NOTICE "nalla_250_soc_network rx: low on mem - packet dropped\n");

      priv->stats.rx_dropped++;
      goto out;
   }

   skb_reserve(skb, 2); /* align IP on 16B boundary */

   kernelPacketBuffer = skb_put(skb, bufferLength);

   /* Copy the packet data into our allocated and preperated skb. */
   //    memcpy(kernelPacketBuffer, buffer, bufferLength);
   for(i=0;i<bufferLength;i++)
     kernelPacketBuffer[i] = buffer[i];

   /*
    * Ethhdr is 14 bytes, but the kernel arranges for iphdr
    * to be aligned (i.e., ethhdr is unaligned)
    */
   ethhdr = (struct ethhdr*)kernelPacketBuffer;
   if(ethhdr->h_proto == ETH_P_IP)
     {
       ih = (struct iphdr *)(kernelPacketBuffer + sizeof(struct ethhdr));
       saddr = &ih->saddr;
       daddr = &ih->daddr;


       if (0)
	 printk("Passing packet to kernel %d.%d.%d.%d --> %d.%d.%d.%d\n",
		((u8*)saddr)[0], ((u8*)saddr)[1], ((u8*)saddr)[2], ((u8*)saddr)[3],
		((u8*)daddr)[0], ((u8*)daddr)[1], ((u8*)daddr)[2], ((u8*)daddr)[3]);

       ih->check = 0;         /* and rebuild the checksum (ip needs it) */
       ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);
     }
  /* Write metadata, and then pass to the receive level */
   skb->dev = destDev;
   skb->protocol = eth_type_trans(skb, destDev);
   skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
   priv->stats.rx_packets++;
   priv->stats.rx_bytes += bufferLength;

   netif_rx_retval = netif_rx(skb);
   if (netif_rx_retval != NET_RX_SUCCESS)
      if (printk_ratelimit())
      {
		  switch (netif_rx_retval)
		  {
			  case NET_RX_DROP:
			  printk("Warning: Ethernet over PCIe, packet dropped, 8f4f9be9-a95d-40b3-b718-6de26d0232dd\n");
			  break;

			  default:
			  printk("Warning: Ethernet over PCIe, unknown issue, 8f4f9be9-a95d-40b3-b718-6de26d0232dd\n");
			  break;
	      }
	  }

  out:
     return;
}

/*
 * Transmit a packet (called by the kernel)
 */
int nalla_250_soc_network_packet_tx(struct sk_buff *skb, struct net_device *dev)
{
   /* Get the private data structure associated with this network device.  We'll
      use this to track packet transmission/drop statistics. */
   nalla_250_soc_network_private_t *priv = netdev_priv(dev);

   int canSendPacket = canSendPacketToPcHost();

   if (!canSendPacket)
   {
      int difference = jiffies - priv->lastPacketTxTime;

      if (difference > NALLA_250_SOC_NETWORK_TRANSMIT_TIMEOUT_JIFFIES)
      {
         printk("Flushing SoC->PC packet buffer\n");

         /* Clear the transmit packet buffer. */
         flushSocToPcPacketBuffer();

         /* Mark that we can not transmit a buffer. */
         canSendPacket = 1;
      }
   }

   /* Verify that we can send a packet to the PC Host. */
   if (canSendPacket)
   {
      /* Send the packet to our PC Host via the PCIe BAR. */
      sendPacketToPcHost(skb->data, skb->len);

      /* Save the timestamp. */
      //      dev->trans_start = jiffies;
#ifdef _OLD_TRANS_START
         dev->trans_start = jiffies;
#else
	 netif_trans_update(dev);
#endif

      /* Packet sent to the PC Host, mark this in network interface stats. */
      priv->stats.tx_packets++;
      priv->stats.tx_bytes += skb->len;

      /* Note when we last sent a packet to PC host. */
      priv->lastPacketTxTime = jiffies;
   }
   else
   {
      /* if (printk_ratelimit()) */
      /*    printk("We couldn't send a buffer to PC Host, yet. TEMP: DROPPING IT\n"); */

      /* PC Host hasn't processed the last packet, so we just drop this packet.
         Mark this in network interface stats. */
      priv->stats.tx_dropped++;
   }

   /* Free the Socket Buffer.  This is the drivers responsibility. */
   dev_kfree_skb(skb);

   /* If we get to this point, transmission was successful. */
   return NETDEV_TX_OK;
}

////////////////////////////////////////////////////////////////////////////////

/*
 * Deal with a transmit timeout.
 */
void nalla_250_soc_network_tx_timeout(struct net_device *dev)
{
   //struct nalla_250_soc_network_priv *priv = netdev_priv(dev);

   PDEBUG("Transmit timeout at %ld, latency %ld\n", jiffies,
         jiffies - dev->trans_start);

   /* Simulate a transmission interrupt to get things moving */

   /* priv->status = NALLA_250_SOC_NETWORK_TX_INTR; */
   /* nalla_250_soc_network_interrupt(0, dev, NULL); */
   /* priv->stats.tx_errors++; */

   netif_wake_queue(dev);
   return;
}



/*
 * Ioctl commands
 */
int nalla_250_soc_network_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
   PDEBUG("ioctl\n");
   return 0;
}

/*
 * Return statistics to the caller
 */
struct net_device_stats *nalla_250_soc_network_stats(struct net_device *dev)
{
   nalla_250_soc_network_private_t* priv = netdev_priv(dev);


   return &priv->stats;
}


int nalla_250_soc_network_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
   struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);
  char mac[6];
  char hostmac[6];
  mac[0] = socMacRegs[0];
  mac[1]=socMacRegs[1];
  mac[2]=socMacRegs[2];
  mac[3]=socMacRegs[3];
  mac[4]=socMacRegs[4];
  mac[5]=socMacRegs[5];

   //printk("nalla_250_soc_network_header()\n");

   eth->h_proto = htons(type);

   /* Copy in the MAC addresses for SoC and PC Host. */
   //   memcpy(eth->h_source, socMacRegs, dev->addr_len);
   memcpy(eth->h_source, mac, dev->addr_len);
  hostmac[0] = pcHostMacRegs[0];
  hostmac[1]=pcHostMacRegs[1];
  hostmac[2]=pcHostMacRegs[2];
  hostmac[3]=pcHostMacRegs[3];
  hostmac[4]=pcHostMacRegs[4];
  hostmac[5]=pcHostMacRegs[5];
   memcpy(eth->h_dest,   hostmac, dev->addr_len);
   //   memcpy(eth->h_dest,   pcHostMacRegs, dev->addr_len);

   /* Make sure our network device recognises that we have the MAC instructed by
      the PC Host. */
   //   memcpy(dev->dev_addr, socMacRegs, dev->addr_len);
   memcpy(dev->dev_addr, mac, dev->addr_len);

   return (dev->hard_header_len);
}

/*
 * The "change_mtu" method is usually not needed.
 * If you need it, it must be like this.
 */
int nalla_250_soc_network_change_mtu(struct net_device *dev, int new_mtu)
{
   /* check ranges */
   if ((new_mtu < 68) || (new_mtu > 1500))
      return -EINVAL;

   /*
    * Do anything you need, and the accept the value
    */
   dev->mtu = new_mtu;

   return 0; /* success */
}

static const struct header_ops nalla_250_soc_network_header_ops = {
   .create  = nalla_250_soc_network_header,
};

static const struct net_device_ops nalla_250_soc_network_netdev_ops = {
   .ndo_open            = nalla_250_soc_network_open,
   .ndo_stop            = nalla_250_soc_network_release,
   .ndo_start_xmit      = nalla_250_soc_network_packet_tx,
   .ndo_do_ioctl        = nalla_250_soc_network_ioctl,
   .ndo_set_config      = nalla_250_soc_network_config,
   .ndo_get_stats       = nalla_250_soc_network_stats,
   .ndo_change_mtu      = nalla_250_soc_network_change_mtu,
   .ndo_tx_timeout      = nalla_250_soc_network_tx_timeout
};

/*
 * The init function (sometimes called probe).
 * It is invoked by register_netdev()
 */
void nalla_250_soc_network_netdev_init(struct net_device *dev)
{
   nalla_250_soc_network_private_t* priv;

   /*
    * Then, assign other fields in dev, using ether_setup() and some
    * hand assignments
    */
   ether_setup(dev); /* assign some of the fields */
   dev->watchdog_timeo = timeout;
   dev->netdev_ops = &nalla_250_soc_network_netdev_ops;
   dev->header_ops = &nalla_250_soc_network_header_ops;

   /* keep the default flags, just add NOARP */
   dev->flags           |= IFF_NOARP;
   dev->features        |= NETIF_F_HW_CSUM;

   /*
    * Then, initialize the priv field. This encloses the statistics
    * and a few private fields.
    */
   priv = netdev_priv(dev);

   memset(priv, 0, sizeof(nalla_250_soc_network_private_t));
}

/*
 * The devices
 */

//struct net_device *nalla_250_soc_net_device;



void nalla_250_soc_network_cleanup(void)
{
   if (nalla_250_soc_net_device)
   {
      unregister_netdev(nalla_250_soc_net_device);

      free_netdev(nalla_250_soc_net_device);
   }

   /* Unmap the PC Host MAC addresss registers area. */
   if (pcHostMacRegs)
      iounmap(pcHostMacRegs);

   /* Unmap the SoC MAC addresss registers area. */
   if (socMacRegs)
     {
       /* printk("unmapping socMacRegs\n"); */
       iounmap(socMacRegs);
     }
}




int nalla_250_soc_network_init(void* privateData)
{
   int result, ret = -ENOMEM;

   nalla_250_soc_network_private_t* priv;

   nalla_250_soc_net_device = alloc_netdev(sizeof(nalla_250_soc_network_private_t),
                                           "nalla%d",
                                           NET_NAME_UNKNOWN,
                                           nalla_250_soc_network_netdev_init);

    if (nalla_250_soc_net_device == NULL)
      goto out;

   ret = -ENODEV;

   if ((result = register_netdev(nalla_250_soc_net_device)))
      printk("nalla_250_soc_network: error %i registering device \"%s\"\n",
             result, nalla_250_soc_net_device->name);
   else
      ret = 0;

   out:
   if (ret)
     {
       /* printk("network init cleanup\n"); */

      nalla_250_soc_network_cleanup();
     }


   priv = (nalla_250_soc_network_private_t*) netdev_priv(nalla_250_soc_net_device);
   priv->privateData = privateData;

   priv->lastPacketTxTime = jiffies;

   /* Map PC host MAC address memory. */
   pcHostMacRegs = ioremap_nocache(PC_HOST_MAC_ADDRESS,
                                   PC_HOST_MAC_LENGTH_BYTES);

   /* Map SoC MAC address memory. */
   socMacRegs = ioremap_nocache(SOC_MAC_ADDRESS, SOC_MAC_LENGTH_BYTES);


   return ret;
}


/* EOF - nalla_250_soc_ep_network.c */
