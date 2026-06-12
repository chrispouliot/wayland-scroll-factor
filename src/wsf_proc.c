#define _GNU_SOURCE

#include "wsf_proc.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Kernel TASK_COMM_LEN is 16 including the NUL terminator. */
#define WSF_COMM_MAX 15

static const char *wsf_basename(const char *path) {
	const char *slash = strrchr(path, '/');

	if (slash == NULL) {
		return path;
	}

	return slash + 1;
}

void wsf_proc_normalize_wrapper(char *name) {
	size_t len = 0;
	static const char suffix[] = "-wrapped";
	const size_t suffix_len = sizeof(suffix) - 1;

	if (name == NULL || name[0] != '.') {
		return;
	}

	len = strlen(name);
	if (len <= suffix_len + 1 ||
		strcmp(name + len - suffix_len, suffix) != 0) {
		return;
	}

	/* ".foo-wrapped" -> "foo" (nixpkgs makeWrapper convention) */
	memmove(name, name + 1, len - 1);
	name[len - 1 - suffix_len] = '\0';
}

static bool wsf_read_proc_file(
	const char *pid_dir,
	const char *leaf,
	char *buf,
	size_t len
) {
	char path[64];
	FILE *file = NULL;

	if (snprintf(path, sizeof(path), "/proc/%s/%s", pid_dir, leaf) >=
		(int) sizeof(path)) {
		return false;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	if (fgets(buf, (int) len, file) == NULL) {
		fclose(file);
		return false;
	}

	fclose(file);

	buf[strcspn(buf, "\n")] = '\0';
	return buf[0] != '\0';
}

static bool wsf_read_comm_for(const char *pid_dir, char *buf, size_t len) {
	return wsf_read_proc_file(pid_dir, "comm", buf, len);
}

static bool wsf_read_cmdline_name(
	const char *pid_dir,
	char *buf,
	size_t len
) {
	char path[64];
	char cmdline[256];
	const char *name = NULL;
	FILE *file = NULL;
	size_t read_len = 0;

	if (snprintf(path, sizeof(path), "/proc/%s/cmdline", pid_dir) >=
		(int) sizeof(path)) {
		return false;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	read_len = fread(cmdline, 1, sizeof(cmdline) - 1, file);
	fclose(file);

	if (read_len == 0) {
		return false;
	}

	cmdline[read_len] = '\0';
	name = wsf_basename(cmdline);
	if (name[0] == '\0') {
		return false;
	}

	strncpy(buf, name, len - 1);
	buf[len - 1] = '\0';
	return true;
}

static bool wsf_read_exe_name(const char *pid_dir, char *buf, size_t len) {
	char path[64];
	char target[512];
	const char *name = NULL;
	ssize_t link_len = 0;

	if (snprintf(path, sizeof(path), "/proc/%s/exe", pid_dir) >=
		(int) sizeof(path)) {
		return false;
	}

	link_len = readlink(path, target, sizeof(target) - 1);
	if (link_len <= 0) {
		return false;
	}
	target[link_len] = '\0';

	/* readlink reports deleted binaries as "path (deleted)". */
	if (link_len > 10 &&
		strcmp(target + link_len - 10, " (deleted)") == 0) {
		target[link_len - 10] = '\0';
	}

	name = wsf_basename(target);
	if (name[0] == '\0') {
		return false;
	}

	strncpy(buf, name, len - 1);
	buf[len - 1] = '\0';
	return true;
}

static bool wsf_comm_equals_target(const char *comm, const char *target) {
	char candidate[256];

	if (strcmp(comm, target) == 0) {
		return true;
	}

	/*
	 * comm is truncated to WSF_COMM_MAX characters by the kernel, so a
	 * long target ("gnome-shell-calendar-server") or a wrapped binary
	 * (".gnome-shell-wrapped" -> ".gnome-shell-wr") never compares equal
	 * directly. Accept comm when it equals the truncation of either the
	 * target itself or its wrapped form.
	 */
	if (strlen(comm) == WSF_COMM_MAX) {
		if (strncmp(comm, target, WSF_COMM_MAX) == 0 &&
			strlen(target) > WSF_COMM_MAX) {
			return true;
		}

		if (snprintf(candidate, sizeof(candidate), ".%s-wrapped",
			target) < (int) sizeof(candidate) &&
			strncmp(comm, candidate, WSF_COMM_MAX) == 0 &&
			strlen(candidate) > WSF_COMM_MAX) {
			return true;
		}
	}

	return false;
}

static bool wsf_name_equals_target(const char *name, const char *target) {
	char normalized[256];

	if (strcmp(name, target) == 0) {
		return true;
	}

	strncpy(normalized, name, sizeof(normalized) - 1);
	normalized[sizeof(normalized) - 1] = '\0';
	wsf_proc_normalize_wrapper(normalized);

	return strcmp(normalized, target) == 0;
}

static bool wsf_pid_dir_matches(const char *pid_dir, const char *target) {
	char name[256];

	if (target == NULL || target[0] == '\0') {
		return false;
	}

	if (wsf_read_exe_name(pid_dir, name, sizeof(name)) &&
		wsf_name_equals_target(name, target)) {
		return true;
	}

	if (wsf_read_comm_for(pid_dir, name, sizeof(name)) &&
		wsf_comm_equals_target(name, target)) {
		return true;
	}

	if (wsf_read_cmdline_name(pid_dir, name, sizeof(name)) &&
		wsf_name_equals_target(name, target)) {
		return true;
	}

	return false;
}

bool wsf_proc_name(char *buf, size_t len) {
	if (buf == NULL || len == 0) {
		return false;
	}

	buf[0] = '\0';

	if (wsf_read_exe_name("self", buf, len)) {
		wsf_proc_normalize_wrapper(buf);
		return true;
	}

	if (wsf_read_comm_for("self", buf, len)) {
		return true;
	}

	return wsf_read_cmdline_name("self", buf, len);
}

bool wsf_proc_matches(const char *target) {
	return wsf_pid_dir_matches("self", target);
}

bool wsf_proc_pid_matches(int pid, const char *target) {
	char pid_dir[32];

	if (pid <= 0) {
		return false;
	}

	if (snprintf(pid_dir, sizeof(pid_dir), "%d", pid) >=
		(int) sizeof(pid_dir)) {
		return false;
	}

	return wsf_pid_dir_matches(pid_dir, target);
}

bool wsf_proc_is_target(const char *target) {
	return wsf_proc_matches(target);
}
