#define __private_include__
#include <private/nvm.h>
#include <platform/fvm.h>
#include <flipper/fmr.h>

/* Pointer to the start of virtual non-volatile memory. */
uint8_t *v_nvm;
/* Declaration of the virtual packet. */
struct _fmr_packet vpacket;

/* Declaration of the virtual endpoint. */
struct _lf_endpoint lf_fvm_ep = {
	fvm_configure,
	fvm_ready,
	fvm_put,
	fvm_get,
	fvm_push,
	fvm_pull,
	fvm_destroy
};

/* The fmr_device object containing global state about the Flipper Virtual Machine. */
struct _lf_device self = {
	{
		"fvm",
		0x363f,
		LF_VERSION,
		(lf_device_32bit | lf_device_little_endian)
	},
	&lf_fvm_ep,
	E_OK,
	NULL
};

int fvm_configure(struct _lf_endpoint *endpoint) {
	/* Allocate the memory needed to emulate NVM. */
	v_nvm = (uint8_t *)malloc(NVM_SIZE);
	/* Format NVM. */
	nvm_format();
	return 0;
}

uint8_t fvm_ready(void) {
	return 0;
}

void fvm_put(uint8_t byte) {

}

uint8_t fvm_get(void) {
	return 0;
}

int fvm_push(void *source, lf_size_t length) {
	memset(&vpacket, 0, sizeof(struct _fmr_packet));
	memcpy(&vpacket, source, length);

	struct _fmr_result result;
	/* If the host is asking for the device attributes, return them. */
	if (vpacket.target.attributes & LF_CONFIGURATION) {
		/* Send the configuration information back to the host. */
		memcpy(&vpacket, &self.configuration, sizeof(struct _lf_configuration));
	} else {
#ifdef __lf_debug__
		/* Print the packet contents for debugging. */
		lf_debug_packet(&vpacket);
#endif
		/* Pause side effect generation for errors. */
		error_pause();
		/* Execute the packet. */
		fmr_perform(&vpacket, &result);
		/* Resume side effect generation. */
		error_resume();
		/* Send the result back to the host. */
		memcpy(&vpacket, &result, sizeof(struct _fmr_result));
	}
	/* Clear any error state generated by the procedure. */
	error_clear();
	return lf_success;
}

int fvm_pull(void *destination, lf_size_t length) {
	memcpy(destination, &vpacket, length);
	return lf_success;
}

int fvm_destroy(struct _lf_endpoint *endpoint) {
	if (v_nvm) {
		free(v_nvm);
		v_nvm = NULL;
	}
	return lf_success;
}
