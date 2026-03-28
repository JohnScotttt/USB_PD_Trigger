#include "hid.h"

#include <ch32x035.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "usbd_core.h"
#include "usbd_hid.h"

#include "../config.h"
#include "delay.h"
#include "uid.h"

// config descriptor size
#define USB_HID_CONFIG_DESC_SIZE (9 + 9 + 9 + 7 + 7)

// custom hid report descriptor size
#define HID_CUSTOM_REPORT_DESC_SIZE 34

#ifdef CONFIG_USBDEV_ADVANCE_DESC
static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0002, 0x01),
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_HID_CONFIG_DESC_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /************** Descriptor of Custom interface *****************/
    0x09,                          /* bLength: Interface Descriptor size */
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType: Interface descriptor type */
    0x00,                          /* bInterfaceNumber: Number of Interface */
    0x00,                          /* bAlternateSetting: Alternate setting */
    0x02,                          /* bNumEndpoints */
    0x03,                          /* bInterfaceClass: HID */
    0x01,                          /* bInterfaceSubClass : 1=BOOT, 0=no boot */
    0x00,                          /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
    0,                             /* iInterface: Index of string descriptor */
    /******************** Descriptor of Custom HID ********************/
    0x09,                    /* bLength: HID Descriptor size */
    HID_DESCRIPTOR_TYPE_HID, /* bDescriptorType: HID */
    0x11,                    /* bcdHID: HID Class Spec release number */
    0x01,
    0x00,                        /* bCountryCode: Hardware target country */
    0x01,                        /* bNumDescriptors: Number of HID class descriptors to follow */
    0x22,                        /* bDescriptorType */
    HID_CUSTOM_REPORT_DESC_SIZE, /* wItemLength: Total length of Report descriptor */
    0x00,
    /******************** Descriptor of Custom in endpoint ********************/
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    HIDRAW_IN_EP,                 /* bEndpointAddress: Endpoint Address (IN) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    WBVAL(HIDRAW_IN_EP_SIZE),     /* wMaxPacketSize: 64 Byte max */
    HIDRAW_IN_INTERVAL,           /* bInterval: Polling Interval */
    /******************** Descriptor of Custom out endpoint ********************/
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    HIDRAW_OUT_EP,                /* bEndpointAddress: Endpoint Address (OUT) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    WBVAL(HIDRAW_OUT_EP_SIZE),    /* wMaxPacketSize: 64 Byte max */
    HIDRAW_OUT_EP_INTERVAL,       /* bInterval: Polling Interval */
};

static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40,
    0x00, 0x00,
};

static const char *string_descriptors[] = {
    (const char[]){0x09, 0x04}, /* Langid */
    "JohnScotttt",              /* Manufacturer */
    "USB-PD-Trigger Bootloader",/* Product */
    "<UID>",                    /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    static char serial_buffer[48];

    if (index == 2)
    {
        snprintf(serial_buffer, sizeof(serial_buffer),
                 "USB-PD-Trigger Bootloader v%d.%d",
                 CONFIG_HW_VERSION_MAJOR, CONFIG_HW_VERSION_MINOR);
        return serial_buffer;
    }

    if (index == 3)
    {
        snprintf(serial_buffer, sizeof(serial_buffer), "%08X-BL%d.%d.%d",
                 CHIP_UID_2,
                 CONFIG_BL_VERSION_MAJOR,
                 CONFIG_BL_VERSION_MINOR,
                 CONFIG_BL_VERSION_PATCH);
        return serial_buffer;
    }

    if (index > 3)
        return NULL;

    return string_descriptors[index];
}

const struct usb_descriptor hid_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};
#else
#error "Please define CONFIG_USBDEV_ADVANCE_DESC"
#endif

// custom hid report descriptor
static const uint8_t hid_custom_report_desc[HID_CUSTOM_REPORT_DESC_SIZE] = {
    0x06, 0x00, 0xFF, // USAGE_PAGE (VENDOR DEFINED PAGE 1)
    0x09, 0x01,       // USAGE (VENDOR USAGE 1)
    0xA1, 0x01,       // COLLECTION (APPLICATION)
                      //   IN
    0x09, 0x01,       //   USAGE (VENDOR USAGE 1)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00, //   LOGICAL_MAXIMUM (255)
    0x95, 0x40,       //   REPORT_COUNT (64)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x81, 0x02,       //   INPUT (DATA,VAR,ABS)
                      //   OUT
    0x09, 0x01,       //   USAGE (VENDOR USAGE 1)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0x00, //   LOGICAL_MAXIMUM (255)
    0x95, 0x40,       //   REPORT_COUNT (64)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x91, 0x02,       //   OUTPUT (DATA,VAR,ABS)
                      //
    0xC0              // END_COLLECTION
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buffer[HIDRAW_OUT_EP_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t send_buffer[HIDRAW_IN_EP_SIZE];

// hid state
static volatile uint8_t custom_state;

// Simple single-packet buffer for IAP commands
#define IAP_CMD_BUF_DEPTH 4
static uint8_t iap_cmd_buf[IAP_CMD_BUF_DEPTH][HIDRAW_OUT_EP_SIZE];
static volatile uint8_t iap_cmd_head;
static volatile uint8_t iap_cmd_tail;

#define IAP_RING_INC(x) (((x) + 1) % IAP_CMD_BUF_DEPTH)

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event)
    {
    case USBD_EVENT_RESET:
        break;
    case USBD_EVENT_CONNECTED:
        break;
    case USBD_EVENT_DISCONNECTED:
        break;
    case USBD_EVENT_RESUME:
        break;
    case USBD_EVENT_SUSPEND:
        break;
    case USBD_EVENT_CONFIGURED:
        custom_state = HID_STATE_IDLE;
        usbd_ep_start_read(busid, HIDRAW_OUT_EP, read_buffer, HIDRAW_OUT_EP_SIZE);
        break;
    default:
        break;
    }
}

// host <- device
static void usbd_hid_custom_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    custom_state = HID_STATE_IDLE;
}

// host -> device
static void usbd_hid_custom_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    // Accept Mini format packets for IAP
    if (nbytes >= 6
        && read_buffer[0] == HID_CMD_HEADER_0_MINI
        && read_buffer[1] == HID_CMD_HEADER_1
        && read_buffer[HID_CMD_MINI_DATA_TYPE_OFFSET] == HID_CMD_TYPE_SYS)
    {
        // Push into ring buffer
        if (IAP_RING_INC(iap_cmd_head) != iap_cmd_tail)
        {
            memcpy(iap_cmd_buf[iap_cmd_head], read_buffer, HIDRAW_OUT_EP_SIZE);
            iap_cmd_head = IAP_RING_INC(iap_cmd_head);
        }
    }
    usbd_ep_start_read(busid, ep, read_buffer, HIDRAW_OUT_EP_SIZE);
}

static struct usbd_endpoint custom_in_ep = {
    .ep_cb = usbd_hid_custom_in_callback,
    .ep_addr = HIDRAW_IN_EP,
};

static struct usbd_endpoint custom_out_ep = {
    .ep_cb = usbd_hid_custom_out_callback,
    .ep_addr = HIDRAW_OUT_EP,
};

static struct usbd_interface intf0;

void usb_init(void)
{
    usbd_desc_register(0, &hid_descriptor);

    usbd_add_interface(0, usbd_hid_init_intf(0, &intf0, hid_custom_report_desc, HID_CUSTOM_REPORT_DESC_SIZE));
    usbd_add_endpoint(0, &custom_in_ep);
    usbd_add_endpoint(0, &custom_out_ep);

    usbd_initialize(0, 0, usbd_event_handler);
}

void usb_dc_low_level_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_16;  // PC_USB_DM
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_17;  // PC_USB_DP
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    AFIO->CTLR = (AFIO->CTLR & ~(AFIO_CTLR_UDP_PUE | AFIO_CTLR_UDM_PUE)) | AFIO_CTLR_USB_PHY_V33 | AFIO_CTLR_UDP_PUE | AFIO_CTLR_USB_IOEN;

    NVIC_InitTypeDef NVIC_InitStructure = {0};
    NVIC_InitStructure.NVIC_IRQChannel = USBFS_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

uint8_t hid_send_report(const uint8_t *data, uint16_t len)
{
    if (custom_state != HID_STATE_IDLE)
        return 0;
    if (len > HIDRAW_IN_EP_SIZE)
        return 0;

    memcpy(send_buffer, data, len);
    if (len < HIDRAW_IN_EP_SIZE)
    {
        memset(send_buffer + len, 0, HIDRAW_IN_EP_SIZE - len);
    }

    int ret = usbd_ep_start_write(0, HIDRAW_IN_EP, send_buffer, HIDRAW_IN_EP_SIZE);
    if (ret == 0)
    {
        custom_state = HID_STATE_BUSY;
        return 1;
    }
    return 0;
}

// ---- IAP command buffer API ----

bool hid_has_iap_cmd(void)
{
    return iap_cmd_head != iap_cmd_tail;
}

const uint8_t *hid_get_iap_cmd(void)
{
    if (!hid_has_iap_cmd())
        return NULL;
    return iap_cmd_buf[iap_cmd_tail];
}

void hid_pop_iap_cmd(void)
{
    if (hid_has_iap_cmd())
        iap_cmd_tail = IAP_RING_INC(iap_cmd_tail);
}
