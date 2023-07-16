

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/circ_buf.h>
#include <linux/cdev.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <asm/uaccess.h>

#include <asm/io.h>


#include "nalla_250_soc_end_point.h"


#define DEVICE_NAME "nalla_250_soc"


struct cdev nalla250_SocCdev;

static struct class *nalla250_SocSysfsClass;

int pcie250_SocMajor = 0;
int pcie250_SocMinor = 0;

void* pcieBarAddress;


struct device *moduleDev; /*!< \brief Nalla 385A-SoC module device structure */


/* Function prototypes. */
static int nalla250_SocOpen(struct inode *inode, struct file *file);
static int nalla250_SocRelease(struct inode *inode, struct file *file);
static long nalla250_SocIoctlNative(struct file *file, unsigned int cmd,
                                    unsigned long pArg);
static long nalla250_SocIoctlCompat(struct file *file, unsigned int cmd,
                                    unsigned long pArg);
static ssize_t nalla250_SocRead(struct file *file, char *buf, size_t count,
                                loff_t *ppos);
static ssize_t nalla250_SocWrite(struct file *file, const char *buf,
                                 size_t count, loff_t *ppos);
loff_t nalla250_SocLlseek(struct file *filp, loff_t off, int whence);





/*! \Brief device driver methods*/
static struct file_operations nalla250_SocFops =
{
   .owner          = THIS_MODULE,
   .read           = nalla250_SocRead,
   .write          = nalla250_SocWrite,
   .llseek         = nalla250_SocLlseek,
   .unlocked_ioctl = nalla250_SocIoctlNative,
   .compat_ioctl   = nalla250_SocIoctlCompat,
   /* .mmap           = nalla_pcie287_mmap, */
   .open           = nalla250_SocOpen,
   .release        = nalla250_SocRelease,
};

static int nalla250_SocOpen(struct inode *inode, struct file *file)
{
  //   printk("NALLA250_SOC: --> nalla250_SocOpen()\n");

  //   printk("NALLA250_SOC: minor node detected as: %d\n", iminor(inode));

  //   printk("NALLA250_SOC: <-- nalla250_SocOpen()\n");

   return 0;
}

static int nalla250_SocRelease(struct inode *inode, struct file *file)
{
  //   printk("NALLA250_SOC: --> nalla250_SocRelease()\n");

  //   printk("NALLA250_SOC: minor node detected as: %d\n", iminor(inode));

  //   printk("NALLA250_SOC: <-- nalla250_SocRelease()\n");

   return 0;
}


static long nalla250_SocIoctlNative(struct file *file, unsigned int cmd,
                                    unsigned long pArg)
{
   return nalla250_SocIoctl(file, cmd, pArg, 0);
}

static long nalla250_SocIoctlCompat(struct file *file, unsigned int cmd,
                                    unsigned long pArg)
{
   return nalla250_SocIoctl(file, cmd, pArg, 1);
}

static ssize_t nalla250_SocRead(struct file *file, char *buf, size_t count,
                                loff_t *ppos)
{
   //return nalla250_SocPrivateRead(file, buf, count, ppos);
   void *regs;
   void *pcieBarPosition;
   int result;
   int bytesLeft;

   /* if (*ppos & 0x40000) */
   /*    regs = ioremap((0xC0000000 - 0x40000) + *ppos, count); */
   /* else */
      regs = ioremap(PCIE_BAR_BASE_ADDRESS + *ppos, count);

   pcieBarPosition = regs;

   bytesLeft = count;
   while (bytesLeft)
   {
		if (bytesLeft >= 4)
		{
			uint32_t value = ioread32(pcieBarPosition);

			result = copy_to_user(buf, &value, 4);

			if (result)
				return -EINVAL;

			buf += 4;
			pcieBarPosition += 4;
			bytesLeft -= 4;
		}
		else
		{
			int result;
			uint8_t value = ioread8(pcieBarPosition++);

      		result = copy_to_user(buf++, &value, 1);

			if (result)
				return -EINVAL;
			bytesLeft--;
		}

   }

   iounmap(regs);


   *ppos += count;

   return count;
}

static ssize_t nalla250_SocWrite(struct file *file, const char *buf,
                                 size_t count, loff_t *ppos)
{
   void *regs;
   void *pcieBarPosition;
   int bytesLeft;

   /* if (*ppos & 0x40000) */
   /*    regs = ioremap((0xC0000000 - 0x40000) + (int) *ppos, count); */
   /* else */
      regs = ioremap(PCIE_BAR_BASE_ADDRESS + (int) *ppos, count);

   pcieBarPosition = regs;

   bytesLeft = count;

   while (bytesLeft)
   {
      if (bytesLeft >= 4)
      {
         uint32_t value;

         if (copy_from_user(&value, buf, 4))
            return -EINVAL;

         buf += 4;

         iowrite32(value, pcieBarPosition);

         pcieBarPosition += 4;

         bytesLeft -= 4;
      }
      else
      {
         uint8_t value;

         if (copy_from_user(&value, buf++, 1))
            return -EINVAL;

         iowrite8(value, pcieBarPosition++);

         bytesLeft--;
      }
   }

   iounmap(regs);


   *ppos += count;

   //printk("nalla250_SocRead(): DONE\n");

   return count;
}

loff_t nalla250_SocLlseek(struct file *filp, loff_t off, int whence)
{
   loff_t newpos;

   //printk("In nalla250_SocLlseek function offset is 0x%lx, sizeof lofft_t is %d bytes\n",(unsigned long)off, (unsigned int)sizeof(loff_t));

   switch (whence)
   {
      case 0: /* SEEK_SET */
         newpos = off;
         break;
      case 1: /* SEEK_CUR */
         newpos = filp->f_pos + off;
         break;
      case 2: /* SEEK_END - unsupported*/
         return -EINVAL;
         break;
      default: /* can't happen */
         return -EINVAL;
   }
   if (newpos < 0)
      return -EINVAL;
   filp->f_pos = newpos;
   return newpos;
}


int nalla_250_soc_device_init(void* privateData)
{
   int result;

   dev_t dev = 0;

   printk("nalla_250_soc_device_init():\n");

   //#define PCIE_BAR_BASE_ADDRESS  (0xFF200000)

   result = alloc_chrdev_region(&dev, 0, 1, "nalla_250_soc");

   pcie250_SocMajor = MAJOR(dev);
   pcie250_SocMinor = MINOR(dev);

   printk("NALLA250_SOC: Major number registered = %d\n", MAJOR(dev));
   printk("NALLA250_SOC: Minor number registered = %d\n", MINOR(dev));

   cdev_init(&nalla250_SocCdev, &nalla250_SocFops);

   nalla250_SocCdev.owner = THIS_MODULE;
   nalla250_SocCdev.ops   = &nalla250_SocFops;

   result = cdev_add(&nalla250_SocCdev, dev, 1);

   nalla250_SocSysfsClass = class_create(THIS_MODULE, DEVICE_NAME);

   device_create(nalla250_SocSysfsClass,
                 NULL,
                 MKDEV(MAJOR(dev), 0), // TODO: Fix this
                 NULL,
                 "pcie250_soc%d",
                 0);


   /* printk("NALLA385SAOC: alloc_chrdev_region() = 0x%x\n", result); */



   return 0;
}

void nalla_250_soc_device_cleanup(void)
{
   dev_t dev = MKDEV(pcie250_SocMajor, pcie250_SocMinor);

   printk("nalla_250_soc_device_cleanup():\n");

   cdev_del(&nalla250_SocCdev);

   unregister_chrdev_region(dev, 1);

   device_destroy(nalla250_SocSysfsClass, MKDEV(MAJOR(dev), 0));

   class_destroy(nalla250_SocSysfsClass);
}


/* EOF nalla_250_soc_device.c */
