#define __private_include__
#include <flipper/flipper.h>
#include <flipper/carbon/pwm.h>

int pwm_configure(void) {
	return lf_invoke(&_pwm, _pwm_configure, NULL);
}