/*                                                     

 */                                                    
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <asm/uaccess.h>

#include <asm/io.h>


#include "nalla_250_soc_end_point.h"
#include "nalla_250_soc_ep_network.h"
#include "nalla_250_soc_device.h"


MODULE_LICENSE("GPL");


#define DEVICE_FS_ENABLED  (1)

//static spinlock_t loopbackBufferLock;

static void* irqGeneratorRegs     = NULL;
static void* pcieIrqGeneratorRegs = NULL;static void* dataToPcHostBuffer   = NULL;
static void* dataFromPcHostBuffer = NULL;
static wait_queue_head_t fpga_int_queue;     
static spinlock_t nalla_s_lock;
static uint32_t int_reason = 0;

void nalla_250_soc_receiveBufferDoTaskletNetwork(unsigned long unused);

DECLARE_TASKLET (nalla_250_soc_receiveBufferTasklet, nalla_250_soc_receiveBufferDoTaskletNetwork, 0);

int NetworkEnable=0;
module_param(NetworkEnable, int, S_IRUSR|S_IWUSR);

static uint8_t* ingressPacketBuffer = NULL;

int nalla250_SocIoctl(struct file *file, unsigned int cmd, unsigned long inArg,uint32_t mode)
{
   unsigned int *pArg = (unsigned int *) inArg;
   unsigned int int_gen;
   IrqGeneratorRegisters_t* pcieIrqGenerator =  (IrqGeneratorRegisters_t*) pcieIrqGeneratorRegs;

   switch(cmd)
     {
     case NALLA_250_SOC_IOCTL_INTERRUPT_WAIT:
       nalla250_Soc_ioctl_int_wait((unsigned long) pArg, NALLA_250_SOC_IRQ_VALUE_RING_BUFFER_INTERRUPT);
       break;
     case NALLA_250_SOC_IOCTL_INTERRUPT_GEN:
       if (!access_ok_handler (VERIFY_READ, (void __user *) pArg, sizeof (unsigned int)))
	 {
	   return -EFAULT;
	 }
       __get_user (int_gen, (unsigned int *) pArg);
       iowrite32(int_gen, &pcieIrqGenerator->initiate);
       //&irqGenerator->reason
       break;
     default:
       printk("NALLA250_SOC: Unrecognised ioctl cmd (%d)\n", cmd);
       break;
     }
  return 0;
}

int nalla250_Soc_ioctl_int_wait(unsigned long pArg, uint32_t int_mask)
{

  int retVal = 0;
  struct nalla_250_soc_int_wait int_wait;
  unsigned long delay, flags;
  u32 timeout = 0;
  u32 usrInt, endloop = 0;
  DEFINE_WAIT (wait);


  if (copy_from_user (&int_wait, (void *) pArg, sizeof (struct nalla_250_soc_int_wait)) != 0)
    {
      retVal = -EFAULT;
      return retVal;
    }
  
  if (int_wait.timeout != 0)
    delay = (int_wait.timeout * HZ) / 1000;
  else
    delay = MAX_SCHEDULE_TIMEOUT;
  
  usrInt = int_wait.interrupt & int_mask;

  if (!usrInt)
    return -EFAULT;
  
  do
    {
      prepare_to_wait (&fpga_int_queue, &wait, TASK_INTERRUPTIBLE);
      
      spin_lock_irqsave (&nalla_s_lock, flags);
       if (int_reason & usrInt)
        {
          spin_unlock_irqrestore (&nalla_s_lock, flags);
          timeout = 1;
        }
      else
        {
          spin_unlock_irqrestore (&nalla_s_lock, flags);
          timeout = schedule_timeout (delay);
        }

      finish_wait (&fpga_int_queue, &wait);
      
      if (timeout == 0)
        {
          /* no interrupt - timed out */
          int_wait.interrupt = 0;
          endloop = 1;
        }
      else
        {
          /* clear the interrupt bits and return */
		spin_lock_irqsave (&nalla_s_lock, flags);
          if (int_reason & usrInt)
            {
				int_wait.interrupt = int_reason & usrInt;
				int_reason &= ~int_wait.interrupt;
				endloop = 1;
            }
          else
            {
              if (int_wait.timeout != 0)
                delay = timeout;
            }
          spin_unlock_irqrestore (&nalla_s_lock, flags);
        }
      
    }
  while (endloop == 0);
  if (copy_to_user ((void __user *) pArg, (void *) &int_wait, sizeof (struct nalla_250_soc_int_wait)) != 0)
    {
      retVal = -EFAULT;
    }
  
  return retVal;

}



void nalla_250_soc_receiveBufferDoTaskletNetwork(unsigned long unused)
{
   int i;

   uint32_t bytesReceived;

   //printk("receiveBufferDoTaskletNetwork():\n");

   /* The first 32 bits of buffer from PC Host are a message length in bytes. */
   bytesReceived = ioread32(dataFromPcHostBuffer);
 
   //   printk("bytesReceived = 0x%x\n", bytesReceived);

   if (bytesReceived == 0)
      return;

   /* Copy bytes from PC Host into  buffer.*/
   for (i = 0; i < bytesReceived; i++)
   {
      /* Copy data into buffer. */
      ingressPacketBuffer[i] = ioread8(dataFromPcHostBuffer + 4 + i);
      //      printk("ingressPacketBuffer[%d] = 0x%x\n", i,ingressPacketBuffer[i]);
   }

   /* Send this packet to Linux Kernel now that we've copied it out of the PCIe
      BAR. */
   nalla_250_soc_network_packet_rx(NULL, ingressPacketBuffer, bytesReceived);

   /* Now we've copied data from shared memory, reset the byte count, so that PC
      Host knows it can write in there again. */
   iowrite32(0x0, dataFromPcHostBuffer);

   //printk("ReceiveTasklet() %d bytes --> kernel\n", bytesReceived);
}

static int ourIrqNumber;

irqreturn_t nalla_250_soc_ep_isr(int this_irq, void *dev_id)
{
  IrqGeneratorRegisters_t* irqGenerator = (IrqGeneratorRegisters_t*) irqGeneratorRegs;

   /* Read the IRQ reason register, to see if we should react to this interrupt. */
  uint32_t reason = ioread32(&irqGenerator->reason);

  //  printk("interrupt, data received\n");

   /* PC Host has sent a buffer to us.  Have the tasklet process it. */
  if ((reason & IRQ_PC_HOST_SENT_BUFFER_TO_SOC) && (NetworkEnable))
    tasklet_schedule(&nalla_250_soc_receiveBufferTasklet);

   /* Clear the IRQ initiate register, and write to reason to indicate the IRQ
      has been handed. */
  iowrite32(reason, &irqGenerator->reason);

	return IRQ_HANDLED;
}


/* 
 *
 */
static int nalla_250_soc_ep_init(void)
{
   void *regs;
   int result;
	//int req;
	int irq_number;
   IrqGeneratorRegisters_t* irqGenerator;
   /* IrqGeneratorRegisters_t* pcieIrqGenerator; */

   struct device_node *np = NULL;

   //printk("Driver built on " __DATE__ " " __TIME__ "\n");

   np = of_find_compatible_node(NULL, NULL, "nalla,250_soc");

   irq_number = irq_of_parse_and_map(np, 0);

   spin_lock_init (&nalla_s_lock);

   /* TODO: Commennt. */
   irqGeneratorRegs = ioremap_nocache(IRQ_GENERATOR_BASE_ADDRESS,
                                      IRQ_GENERATOR_LENGTH_BYTES);

   irqGenerator = (IrqGeneratorRegisters_t*) irqGeneratorRegs;
  
   /* Enable the first two IRQs. */
   iowrite32(IRQ_GENERATOR_MASK, &irqGenerator->mask);

   /* Clear any pending IRQs from before driver was loaded. */
   iowrite32(0xF, &irqGenerator->reason);

   /* TODO: Commennt. */
   pcieIrqGeneratorRegs = irqGenerator;

   /* pcieIrqGenerator = (IrqGeneratorRegisters_t*) pcieIrqGeneratorRegs; */

   init_waitqueue_head (&fpga_int_queue);
   /* Enable the first IRQ. */
   /* iowrite32(PCIE_IRQ_GENERATOR_MASK, &pcieIrqGenerator->mask); */

   /* /\* Clear any pending IRQs from before driver was loaded. *\/ */
   /* iowrite32(0xF, &pcieIrqGenerator->reason); */


   dataFromPcHostBuffer = ioremap_nocache(DATA_FROM_PC_HOST_BUFFER_ADDRESS,
                                          DATA_FROM_PC_HOST_BUFFER_LENGTH_BYTES);

   /* Zero the received byte count. */
   iowrite32(0x0, dataFromPcHostBuffer);

   dataToPcHostBuffer = ioremap_nocache(DATA_TO_PC_HOST_BUFFER_ADDRESS,
                                        DATA_TO_PC_HOST_BUFFER_LENGTH_BYTES);

   /* Zero the sent byte count. */
   iowrite32(0x0, dataToPcHostBuffer);

   ////////////////////////////////////////////////////////////////////////////////

   /* printk("Allocating %d bytes for ingressPacketBuffer.\n", */
   /*        INGRESS_PACKET_BUFFER_LENGTH_BYTES); */

   ingressPacketBuffer = kmalloc(INGRESS_PACKET_BUFFER_LENGTH_BYTES, GFP_KERNEL);

   if (ingressPacketBuffer == NULL)
   {
      printk("Couldn't allocate memory (%d bytes) for ingressPacketBuffer!\n",
             INGRESS_PACKET_BUFFER_LENGTH_BYTES);

      return -1;
   }

   if (NetworkEnable)
   {
      int networkResult = nalla_250_soc_network_init(NULL);

      printk("nalla_250_soc_network_init(NULL) = %d = 0x%x\n", networkResult, networkResult);
   }

   if (DEVICE_FS_ENABLED)
   {
      int deviceResult = nalla_250_soc_device_init(NULL);

      printk("nalla_250_soc_device_init(NULL) = %d\n", deviceResult);
   }

   if (irq_number == 0)
   {
		pr_err("irq_number = %i\n", irq_number);
		return -1;
   }


   /* Now we've initialised everything else, hook up our IRQ service routine. */
   result = request_irq(irq_number, nalla_250_soc_ep_isr, 0, 0, 0);

	if (result != 0)
   {
		pr_err("Failure requesting irq %i\n", irq_number);
		return result;
	}

   /* Store IRQ number in global variable. */
   ourIrqNumber = irq_number;

   /* Indicate for the PC Host that the SOC driver is up and ready for
      packets. */
   regs = ioremap(SOC_DRIVER_UP_ADDRESS, 4);  
   iowrite32(DRIVER_UP_SIGIL, regs);
   iounmap(regs);

   return 0;
}

static void nalla_250_soc_ep_exit(void)
{
   void* regs; // TODO: Rename

   printk(KERN_ALERT "nalla_250_soc_ep_exit()\n");

   //printk("Freeing IRQ %d...\n", ourIrqNumber);

	free_irq(ourIrqNumber, 0);

   /* Unmap the IRQ generator registers area. */
   if (irqGeneratorRegs)
      iounmap(irqGeneratorRegs);

   /* Free the ingress packet buffer. */
   if (ingressPacketBuffer)
      kfree(ingressPacketBuffer);

   /* Unmap the shared buffers for data to/from PC Host. */
   if (dataFromPcHostBuffer)
      iounmap(dataFromPcHostBuffer);

   if (dataToPcHostBuffer)
      iounmap(dataToPcHostBuffer);

   /* Indicate to the PC Host that the SoC driver is down. */
   regs = ioremap(SOC_DRIVER_UP_ADDRESS, 4);  
   iowrite32(0, regs);
   iounmap(regs);

   /* If network was enabled, shut it down. */
   if (NetworkEnable)
      nalla_250_soc_network_cleanup();

   /* If device_fs code was enabled, shut it down. */
   if (DEVICE_FS_ENABLED)
      nalla_250_soc_device_cleanup();
}


int canSendPacketToPcHost()
{
   uint32_t bytesDataToPcHostBuffer = ioread32(dataToPcHostBuffer);

   if (bytesDataToPcHostBuffer != 0)
      return 0;
   else
      return 1;
}

void sendPacketToPcHost(uint8_t* buffer, int lengthBytes)
{
   int i;

   IrqGeneratorRegisters_t* pcieIrqGenerator =  (IrqGeneratorRegisters_t*) pcieIrqGeneratorRegs;
   //   int initiate;
   uint32_t bytesDataToPcHostBuffer;

   bytesDataToPcHostBuffer = ioread32(dataToPcHostBuffer);

   if (bytesDataToPcHostBuffer != 0)
   {
      printk("PC Host not ready for more data yet.\n");
      //spin_unlock(&loopbackBufferLock);
      return;
   }

   for (i = 0; i < lengthBytes; i++)
   {
      // DO temp loopback
      iowrite8(buffer[i], dataToPcHostBuffer + 4 + i);
   }

   /* Tell PC host how many bytes we've sent. */
   iowrite32(lengthBytes, dataToPcHostBuffer);

   /* Read back from PCI BAR how many bytes we've sent, to make sure the value
      has been stored before we trigger the interrupt. */
   bytesDataToPcHostBuffer = ioread32(dataToPcHostBuffer);

   //   printk("sendBufferToPcHost() --> %d bytes --> pchost\n", lengthBytes);

   /* Trigger an interrupt, we have data for the PC Host to handle. */
   iowrite32(PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST, &pcieIrqGenerator->initiate);
   /* initiate = ioread32(&pcieIrqGenerator->initiate); */
   /* if(initiate & PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST) */
   /*     iowrite32(initiate & ~PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST,&pcieIrqGenerator->initiate); */
   /* else */
   /*     iowrite32(initiate | PCIE_IRQ_SOC_SENT_BUFFER_TO_PC_HOST,&pcieIrqGenerator->initiate); */
 
}


void flushSocToPcPacketBuffer()
{
   /* IrqGeneratorRegisters_t* pcieIrqGenerator = (IrqGeneratorRegisters_t*) pcieIrqGeneratorRegs; */

   /* Mark the buffer for data to PC host as empty. */
   iowrite32(0, dataToPcHostBuffer);

   /* Clear any unhandled interrupts we may have sent to the PC Host. */
   //   iowrite32(0xf, &pcieIrqGenerator->reason);
}



int pcHostDriverUp()
{
   void* regs = ioremap(PC_HOST_DRIVER_UP_ADDRESS, 4);
   
   uint32_t value = readl(regs);

   iounmap(regs);

   return value == DRIVER_UP_SIGIL;
}


module_init(nalla_250_soc_ep_init);
module_exit(nalla_250_soc_ep_exit);


/* EOF - nalla_250_soc_end_point.c */
