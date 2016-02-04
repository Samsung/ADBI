#include "state.h"
#include "process/process.h"

static bool state_tracing_val = false;

bool state_tracing() {
    return state_tracing_val;
}

void state_tracing_set(bool state) {
    if (state_tracing_val == state)
        return;
    if (state) {
        process_continue_all();
    } else {
        process_stop_all();
    }
    state_tracing_val = state;
}
