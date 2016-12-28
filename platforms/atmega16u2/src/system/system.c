#define __private_include__
#include <platforms/atmega16u2.h>
#include <private/megausb.h>
#include <flipper/modules.h>

/* The fmr_device object containing global state about this device. */
struct _lf_device lf_self = {
	{
		"flipper",
		0xc713,
		LF_VERSION,
		(lf_device_16bit | lf_device_little_endian)
	},
	&megausb,
	E_OK,
	false,
	NULL
};

#define cpu_prescale(clock) (CLKPR = 0x80, CLKPR = clock)

/* Helper functions to libflipper. */
void fmr_push(fmr_module module, fmr_function function, lf_size_t length) {
	void *swap = malloc(length);
	if (!swap) {
		error_raise(E_MALLOC, NULL);
		return;
	}
	megausb_pull(swap, length);
	uint32_t types = fmr_type(lf_size_t) << 2 | fmr_type(void *);
	struct {
		void *source;
		lf_size_t length;
	} args = { swap, length };
	fmr_execute(module, function, 2, types, &args);
	free(swap);
}

void fmr_pull(fmr_module module, fmr_function function, lf_size_t length) {
	void *swap = malloc(length);
	if (!swap) {
		error_raise(E_MALLOC, NULL);
		return;
	}
	uint32_t types = fmr_type(lf_size_t) << 2 | fmr_type(void *);
	struct {
		void *source;
		lf_size_t length;
	} args = { swap, length };
	/* Call the function. */
	fmr_execute(module, function, 2, types, &args);
	megausb_push(swap, length);
	free(swap);
}

void system_task(void) {
	while (1) {
		struct _fmr_packet packet;
		int8_t _e = megausb_pull((void *)(&packet), sizeof(struct _fmr_packet));
		if (!(_e < lf_success)) {
			/* Create a buffer to the result of the operation. */
			struct _fmr_result result;
			/* Execute the packet. */
			fmr_perform(&packet, &result);
			/* Send the result back to the host. */
			megausb_push(&result, sizeof(struct _fmr_result));
			/* Clear any error state generated by the procedure. */
			error_clear();
		}
	}
}

void system_init() {
	/* Prescale CPU for maximum clock. */
	cpu_prescale(0);
	/* Configure the USB subsystem. */
	configure_usb();
	/* Configure the UART0 subsystem. */
	uart0_configure();
	/* Configure the SAM4S. */
	cpu_configure();
	/* Configure reset button and PCINT8 interrupt. */
//	PCMSK1 |= (1 << PCINT8);
//	PCICR |= (1 << PCIE1);
	/* Globally enable interrupts. */
	sei();
}

void system_deinit(void) {
	/* Clear the state of the status LED. */
	led_set_rgb(LED_OFF);
}

void system_reset(void) {
	/* Shutdown the system. */
	system_deinit();
	/* Fire the watchdog timer. */
	wdt_fire();
	/* Wait until reset. */
	while (1);
}

/* PCINT8 interrupt service routine; captures reset button press and resets the device. */
ISR (PCINT1_vect) {
	system_reset();
}