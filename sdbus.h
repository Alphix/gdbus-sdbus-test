#ifndef fooutilshfoo
#define fooutilshfoo

#include <systemd/sd-bus.h>
#include <stdbool.h>
#include <stdlib.h>

static inline void* zmalloc(size_t size)
{
	return calloc(1, size);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define _cleanup_(x) __attribute__((cleanup(x)))

#define DEFINE_TRIVIAL_CLEANUP_FUNC(type, func)				\
	static inline void func##p(type *p) {				\
		if (*p)							\
			func(*p);					\
	}								\
	struct __useless_struct_to_allow_trailing_semicolon__

static inline void strv_free(char **l) {
        char **k;
        if (l) {
                for (k = l; *k; k++)
                        free(k);
                free(l);
        }
}
DEFINE_TRIVIAL_CLEANUP_FUNC(char **, strv_free);
#define _cleanup_strv_free_ _cleanup_(strv_freep)

static inline void freep(void *p) {
	free(*(void**) p);
}
#define _cleanup_free_ _cleanup_(freep)

#define _cleanup_bus_flush_close_unref_ _cleanup_(sd_bus_flush_close_unrefp)

#define _cleanup_bus_slot_unref_ _cleanup_(sd_bus_slot_unrefp)

#define _cleanup_bus_message_unref_ _cleanup_(sd_bus_message_unrefp)

#define _cleanup_event_source_unref_ _cleanup_(sd_event_source_unrefp)

#define _cleanup_bus_error_free_ _cleanup_(sd_bus_error_free)

#endif

