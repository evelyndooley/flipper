#define __private_include__
#include <flipper/libflipper.h>
#include <flipper/fmr.h>

/* Include the Carbon board file. */
#include <flipper/carbon.h>

lf_device_list lf_attached_devices;
struct _lf_device *lf_current_device;
lf_event_list lf_registered_events;

/* Expose the virtual interface for this driver. */
const struct _flipper flipper = {
	flipper_attach,
	flipper_select,
	flipper_detach,
	flipper_exit,
};

/* Creates a new device. */
struct _lf_device *lf_device_create(struct _lf_endpoint *endpoint) {
	struct _lf_device *device = (struct _lf_device *)calloc(1, sizeof(struct _lf_device));
	lf_assert(device, failure, E_MALLOC, "Failed to allocate memory for new device.");
	device -> endpoint = endpoint;
	return device;
failure:
	if (device) free(device);
	return NULL;
}

/* Attempts to attach to all unattached devices. Returns how many devices were attached. */
int lf_attach(struct _lf_device *device) {
	lf_assert(device, failure, E_NULL, "NULL device pointer provided for attach.");
	/* Ask the device for its configuration. */
	int _e = lf_load_configuration(device);
	lf_assert(_e == lf_success, failure, E_CONFIGURATION, "Failed to obtain configuration from device.");
	lf_ll_append(&lf_get_device_list(), device, lf_detach);
	lf_select(device);
	return lf_success;
failure:
	return lf_error;
}

/* Detaches a device from libflipper. */
int lf_detach(struct _lf_device *device) {
	lf_assert(device, failure, E_NULL, "NULL device pointer provided for detach.");
	/* Release the device's endpoint. */
	lf_endpoint_release(device -> endpoint);
	return lf_success;
failure:
	return lf_error;
}

/* Call's the device's selector function and selects the device. */
int lf_select(struct _lf_device *device) {
	lf_assert(device, failure, E_NULL, "NULL device pointer provided for selection.");
	device -> selector(device);
	lf_set_current_device(device);
failure:
	return lf_error;
}

/* Deactivates libflipper state and releases the event loop. */
int __attribute__((__destructor__)) lf_exit(void) {
	/* Release all of the events. */
	lf_ll_release(&lf_get_event_list());
	/* Release all of the attached devices. */
	lf_ll_release(&lf_get_device_list());
	return lf_success;
}

/* Shim to attach all possible flipper devices that could be attached to the system. */
int flipper_attach(void) {
	return carbon_attach();
}

int flipper_select(struct _lf_device *device) {
	lf_assert(device, failure, E_NULL, "NULL device pointer provided for selection.");
	lf_set_current_device(device);
	return lf_success;
failure:
	return lf_error;
}

/* Shim around lf_detach. */
int flipper_detach(struct _lf_device *device) {
	return lf_detach(device);
}

/* Shim around lf_exit. */
int flipper_exit(void) {
	return lf_exit();
}

int lf_load_configuration(struct _lf_device *device) {
	/* Create a configuration packet. */
	struct _fmr_packet packet = { 0 };
	/* Set the magic number. */
	packet.header.magic = FMR_MAGIC_NUMBER;
	/* Compute the length of the packet. */
	packet.header.length = sizeof(struct _fmr_header);
	/* Make the outgoing packet a configuration packet. */
	packet.header.class = fmr_configuration_class;
	/* Calculate the packet checksum. */
	packet.header.checksum = lf_crc(&packet, packet.header.length);
	/* Send the packet to the target device. */
	int _e = lf_transfer(device, &packet);
	if (_e < lf_success) {
		return lf_error;
	}
	/* Obtain the configuration from the device. */
	struct _lf_configuration configuration;
	_e = device -> endpoint -> pull(device -> endpoint, &configuration, sizeof(struct _lf_configuration));
	if (_e < lf_success) {
		return lf_error;
	}
	/* Obtain the result of the operation. */
	struct _fmr_result result;
	_e = lf_get_result(device, &result);
	if (_e < lf_success) {
		return lf_error;
	}

	// /* Compare the device identifiers. */
	// if (device -> configuration.identifier != configuration.identifier) {
	// 	lf_error_raise(E_NO_DEVICE, error_message("Identifier mismatch for device '%s'. (0x%04x instead of 0x%04x)", device -> configuration.name, configuration.identifier, device -> configuration.identifier));
	// 	return lf_error;
	// }

	/* Copy the returned configuration into the device. */
	memcpy(&(device -> configuration), &configuration, sizeof(struct _lf_configuration));
	return lf_success;
}

/* Binds the lf_module structure to its counterpart on the attached device. */
int lf_bind(struct _lf_module *module) {
	/* Ensure that the module structure was allocated successfully. */
	if (!module) {
		lf_error_raise(E_NULL, error_message("No module provided to bind."));
		return lf_error;
	}
	/* Calculate the identifier of the module, including the NULL terminator. */
	lf_crc_t identifier = lf_crc(module -> name, strlen(module -> name) + 1);
	/* Attempt to get the module index. */
	fmr_module index = fld_index(identifier) | FMR_USER_INVOCATION_BIT;
	/* Throw an error if there is no counterpart module found. */
	lf_assert(index == -1, failure, E_MODULE, "No counterpart module loaded for bind to module '%s'.", module -> name);
	/* Set the module's indentifier. */
	module -> identifier = identifier;
	/* Set the module's index. */
	module -> index = index;
	/* Set the module's device. */
	module -> device = &lf_get_current_device();
	return lf_success;
failure:
	return lf_error;
}

/* PROTOTYPE FUNCTION: Load an image into a device's RAM. */
int lf_ram_load(struct _lf_device *device, void *source, lf_size_t length) {
	if (!source) {
		lf_error_raise(E_NULL, error_message("No source provided for load operation."));
	} else if (!length) {
		return lf_success;
	}
	/* If no device is provided, throw an error. */
	if (!device) {
		lf_error_raise(E_NO_DEVICE, error_message("Failed to load to device."));
		return lf_error;
	}
	struct _fmr_packet _packet = { 0 };
	struct _fmr_push_pull_packet *packet = (struct _fmr_push_pull_packet *)(&_packet);
	/* Set the magic number. */
	_packet.header.magic = FMR_MAGIC_NUMBER;
	/* Compute the initial length of the packet. */
	_packet.header.length = sizeof(struct _fmr_push_pull_packet);
	/* Set the packet class. */
	_packet.header.class = fmr_ram_load_class;
	/* Set the push length. */
	packet -> length = length;
	/* Compute and store the packet checksum. */
	_packet.header.checksum = lf_crc(packet, _packet.header.length);
	/* Send the packet to the target device. */
	int _e = lf_transfer(device, &_packet);
	if (_e < lf_success) {
		return lf_error;
	}
	/* Transfer the data through to the address space of the device. */
	_e = device -> endpoint -> push(device -> endpoint, source, length);
	/* Ensure that the data was successfully transferred to the device. */
	if (_e < lf_success) {
		return lf_error;
	}
	struct _fmr_result result;
	/* Obtain the result of the operation. */
	lf_get_result(device, &result);
	/* Return a pointer to the data. */
	return result.value;
}

/* Debugging functions for displaying the contents of various FMR related data structures. */

void lf_debug_call(struct _fmr_invocation *call) {
	printf("call:\n");
	printf("\t└─ index:\t0x%x\n", call -> index);
	printf("\t└─ function:\t0x%x\n", call -> function);
	printf("\t└─ types:\t0x%x\n", call -> types);
	printf("\t└─ argc:\t0x%x (%d arguments)\n", call -> argc, call -> argc);
	printf("arguments:\n");
	/* Calculate the offset into the packet at which the arguments will be loaded. */
	uint8_t *offset = call -> parameters;
	char *typestrs[] = { "fmr_int8", "fmr_int16", "fmr_int32" };
	fmr_types types = call -> types;
	for (int i = 0; i < call -> argc; i ++) {
		fmr_type type = types & 0x3;
		fmr_arg arg = 0;
		memcpy(&arg, offset, fmr_sizeof(type));
		printf("\t└─ %s:\t0x%x\n", typestrs[type], arg);
		offset += fmr_sizeof(type);
		types >>= 2;
	}
	printf("\n");
}

void lf_debug_packet(struct _fmr_packet *packet, size_t length) {
	if (packet -> header.magic == FMR_MAGIC_NUMBER) {
		printf("header:\n");
		printf("\t└─ magic:\t0x%x\n", packet -> header.magic);
		printf("\t└─ checksum:\t0x%x\n", packet -> header.checksum);
		printf("\t└─ length:\t%d bytes (%.02f%%)\n", packet -> header.length, (float) packet -> header.length/sizeof(struct _fmr_packet)*100);
		char *classstrs[] = { "configuration", "std_call", "user_call", "push", "pull", "event" };
		printf("\t└─ class:\t%s\n", classstrs[packet -> header.class]);
		struct _fmr_invocation_packet *invocation = (struct _fmr_invocation_packet *)(packet);
		struct _fmr_push_pull_packet *pushpull = (struct _fmr_push_pull_packet *)(packet);
		switch (packet -> header.class) {
			case fmr_configuration_class:
			break;
			case fmr_standard_invocation_class:
				lf_debug_call(&invocation -> call);
			break;
			case fmr_user_invocation_class:
				lf_debug_call(&invocation -> call);
			break;
			case fmr_push_class:
			case fmr_pull_class:
				printf("length:\n");
				printf("\t└─ length:\t0x%x\n", pushpull -> length);
				lf_debug_call(&pushpull -> call);
			break;
			default:
				printf("Invalid packet class.\n");
			break;
		}
		for (int i = 1; i <= length; i ++) {
			printf("0x%02x ", ((uint8_t *)packet)[i - 1]);
			if (i % 8 == 0 && i < length - 1) printf("\n");
		}
	} else {
		printf("Invalid magic number (0x%02x).\n", packet -> header.magic);
	}
	printf("\n\n-----------\n\n");
}

void lf_debug_result(struct _fmr_result *result) {
	printf("response:\n");
	printf("\t└─ value:\t0x%x\n", result -> value);
	printf("\t└─ error:\t0x%x\n", result -> error);
	printf("\n-----------\n\n");
}
