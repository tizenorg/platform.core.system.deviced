#include <linux/usb/functionfs.h>
#include <linux/usb/ch9.h>
#include <stdio.h>
#include <endian.h>

/******************** Descriptors and Strings *******************************/

#define MISSING_DESC_HEAD
#ifdef MISSING_DESC_HEAD
enum {
        FUNCTIONFS_DESCRIPTORS_MAGIC_V2 = 3,
};

enum functionfs_flags {
        FUNCTIONFS_HAS_FS_DESC = 1,
        FUNCTIONFS_HAS_HS_DESC = 2,
};

struct usb_functionfs_descs_head_v2 {
        __le32 magic;
        __le32 length;
        __le32 flags;
        /*
         * __le32 fs_count, hs_count, fs_count; must be included manually in
         * the structure taking flags into consideration.
         */
} __attribute__((packed));
#endif

static const struct {
	struct usb_functionfs_descs_head_v2 header;
	__le32 fs_count;
	__le32 hs_count;
	struct {
		struct usb_interface_descriptor intf;
		struct usb_endpoint_descriptor_no_audio bulk_in;
		struct usb_endpoint_descriptor_no_audio bulk_out;
	} __attribute__ ((__packed__)) fs_descs, hs_descs;
} __attribute__ ((__packed__)) descriptors = {
	.header = {
		.magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
		.flags = htole32(FUNCTIONFS_HAS_FS_DESC |
				     FUNCTIONFS_HAS_HS_DESC),
		.length = htole32(sizeof(descriptors)),
	},
	.fs_count = htole32(3),
	.fs_descs = {
		.intf = {
			.bLength = sizeof(descriptors.fs_descs.intf),
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 2,
			.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
			.iInterface = 1,
		},
		.bulk_in = {
			.bLength = sizeof(descriptors.fs_descs.bulk_in),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
		},
		.bulk_out = {
			.bLength = sizeof(descriptors.fs_descs.bulk_out),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 2 | USB_DIR_OUT,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
		},
	},
	.hs_count = htole32(3),
	.hs_descs = {
		.intf = {
			.bLength = sizeof(descriptors.hs_descs.intf),
			.bDescriptorType = USB_DT_INTERFACE,
			.bNumEndpoints = 2,
			.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
			.iInterface = 1,
		},
		.bulk_in = {
			.bLength = sizeof(descriptors.hs_descs.bulk_in),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 1 | USB_DIR_IN,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize = htole16(512),
		},
		.bulk_out = {
			.bLength = sizeof(descriptors.hs_descs.bulk_out),
			.bDescriptorType = USB_DT_ENDPOINT,
			.bEndpointAddress = 2 | USB_DIR_OUT,
			.bmAttributes = USB_ENDPOINT_XFER_BULK,
			.wMaxPacketSize = htole16(512),
		},
	},
};

#define STR_INTERFACE "loop input to output"

static const struct {
	struct usb_functionfs_strings_head header;
	struct {
		__le16 code;
		const char str1[sizeof(STR_INTERFACE)];
	} __attribute__ ((__packed__)) lang0;
} __attribute__ ((__packed__)) strings = {
	.header = {
		.magic = htole32(FUNCTIONFS_STRINGS_MAGIC),
		.length = htole32(sizeof(strings)),
		.str_count = htole32(1),
		.lang_count = htole32(1),
	},
	.lang0 = {
		htole16(0x0409), /* en-us */
		STR_INTERFACE,
	},
};

int main()
{
	int ret;
	FILE *sfp, *dfp;

	dfp = fopen("descs", "w");
	if (!dfp) {
		perror("Could not open descritptors file");
		return -1;
	}

	sfp = fopen("strs", "w");
	if (!sfp) {
		perror("Could not open strings file");
		return -1;
	}

	ret = fwrite(&descriptors, sizeof(descriptors), 1, dfp);
	if (ret < 0) {
		perror("Could not write descriptors");
		goto out;
	}

	ret = fwrite(&strings, sizeof(strings), 1, sfp);
	if (ret < 0) {
		perror("Could not write strings");
		goto out;
	}

out:
	close(sfp);
	close(dfp);

	return ret;
}
