

/*****************************************************************************/
/* Driver identification and API.                                             */
/*****************************************************************************/
static const struct pci_device_id foo_ids[] = {
   {
      .class = 0x000000,
      .class_mask = 0x000000,
      .vendor = 0xdead,
      .device = 0xbeef,
      .subvendor = PCI_ANY_ID,
      .subdevice = PCI_ANY_ID,
      .driver_data = 0 /*Arbitrary data*/

   },
   {/*End: all zeros*/}
};

MODULE_DEVICE_TABLE(pci, ids);

static struct pci_driver foo_driver = {
   .name = "foo",
   .id_table = foo_ids,
   .probe = foo_probe,
   .remove = __devexit_p(foo_remove),
};

/*****************************************************************************/
/* Drivers's interrupt handler.                                              */
/*****************************************************************************/
static irqreturn_t foo_msi(int irq, void *data)
{
   struct foo_privdata *priv = data; /*Private data*/

   priv->flag = 1;
   wake_up_interruptable(&priv->waitq);
   return IRQ_HANDLED;
}

/*****************************************************************************/
/* Drivers's probe function.                                                 */
/*****************************************************************************/
static int __devinit foo_probe(struct pci_dev *dev,
                               const struct pci_device_id *ent)
{
   struct foo_privdata *priv; /*Driver's private data*/
   int rc = 0; /*Return code*/
   
   if ((priv = kzalloc(sizeof(*priv), GFP_KERNEL)) == NULL) {
      return -ENOMEM;
   }
   pci_set_drvdata(dev, priv);

   if ((rc = pci_enable_device(dev)) != 0) {
      rc = -ENODEV;
      goto foo_probe_exit;
   }
   ...

   /*Setup a single MSI interrupt.*/
   if (pci_enable_msi(dev)) {
      rc = -ENODEV;
      goto foo_probe_exit;
   }
   init_waitqueue_head(&priv->waitq);
   if ((rc = request_irq(dev->irq, foo_msi, 0, "foo", priv)) != 0) {
      rc = -ENODEV;
      goto foo_probe_exit;
   }

   /*Error handler and exit.*/
foo_probe_exit:
   return rc;
}


/*****************************************************************************/
/* Driver's front-end functions.                                              */
/*****************************************************************************/
static int __init foo_init(void)
{
   return pci_register_driver(&foo_driver);
}

static int __exit foo_exit(void)
{
   return pci_unregister_driver(&foo_driver);
}

module_init(foo_init);
module_exit(foo_exit);

