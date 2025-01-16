#include <stdio.h>
#include <time.h>

extern "C" {
extern time_t __real_time(time_t *__timer);
time_t __wrap_time(time_t *__timer) {
    static auto relative = __real_time(NULL);
    return __real_time(__timer) - relative;
//    return __real_time(__timer) + 5259486;
}
}
