#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "capabilities.h"

#ifdef USE_CAPABILITIES

#include <sys/capability.h>

/* Retrieve the capabilities of the given process. Return non-zero on success. */
static int caps_get(pid_t pid, cap_user_data_t caps) {

    struct __user_cap_header_struct cap_header;
    
    cap_header.pid = pid;
    cap_header.version = _LINUX_CAPABILITY_VERSION;
    
    if (capget(&cap_header, caps) < 0) {
        error("Error reading capabilities of process %d.", (int) pid);
        return 0;
    }
    
    return 1;
}

/* Retrieve the capabilities of the current process. Returns non-zero on
 * success. */
static int caps_get_current(cap_user_data_t caps) {
    return caps_get(getpid(), caps);
}

/* Return true if the current process has the CAP_SYS_PTRACE capability. */
static int caps_can_ptrace() {
    int res;
    
    struct __user_cap_data_struct caps;
    if (!caps_get_current(&caps)) {
        return 0;
    }
    
    res = (caps.effective & (1 << CAP_SYS_PTRACE))
    
          debug("We have %spermission to use ptrace.", res ? "", "no ");
    return res;
}

#else

static int caps_can_ptrace() {
    static int warned = 0;

    if (!warned) {
        warning("Warning: Capability checking is disabled. Assuming that we "
                "have all possible permissions.");
        warned = 1;
    }

    return 1;
}

#endif

int caps_init() {
    if (caps_can_ptrace())
        return 1;
    else {
        fatal("Insufficient permissions to use ptrace (CAP_SYS_PTRACE capability missing).");
        return 0;
    }
}
