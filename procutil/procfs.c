#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include "process/process.h"
#include "process/thread.h"
#include "process/segment.h"
#include "procfs.h"

/* This is defined in procfs_maps.c */
void procfs_maps_parse_segment(char * buf, segment_t * segment);

/* Get the maximum PID value. */
static pid_t procfs_get_pid_max() {
    char buf[16];
    FILE * file = fopen("/proc/sys/kernel/pid_max", "r");
    assert(file);
    if (!fgets(buf, 16, file))
        assert(0);
    fclose(file);
    return (pid_t) atol(buf);
}

static const char * procfs_pid_get_path(pid_t pid, pid_t tid, const char * entryname) {
    static __thread char path[256];
    int ret;
    if (tid == 0)
        ret = snprintf(path, 256, "/proc/%d/%s", pid, entryname);
    else
        ret = snprintf(path, 256, "/proc/%d/task/%d/%s", pid, tid, entryname);
    assert((ret < 256) && (ret > 0));
    return path;
}

/* Get the main executable path of the given pid-tid. */
const char * procfs_get_exe(pid_t pid, pid_t tid) {
    static __thread char filename[PATH_MAX];
    
    const char * path = procfs_pid_get_path(pid, tid, "exe");
    
    int nchars = readlink(path, filename, PATH_MAX);
    if (nchars < 0) {
        error("Can't read link %s: %s.", path, strerror(errno));
        return NULL;
    } else {
        if (PATH_MAX <= nchars)
            nchars = PATH_MAX - 1;
        filename[nchars] = '\0';
    }
    
    return filename;
}

/* Return a string representing the path to an entry in the proc file system for the given thread. The result string
 * does not need to be freed. */
static const char * procfs_thread_get_path(const thread_t * thread, const char * entryname) {
    return procfs_pid_get_path(thread->process->pid, thread->pid, entryname);
}

/* Get the value of the given field from a /proc/.../status file. */
static char * get_status_field(pid_t pid, pid_t tid, const char * field) {
    static __thread char result[256];
    FILE * file = NULL;
    const char * path = procfs_pid_get_path(pid, tid, "status");
    char buf[256];
    size_t field_len = strlen(field);
    
    result[0] = '\0';
    
    if (!(file = fopen(path, "r"))) {
        error("Error opening %s: %s.", path, strerror(errno));
        goto out;
    }
    
    while (fgets(buf, sizeof(buf), file)) {
        if ((strncmp(buf, field, field_len) == 0) && (buf[field_len] == ':')) {
            char * t;
            
            /* Found the field, skip whitespace. */
            for (t = buf + field_len + 1; (*t) && isspace(*t); ++t);
            
            /* Copy to result */
            strncpy(result, t, 256);
            
            /* Convert the trailing new line to a null char. */
            for (t = result; *t; ++t)
                if (*t == '\n')
                    *t = '\0';
                    
            debug("Thread %d:%d has status field '%s' == '%s'.", pid, tid, field, result);
            goto out;
        }
    }
    
    error("Thread %d:%d has no status field '%s'.", pid, tid, field);
    
out:
    if (file)
        fclose(file);
    return result;
}

/* Get the value of the given field from a /proc/.../status file and convert it to an unsigned int.
 * Return success flag.*/
static bool get_status_field_uint(pid_t pid, pid_t tid, const char * field, unsigned int * result) {
    char * str;
    long int val;
    int success;
    
    if (!(str = get_status_field(pid, tid, field))) {
        *result = 0;
        return 0;
    }
    
    val = strtol(str, NULL, 0);
    
    success = (val >= 0) && ((unsigned long) val <= UINT_MAX);
    
    if (success)
        *result = (unsigned int) val;
    else {
        error("Value of '%s' of thread %d:%d is out of range.", field, pid, tid);
        *result = 0;
    }
    
    return success;
}

/**********************************************************************************************************************/

/* Get the TGID of the given process. */
pid_t procfs_get_tgid(pid_t pid) {
    unsigned int val;
    get_status_field_uint(pid, 0, "Tgid", &val);
    return val;
}

/* Get the parent PID of the given process. */
pid_t procfs_get_ppid(pid_t pid) {
    unsigned int val;
    get_status_field_uint(pid, 0, "PPid", &val);
    return val;
}

/* Get the PID of the process, which is currently tracing the given process or thread. */
pid_t procfs_get_tracerpid(pid_t pid) {
    unsigned int val;
    get_status_field_uint(pid, 0, "TracerPid", &val);
    return val;
}

/**********************************************************************************************************************/

/* Get the process status char of the given process (e.g. 'R' for Running, 'T' for stopped, 'Z' for zombie, etc). On
 * failure return '?'. */
char procfs_pid_state(pid_t pid, pid_t tid) {
    char * val = get_status_field(pid, tid, "State");
    
    if (val) {
        return val[0];
        free(val);
    } else
        return '?';
}

/**********************************************************************************************************************/

/* Call fn for each running thread of the given process. Return success flag (success is when the function iterates
 * over all entries). */
bool procfs_iter_threads(pid_t pid, void (fn)(pid_t tid)) {
    const char * dirpath = procfs_pid_get_path(pid, 0, "task");
    DIR * dir = NULL;
    bool ret = false;
    struct dirent * direntry;
    pid_t max_pid = procfs_get_pid_max();
    
    if (!(dir = opendir(dirpath))) {
        error("Error opening %s directory.", dirpath);
        goto out;
    }
    
    while ((direntry = readdir(dir))) {
        long new_pid;
        
        if (direntry->d_name[0] == '.') {
            continue;
        }
        
        /* Try to convert to pid_t. */
        new_pid = (pid_t) atol(direntry->d_name);
        
        if ((new_pid <= 1) || (pid >= max_pid)) {
            warning("Warning: Found invalid PID '%s' in %s directory. Ignoring.", direntry->d_name, dirpath);
            continue;
        }
        
        /* Report. */
        fn((pid_t) new_pid);
    }
    
    ret = true;
    
out:
    if (dir)
        closedir(dir);
    return ret;
}

/* Iterate over all segments of a process (thread) and call fn for all detected segments. Return success flag. */
bool procfs_iter_segments(const thread_t * thread, void (fn)(const segment_t * segment)) {
    const char * path = procfs_thread_get_path(thread, "maps");
    FILE * file = NULL;
    
    char buf[MAX_PATH * 2];
    bool result = false;
    
    if (!(file = fopen(path, "r"))) {
        error("Error opening '%s': %s.", path, strerror(errno));
        goto out;
    }
    
    while (fgets(buf, sizeof(buf), file)) {
        segment_t segment;
        if ((*buf == '\0') || (*buf == '\n')) {
            /* This is an empty line */
            continue;
        }
        procfs_maps_parse_segment(buf, &segment);

        if (segment.start == segment.end)
            /* Ignore segments with zero size */
            continue;

        fn(&segment);
    }
    
    result = true;
    
out:
    if (file)
        fclose(file);
    return result;
}

/**********************************************************************************************************************/

/* Check if the given address in the thread is executable. */
bool procfs_address_executable(const thread_t * thread, address_t address) {
    bool result = false;
    void callback(const segment_t * segment) {
        result |= ((segment->start <= address) && (segment->end >= address) && segment_is_executable(segment));
    }
    procfs_iter_segments(thread, callback);
    return result;
}

/**********************************************************************************************************************/

/* Read at most size bytes from the memory of the given thread using the /proc/.../mem entry starting at the given
 * address.  Write results to out. Out must be large enough to hold size bytes. Return bytes written (which is size
 * or less). */
size_t procfs_mem_read(thread_t * thread, address_t offset, size_t size, void * out) {

    const char * path = procfs_thread_get_path(thread, "mem");
    int fd = -1;
    ssize_t count = 0;
    
    if ((fd = open64(path, O_RDONLY)) == -1) {
        error("Error opening %s.", path);
        goto out;
    }
    
    if (lseek64(fd, (off64_t) offset, SEEK_SET) == -1) {
        error("Error seeking through %s.", path);
        goto out;
    }
    
    count = read(fd, out, size);
    
    if (count != (ssize_t) size)
        error("Error reading %zu bytes from %s at %lx: %s", size, path, offset, strerror(errno));

out:
    if (fd >= 0)
        close(fd);
    if (count >= 0)
        return (size_t) count;
    else
        return 0;
}
