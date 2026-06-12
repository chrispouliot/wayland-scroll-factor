#ifndef WSF_PROC_H
#define WSF_PROC_H

#include <stdbool.h>
#include <stddef.h>

bool wsf_proc_name(char *buf, size_t len);
bool wsf_proc_is_target(const char *target);

/*
 * Match the current process (or an arbitrary pid) against a target name,
 * checking /proc exe, comm, and cmdline. Understands distro wrapper
 * conventions (nixpkgs ".<name>-wrapped" binaries) and the kernel's
 * 15-character comm truncation.
 */
bool wsf_proc_matches(const char *target);
bool wsf_proc_pid_matches(int pid, const char *target);

/* Strip the nixpkgs wrapper convention in place: ".foo-wrapped" -> "foo". */
void wsf_proc_normalize_wrapper(char *name);

#endif
