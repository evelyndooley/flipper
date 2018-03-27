#include <flipper.h>

enum { _temp_configure };

int temp_configure(void);

void *temp_interface[] = {
	&temp_configure
};

LF_MODULE(temp, "temp", temp_interface);

LF_WEAK int temp_configure(void) {
	return lf_invoke(lf_get_current_device(), "temp", _temp_configure, lf_int32_t, NULL);
}
