#define __private_include__
#include <flipper/gpio.h>

/* Define the virtual interface for this module. */
const struct _gpio gpio = {
	gpio_configure,
	gpio_set_direction,
	gpio_write,
	gpio_read
};
