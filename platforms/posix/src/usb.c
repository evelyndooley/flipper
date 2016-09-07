/* usb.c - USB endpoint wrapper using libusb. */

#define __private_include__
#include <platform/posix/usb.h>
#include <flipper/flipper.h>
#include <libusb-1.0/libusb.h>
#include <flipper/error.h>

#define USB_DIRECTION_IN 0x80
#define USB_DIRECTION_OUT 0x01

const struct _lf_endpoint usb = {
	usb_configure,
	usb_ready,
	usb_put,
	usb_get,
	usb_push,
	usb_pull,
	usb_destroy
};

struct _libusb_record {
	struct libusb_device_handle *handle;
	struct libusb_context *context;
};

int usb_configure(struct _lf_endpoint *endpoint) {
	/* Allocate memory for the USB record if it has not yet been allocated. */
	if (!(endpoint -> record)) {
		endpoint -> record = malloc(sizeof(struct _libusb_record));
	}
	struct _libusb_record *record = endpoint -> record;
	/* Initialize the libusb context associated with this endpoint. */
	int _e = libusb_init(&(record -> context));
	if (_e < 0) {
		error_raise(E_LIBUSB, error_message("Failed to initialize libusb."));
		return lf_error;
	}
	/* Attach a physical device to this endpoint. */
	record -> handle = libusb_open_device_with_vid_pid(record -> context, VENDOR, PRODUCT);
	if (!(record -> handle)) {
		error_raise(E_LIBUSB, error_message("Failed to open USB device."));
		return lf_error;
	}
	/* Claim the interface used to send and receive message runtime packets. */
	_e = libusb_claim_interface(record -> handle, 0);
	if (_e < 0) {
		error_raise(E_LIBUSB, error_message("Failed to claim interface 0."));
		return lf_error;
	}
	libusb_set_debug(record -> context, 3);
	/* Reset the device's USB controller. */
	libusb_reset_device(record -> handle);
	return lf_success;
}

uint8_t usb_ready(void) {

	return 0;
}

void usb_put(uint8_t byte) {

}

uint8_t usb_get(void) {

	return 0;
}

int usb_transfer(void *data, lf_size_t length, uint8_t direction) {
	struct _libusb_record *record = flipper.device -> endpoint -> record;
	if (!record) {
		error_raise(E_ENDPOINT, error_message("No libusb record associated with the selected USB endpoint. Did you attach?"));
		return lf_error;
	}
	int _length, _e;
	if (length <= sizeof(struct _fmr_packet)) {
		_e = libusb_interrupt_transfer(record -> handle, INTERRUPT_ENDPOINT + direction, data, length, &_length, 0);
	}
	else {
		_e = libusb_bulk_transfer(record -> handle, BULK_ENDPOINT + direction, data, length, &_length, 0);
	}
	if (_e < 0 || (_length != length)) {
		error_raise(E_TRANSFER, error_message("Failed to transfer data to/from USB device."));
		return lf_error;
	}
	return lf_success;
}

int usb_push(void *source, lf_size_t length) {
	return usb_transfer(source, length, USB_DIRECTION_OUT);
}

int usb_pull(void *destination, lf_size_t length) {
	return usb_transfer(destination, length, USB_DIRECTION_IN);
}

int usb_destroy(struct _lf_endpoint *endpoint) {
	struct _libusb_record *record = flipper.device -> endpoint -> record;
	if (!record) {
		error_raise(E_ENDPOINT, error_message("No libusb record associated with the selected USB endpoint. Did you attach?"));
		return lf_error;
	}
	/* Close the device handle. */
	libusb_close(record -> handle);
	/* Destroy the libusb context. */
	libusb_exit(record -> context);
	return lf_success;
}