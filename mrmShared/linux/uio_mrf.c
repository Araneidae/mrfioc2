/* Copyright (C) 2010 Brookhaven National Lab
 * All rights reserved.
 * See file LICENSE for terms.
 */

#include <linux/version.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#define DRV_NAME "mrf-pci"
#define DRV_VERSION "0"

MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Michael Davidsaver <mdavidsaver@bnl.gov>");

/************************ Register definitions ****************************/

/*
 * A selection of registers for the PLX PCI9030
 *
 * This device is exposed as BAR #0 on PCI and PMC
 * versions of the EVR
 */

/* Address space #0 is exposed as BAR #2 */
#define LAS0BRD  0x28
/* Set for big endian, clear for little endian (swapped) */
#  define LAS0BRD_ENDIAN 0x01000000

/* Interrupt control */
#define INTCSR   0x4c
#  define INTCSR_INT1_Enable   0x01
#  define INTCSR_INT1_Polarity 0x02
#  define INTCSR_INT1_Status   0x04
#  define INTCSR_INT2_Enable   0x08
#  define INTCSR_INT2_Polarity 0x10
#  define INTCSR_INT2_Status   0x20
#  define INTCSR_PCI_Enable    0x40
#  define INTCSR_SW_INTR       0x80

/* For MRM EVR 230 series
 */

#define IRQFlag     0x008
#  define IRQ_LinkChg   0x40
#  define IRQ_BufFull   0x20
#  define IRQ_HWMapped  0x10
#  define IRQ_Event     0x08
#  define IRQ_Heartbeat 0x04
#  define IRQ_FIFOFull  0x02
#  define IRQ_RXErr     0x01

#define IRQEnable   0x00c
/* Same bits as IRQFlag plus */
#  define IRQ_Enable    0x80000000

/************************ Compatability ****************************/


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28)
static
void __iomem *pci_ioremap_bar(struct pci_dev* pdev,int bar)
{
        if(!(pci_resource_flags(pdev,bar) & IORESOURCE_MEM )){
                WARN_ON(1);
                return NULL;
        }
        return ioremap_nocache(pci_resource_start(pdev,bar),
                pci_resource_len(pdev,bar));
}
#endif

/**************************** PCI Driver *************************/

static
irqreturn_t
mrf_handler(int irq, struct uio_info *info)
{
    void __iomem *base = info->mem[2].internal_addr;
    void __iomem *plx = info->mem[0].internal_addr;
    u32 plxcsr= ioread32(plx + INTCSR);

    if (!(plxcsr&(INTCSR_INT1_Status|INTCSR_INT2_Status)))
        return IRQ_NONE;

    if (plxcsr&INTCSR_INT1_Status)
        plxcsr&=~INTCSR_INT1_Enable;

    if (plxcsr&INTCSR_INT2_Status)
        plxcsr&=~INTCSR_INT2_Enable;

    /* Enable active high interrupt1 through the PLX to the PCI bus. */
    iowrite16(plxcsr, plx + INTCSR);

    return IRQ_HANDLED;
}

static
int __devinit
mrf_probe(struct pci_dev *dev,
          const struct pci_device_id *id)
{
        int ret = -ENODEV;
        struct uio_info *info;

        info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
        if (!info) return -ENOMEM;

        ret = pci_enable_device(dev);
        if (ret) {
                dev_err(&dev->dev, "pci_enable_device failed with %d\n",ret);
                goto err_free;
        }
        if (!dev->irq) {
                dev_warn(&dev->dev, "Device not configured with IRQ!\n");
                ret=-ENODEV;
                goto err_disable;
        }

        if (pci_request_regions(dev, DRV_NAME))
                goto err_disable;

        /* BAR 0 is the PLX bridge */
        info->mem[0].addr = pci_resource_start(dev, 0);
        info->mem[0].size = pci_resource_len(dev,0);
        info->mem[0].internal_addr =pci_ioremap_bar(dev,0);
        info->mem[0].memtype = UIO_MEM_PHYS;

        /* Not used */
        info->mem[1].memtype = UIO_MEM_NONE;
        info->mem[1].size = 1; /* Otherwise UIO will stop searching... */

        /* BAR 2 is the EVR */
        info->mem[2].addr = pci_resource_start(dev, 2);
        info->mem[2].size = pci_resource_len(dev,2);
        info->mem[2].internal_addr =pci_ioremap_bar(dev,2);
        info->mem[2].memtype = UIO_MEM_PHYS;

        if (!info->mem[0].internal_addr ||
            !info->mem[0].addr ||
            !info->mem[2].internal_addr ||
            !info->mem[2].addr) {
                dev_err(&dev->dev, "Failed to map BARS!\n");
                ret=-ENODEV;
                goto err_release;
        }

        info->irq = dev->irq;
        info->irq_flags = IRQF_SHARED;
        info->handler = mrf_handler;

        info->name = DRV_NAME;
        info->version = DRV_VERSION;

        pci_set_drvdata(dev, info);

        ret = uio_register_device(&dev->dev, info);
        if (ret)
                goto err_unmap;

        return 0;

err_unmap:
        iounmap(info->mem[0].internal_addr);
        iounmap(info->mem[2].internal_addr);
err_release:
        pci_release_regions(dev);
err_disable:
        pci_disable_device(dev);
err_free:
        kfree(info);
        return ret;
}

#define PCI_SUBVENDOR_ID_MRF             0x1a3e

#define PCI_SUBDEVICE_ID_MRF_PMCEVR_230   0x11e6
#define PCI_SUBDEVICE_ID_MRF_PXIEVR_230   0x10e6

static struct pci_device_id mrf_pci_ids[] __devinitdata = {
    {
        .vendor =       PCI_VENDOR_ID_PLX,
        .device =       PCI_DEVICE_ID_PLX_9030,
        .subvendor =    PCI_SUBVENDOR_ID_MRF,
        .subdevice =    PCI_SUBDEVICE_ID_MRF_PXIEVR_230,
    },
    {
        .vendor =       PCI_VENDOR_ID_PLX,
        .device =       PCI_DEVICE_ID_PLX_9030,
        .subvendor =    PCI_SUBVENDOR_ID_MRF,
        .subdevice =    PCI_SUBDEVICE_ID_MRF_PMCEVR_230,
    },
    { 0, }
};

/************************** Module boilerplate ****************************/

static
void __devexit
mrf_remove(struct pci_dev *dev)
{
        struct uio_info *info = pci_get_drvdata(dev);

        uio_unregister_device(info);
        platform_set_drvdata(dev, NULL);
        iounmap(info->mem[0].internal_addr);
        iounmap(info->mem[2].internal_addr);
        pci_release_regions(dev);
        pci_disable_device(dev);

        kfree(info);
}


static struct pci_driver mrf_driver = {
    .name    =DRV_NAME,
    .id_table=mrf_pci_ids,
    .probe   = mrf_probe,
    .remove  = __devexit_p(mrf_remove),
};

static int __init mrf_init_module(void)
{
        return pci_register_driver(&mrf_driver);
}
module_init(mrf_init_module);

static void __exit mrf_exit_module(void)
{
        pci_unregister_driver(&mrf_driver);
}
module_exit(mrf_exit_module);
