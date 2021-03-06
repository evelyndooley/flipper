/* The Osmium loader implementation. */

#include <os/loader.h>
#include <os/scheduler.h>

/*
 * The loader handles the loading of moudles and applications into program memory
 * and RAM for debugging. Flipper moudles and applications must be compiled
 * to comply with the Flipper ABI (Application Binary Interface) such that
 * the loader can properly handle the relocation of the data segment for modules
 * and applications loaded into ROM. RAM loaded modules and applications do not
 * require relocation of their .data and .bss sections, and thus be compiled as
 * position independant, and without a GOT (Global Offset Table).
 *
 */


 /* FDL Module/Application ABI Specification */

 /*------------------------------+  0x0000     --+
  |          sizeof(name)        |               |
  +------------------------------+  0x0004       |
  |            &(name)           |               |
  +------------------------------+  0x0008       |
  |            &(main)           |               |
  +------------------------------+  0x000c       |
  |        sizeof(.module)       |               |
  +------------------------------+  0x0010       |
  |           &(.module)         |               |
  +------------------------------+  0x0014       |
  |          sizeof(.data)       |               |
  +------------------------------+  0x0018       | - HEADER
  |            &(.data)          |               |
  +------------------------------+  0x001c       |
  |          sizeof(.bss)        |               |
  +------------------------------+  0x0020       |
  |            &(.bss)           |               |
  +------------------------------+  0x0024       |
  |          sizeof(.got)        |               |
  +------------------------------+  0x0028     --+
  |            &(.got)           |
  +------------------------------+  [0x000c]
  |           .module            |
  +------------------------------+
  |            .text             |
  +------------------------------+  [0x0018]
  |            .data             |
  +------------------------------+  [0x0020]
  |             .bss             |
  +------------------------------*/

struct _user_modules user_modules;

struct _os_app {
	/* Where the app was loaded. */
	void *base;
	/* The name of the app. */
	char *name;
	/* The task running the app. */
	struct _os_task *task;
};

struct _lf_ll *apps;

struct _os_app *get_app(struct _lf_abi_header *header) {
	char *name = (void *)header + header->name_offset;
	struct _os_app *app = NULL;
	int count = lf_ll_count(apps);
	for (int i = 0; i < count; i ++) {
		struct _os_app *item = lf_ll_item(apps, i);
		if (strcmp(name, item->name)) continue;
		app = item;
		break;
	}
	return app;
}

void os_app_exit(void *_app) {
	if (!_app) return;
	struct _os_app *app = (struct _os_app *)_app;
	free(app->base);
	lf_ll_remove(&apps, app);
}

/* Loads an application into RAM. */
int os_load_application(void *_base) {
	struct _os_task *task = NULL;

	struct _lf_abi_header *header = (struct _lf_abi_header *)_base;
	struct _os_app *app = get_app(header);
	if (app) os_task_release(app->task);

	app = malloc(sizeof(struct _os_app));
	lf_assert(app, failure, E_NULL, "Failed to allocate memory to create app.");
	app->task = NULL;
	app->base = _base;
	app->name = _base + header->name_offset;

	void *_main = _base + header->entry;
	task = os_task_create(_main, os_app_exit, app, APPLICATION_STACK_SIZE_WORDS * sizeof(uint32_t));
	lf_assert(task, failure, E_NULL, "Failed to allocate memory for task");
	app->task = task;

	lf_ll_append(&apps, app, free);

	/* Add the task. */
	os_task_add(task);
	/* Launch the task. */
	os_task_next();
	return lf_success;
failure:
	if (app) free(app);
	if (task) os_task_release(task);
	return lf_error;
}

/* Releases a previously loaded module. */
int os_release_module(struct _user_module *module) {
	printf("Releasing module\n");
	/* Ensure the module pointer is valid. */
	if (!module) {
		return lf_error;
	}
	/* Free the module's base pointer. */
	if (module->base) {
		free(module->base);
	}
	memset(module, 0, sizeof(struct _user_module));
	return lf_success;
}

/* Loads a module into RAM. */
int os_load_module(void *base, struct _lf_abi_header *header) {
	/* If there are no more user module slots to allocate, return with error. */
	if (user_modules.count >= MAX_USER_MODULES) {
		lf_error_raise(E_UNIMPLEMENTED, NULL);
		return lf_error;
	} else {
		/* Obtain the module's identifier from its name. */
		char *name = base + header->name_offset;
		lf_crc_t id = lf_crc(name, header->name_size);
		int index = os_get_module_index(id);
		if (index == lf_error) {
			index = user_modules.count++;
		} else {
			os_release_module(&user_modules.modules[index]);
		}
		struct _user_module *module = &user_modules.modules[index];
		module->identifier = id;
		/* Save the module struct. */
		module->functions = base + header->module_offset;
		/* Store the number of functions that exist within the module. */
		module->func_c = (header->module_size / sizeof(uintptr_t));
		/* Save the base address of the module. */
		module->base = base;
		/* Send the index back to the host. */
		return index;
	}
}

int os_get_module_index(lf_crc_t identifier) {
	for (int i = 0; i < user_modules.count; i ++) {
		if (user_modules.modules[i].identifier == identifier) {
			return i;
		}
	}
	return lf_error;
}

/* Loads an image into RAM. */
int os_load_image(void *base) {
	/* Cast the base pointer to obtain the ABI header. */
	struct _lf_abi_header *header = base;

	/* Patch the function pointers in the module structure. */
	void **_functions = base + header->module_offset;
	for (uint32_t i = 0; i < header->module_size / sizeof(uintptr_t); i ++) {
		_functions[i] += (uintptr_t)base;
	}

	/* Patch the Global Offset Table of the image. */
	uintptr_t *got = base + header->got_offset;
	for (uint32_t i = 0; i < header->got_size / sizeof(uintptr_t); i ++) {
		got[i] += (uintptr_t)base;
	}

	/* Zero the BSS. */
	uint32_t *bss = base + header->got_offset;
	for (uint32_t i = 0; i < header->bss_size; i ++) {
		bss[i] = 0;
	}

	int retval;

	if (header->entry) {
		/* If the image has an entry point, load it as an application. */
		if ((retval = os_load_application(base)) != lf_success) {
			goto failure;
		}
	} else {
		/* If not, load the image as a module. */
		if ((retval = os_load_module(base, header)) != lf_success) {
			goto failure;
		}
	}

	return retval;
failure:
	/* Free the memory allocated to load the image. */
	free(base);
	return lf_error;
}

/* Handles the invocation of user functions. */
int fmr_perform_user_invocation(struct _fmr_invocation *invocation, struct _fmr_result *result) {
	/* Ensure that the index is within bounds. */
	if (invocation->index >= user_modules.count) {
		return lf_error;
	}
	/* Get a pointer to the module. */
	struct _user_module *module = &user_modules.modules[invocation->index];
	/* Ensure that the function is within bounds. */
	if (invocation->function >= module->func_c) {
		return lf_error;
	}
	/* Dereference a pointer to the target function. */
	const void *address = module->functions[invocation->function];
	/* Ensure that the function address is valid. */
	if (!address) {
		lf_error_raise(E_RESOULTION, NULL);
		return lf_error;
	}
	/* Perform the function call internally. */
	result->value = fmr_call(address, invocation->ret, invocation->argc, invocation->types, invocation->parameters);
	return result->error = lf_error_get();
}
