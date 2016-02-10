/*
 * deviced
 *
 * Copyright (c) 2012 - 2013 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef __DD_USBHOST_H__
#define __DD_USBHOST_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file        dd-usbhost.h
 * @ingroup     CAPI_SYSTEM_DEVICED
 * @defgroup    CAPI_SYSTEM_DEVICED_USBHOST_MODULE usbhost
 * @brief       This file provides the API for usb host operations
 */

/**
 * @addtogroup CAPI_SYSTEM_DEVICED_USBHOST_MODULE
 * @{
 */

#include <limits.h>
struct usbhost_device {
	char devpath[PATH_MAX]; /* unique info. */
	int baseclass;
	int subclass;
	int protocol;
	int vendorid;
	int productid;
	char *manufacturer;
	char *product;
	char *serial;
};

enum usbhost_state {
	USB_HOST_REMOVED,
	USB_HOST_ADDED,
};

/**
 * @par Description:
 *      This API is used to initialize usbhost signal \n
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *  ...
 * @endcode
 */
int init_usbhost_signal(void);

/**
 * @par Description:
 *      This API is used to deinitialize usbhost signal \n
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  // Do something
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
void deinit_usbhost_signal(void);

/**
 * @par Description:
 *      This API is used to register usbhost signal \n
 * @param[in] storage_changed callback function which is called when the usbhost signal is delivered
 * @param[in] data parameter of storage_changed callback function
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  // Do something
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int register_usb_storage_change_handler(
		void (*storage_changed)(char *type, char *path, int mount, void *),
		void *data);

/**
 * @par Description:
 *      This API is used to register usbhost signal \n
 * @param[in] device_changed callback function which is called when the device is connected/disconnected
 * @param[in] data parameter of the callback function
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_device_change_handler(device_cb, data) < 0) {
 *      printf("Failed to register device handler\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  // Do something
 *
 *  if (unregister_usb_device_changed_handler() < 0)
 *      printf("Failed to unregister device changed signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int register_usb_device_change_handler(
		void (*device_changed)(struct usbhost_device *device, int state, void *data),
		void *data);

/**
 * @par Description:
 *      This API is used to unregister usbhost signal \n
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  // Do something
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int unregister_usb_storage_change_handler(void);

/**
 * @par Description:
 *      This API is used to unregister usb connect/disconnect signal handler\n
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_device_change_handler(device_cb, data) < 0) {
 *      printf("Failed to register device signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  // Do something
 *
 *  if (unregister_usb_device_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int unregister_usb_device_change_handler(
		void (*device_changed)(struct usbhost_device *device, int state, void *data));

/**
 * @par Description:
 *      This API is used to request usb storage information \n
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  if (request_usb_storage_info() < 0)
 *      printf("Failed to request all of storage information");
 *
 *  // Do someting
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int request_usb_storage_info(void);

/**
 * @par Description:
 *      This API is used to mount usb storage \n
 * @param[in] path path to unmount
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  if (mount_usb_storage("/opt/storage/usb/UsbDriveA1") < 0)
 *      printf("Failed to mount usb storage");
 *
 *  // Do something.. Mounting takes some time
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int mount_usb_storage(char *path);

/**
 * @par Description:
 *      This API is used to unmount usb storage \n
 * @param[in] path path to mount
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  if (ummount_usb_storage("/opt/storage/usb/UsbDriveA1") < 0)
 *      printf("Failed to unmount usb storage");
 *
 *  // Do something.. Unmounting takes some time
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int unmount_usb_storage(char *path);

/**
 * @par Description:
 *      This API is used to format usb storage \n
 * @param[in] path path to format
 * @return 0 on success, negative if failed
 * @par Example
 * @code
 *  ...
 *  if (init_usbhost_signal() < 0)
 *      printf("Failed to initialize usbhost signal\n");
 *
 *  if (register_usb_storage_change_handler(storage_cb, data) < 0) {
 *      printf("Failed to register storage signal\n");
 *      deinit_usbhost_signal();
 *      return;
 *  }
 *
 *  if (format_usb_storage("/opt/storage/usb/UsbDriveA1") < 0)
 *      printf("Failed to unmount usb storage");
 *
 *  // Do something.. Formatting takes some time
 *
 *  if (unregister_usb_storage_change_handler() < 0)
 *      printf("Failed to unregister storage signal\n");
 *
 *  deinit_usbhost_signal();
 *  ...
 * @endcode
 */
int format_usb_storage(char *path);

/**
 * @par Description:
 *      This API is used to open usb device \n
 * @param[in] path path to device
 * @return 0 on success, negative error code if failed
 * @par Example
 * @code
 * ...
 *
 * r = open_usb_device(path, &fd);
 * if (r < 0)
 * 	printf("failed to open device");
 *
 * ...
 * @endcode
 */
int open_usb_device(char *path, int *fd);

/**
 * @} // end of CAPI_SYSTEM_DEVICED_USBHOST_MODULE
 */

#ifdef __cplusplus
}
#endif
#endif
