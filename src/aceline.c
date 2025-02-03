#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/hid.h>

#define MIN(a,b) (((a) <= (b)) ? (a) : (b))

#define DRIVER_NAME "aceline_mouse"
#define MINOR_BASE  111
#define VENDOR_ID   0x1a2c
#define PRODUCT_ID  0x0044
#define INT_MS      10
#define DATA_SIZE_BYTES 4

struct usb_aceline {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct urb *intf_in_urb;
    unsigned char *intf_in_buffer;
    unsigned char *file_buffer;
    size_t intf_in_size;
    size_t time;
    __u8 intf_in_endpoint_addr;
    short connected;
};

static struct usb_aceline device;

static void save_data(unsigned char *buffer)
{
    device.file_buffer[0] = buffer[0];
    device.file_buffer[1] = buffer[1];
    device.file_buffer[2] = buffer[2];
    device.file_buffer[3] = buffer[3];
}

static void aread_intf_callback(struct urb *urb)
{
    int res;

    if (!device.connected)
        return;

    res = usb_submit_urb(device.intf_in_urb, GFP_KERNEL);

    if (res < 0) {
        printk(KERN_INFO "+ ERROR: usb_submit_urb error\n");
        return;
    }

    save_data(device.intf_in_buffer);
}

static ssize_t aread(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
    printk(KERN_INFO "+ INFO : device call aread\n");
    int ret;
    unsigned char *devbuf;
    devbuf = device.file_buffer;
    ret = copy_to_user(buffer, devbuf, DATA_SIZE_BYTES);
    printk(KERN_INFO "+ INFO : device aread success\n");
    return DATA_SIZE_BYTES;
}

static int aopen(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "+ INFO : device open\n");
    return 0;
}

static struct file_operations afops = {
    .read = aread,
    .open = aopen,
};

static struct usb_class_driver adriver_class = {
    .name = DRIVER_NAME,
    .fops = &afops,
    .minor_base = MINOR_BASE,
};

static int mouse_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    printk(KERN_INFO "+ INFO : mouse_probe\n");
    int res, ret;

    if (!device.connected) {
        struct usb_endpoint_descriptor *intf_in;
        device.udev = usb_get_dev(interface_to_usbdev(interface));
        device.interface = usb_get_intf(interface);

        res = usb_find_common_endpoints(interface->cur_altsetting, NULL, NULL, &intf_in, NULL);
        if (res < 0) {
            printk(KERN_INFO "+ ERROR: no endpoints\n");
            return -1;
        }
        device.intf_in_size = usb_endpoint_maxp(intf_in);
        device.intf_in_endpoint_addr = intf_in->bEndpointAddress;

        // printk(KERN_INFO "+ INFO : Interrupt In Endpoint:\n");
        // printk(KERN_INFO "+ INFO :   Endpoint Address: %02x\n", intf_in->bEndpointAddress);
        // printk(KERN_INFO "+ INFO :   Max Packet Size: %d\n", usb_endpoint_maxp(intf_in));
        // printk(KERN_INFO "+ INFO :   Endpoint Type: %d\n", usb_endpoint_type(intf_in));
        // printk(KERN_INFO "+ INFO :   Interval: %d\n", intf_in->bInterval);

        device.intf_in_buffer = kmalloc(device.intf_in_size, GFP_KERNEL);
        if (device.intf_in_buffer == NULL) {
            printk(KERN_INFO "+ ERROR: kmalloc intf_in_buffer\n");
            return -1;
        }

        device.file_buffer = kmalloc(DATA_SIZE_BYTES, GFP_KERNEL);
        if (device.file_buffer == NULL) {
            printk(KERN_INFO "+ ERROR: kmalloc file_buffer\n");
            return -1;

        }

        device.intf_in_urb = usb_alloc_urb(0, GFP_KERNEL);
        if (device.intf_in_urb == NULL) {
            printk(KERN_INFO "+ ERROR: usb_alloc_urb\n");
            return -1;
        }

        device.time = INT_MS;

        ret = 0;
        printk(KERN_INFO "+ INFO : device connect success\n");
        printk(KERN_INFO "+ INFO : intf_in_endpoint_addr = %d\n", device.intf_in_endpoint_addr);

        ret = usb_register_dev(interface, &adriver_class);
        if (ret != 0) {
            printk(KERN_INFO "+ ERROR: usb_register_dev error\n");
            return -1;
        }

        printk(KERN_INFO "+ INFO : device driver register success\n");

        usb_fill_int_urb(
            device.intf_in_urb,
            device.udev,
            usb_rcvintpipe(device.udev, device.intf_in_endpoint_addr),
            device.intf_in_buffer,
            device.intf_in_size,
            aread_intf_callback,
            NULL,
            device.time
        );

        printk(KERN_INFO "+ INFO : usb_fill_int_urb success\n");

        res = usb_submit_urb(device.intf_in_urb, GFP_KERNEL);
        if (res < 0) {
            printk(KERN_INFO "+ ERROR: usb_submit_urb error\n");
            return -1;
        }

        device.connected = 1;
        printk(KERN_INFO "+ INFO : device connection success\n");

        return 0;
    }

    printk(KERN_INFO "+ ERROR: device in use\n");
    return -1;
}

static void mouse_disconnect(struct usb_interface *interface)
{
    printk(KERN_INFO "+ INFO : call mouse_disconnect\n");
    usb_set_intfdata(device.interface, NULL);
    device.connected = 0;
    usb_deregister_dev(interface, &adriver_class);
    usb_kill_urb(device.intf_in_urb);
    printk(KERN_INFO "+ INFO : mouse disconnect\n");
}

static struct usb_device_id mouse_usb_id_table[] = {
    {USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
    {}
};
MODULE_DEVICE_TABLE(usb, mouse_usb_id_table);

static struct usb_driver adriver = {
    .name       = DRIVER_NAME,
    .probe      = mouse_probe,
    .disconnect = mouse_disconnect,
    .id_table   = mouse_usb_id_table,
};

static int __init my_driver_init(void)
{
    printk(KERN_INFO "+ INFO : call my_driver_init\n");
    int r;
    device.connected = 0;
    r = usb_register(&adriver);

    if (r < 0) {
        printk(KERN_ERR "+ ERROR: usb_register error, return code %d\n", r);
        return -1;
    }

    printk(KERN_INFO "+ INFO : driver load\n");
    return 0;
}

static void __exit my_driver_exit(void)
{
    printk(KERN_INFO "+ INFO : call my_driver_exit\n");
    usb_deregister(&adriver);
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeria Avdeykina");
