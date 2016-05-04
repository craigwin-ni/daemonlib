/*
 * daemonlib
 * Copyright (C) 2012-2014, 2016 Matthias Bolte <matthias@tinkerforge.com>
 *
 * daemon.c: Daemon implementation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>

#include "daemon.h"

#include "log.h"
#include "pid_file.h"
#include "utils.h"

int daemon_start(const char *log_filename, File *log_file,
                 const char *pid_filename, bool double_fork) {
	int status_pipe[2];
	pid_t pid;
	int rc;
	int pid_fd = -1;
	uint8_t success = 0;
	bool log_file_created = false;
	IO *previous_log_output = NULL;
	int stdin_fd = -1;
	int stdout_fd = -1;

	if (double_fork) {
		// create status pipe
		if (pipe(status_pipe) < 0) {
			fprintf(stderr, "Could not create status pipe: %s (%d)\n",
			        get_errno_name(errno), errno);

			return -1;
		}

		// first fork
		pid = fork();

		if (pid < 0) { // error
			fprintf(stderr, "Could not fork first child process: %s (%d)\n",
			        get_errno_name(errno), errno);

			close(status_pipe[0]);
			close(status_pipe[1]);

			return -1;
		}

		if (pid > 0) { // first parent
			close(status_pipe[1]);

			// wait for first child to exit
			while (waitpid(pid, NULL, 0) < 0 && errno_interrupted());

			// wait for second child to start successfully
			if (robust_read(status_pipe[0], &success, sizeof(success)) < 0) {
				fprintf(stderr, "Could not read from status pipe: %s (%d)\n",
				        get_errno_name(errno), errno);
			}

			close(status_pipe[0]);

			// exit first parent
			exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
		}

		// first child, decouple from parent environment
		close(status_pipe[0]);

		if (chdir("/") < 0) {
			fprintf(stderr, "Could not change directory to '/': %s (%d)\n",
			        get_errno_name(errno), errno);

			close(status_pipe[1]);

			exit(EXIT_FAILURE);
		}

		if (setsid() == (pid_t)-1) {
			fprintf(stderr, "Could not create new session: %s (%d)\n",
			        get_errno_name(errno), errno);

			close(status_pipe[1]);

			exit(EXIT_FAILURE);
		}

		umask(0);

		// second fork
		pid = fork();

		if (pid < 0) {
			fprintf(stderr, "Could not fork second child process: %s (%d)\n",
			        get_errno_name(errno), errno);

			close(status_pipe[1]);

			exit(EXIT_FAILURE);
		}

		if (pid > 0) {
			// exit second parent
			exit(EXIT_SUCCESS);
		}

		// continue as second child
	}

	// write pid
	pid_fd = pid_file_acquire(pid_filename, getpid());

	if (pid_fd < 0) {
		if (pid_fd == PID_FILE_ALREADY_ACQUIRED) {
			fprintf(stderr, "Already running according to '%s'\n", pid_filename);
		}

		goto cleanup;
	}

	// open log file
	if (file_create(log_file, log_filename,
	                O_CREAT | O_WRONLY | O_APPEND, 0644) < 0) {
		fprintf(stderr, "Could not open log file '%s': %s (%d)\n",
		        log_filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	log_file_created = true;
	previous_log_output = log_get_output();

	log_set_output(&log_file->base);

	// redirect standard file descriptors
	stdin_fd = open("/dev/null", O_RDONLY);

	if (stdin_fd < 0) {
		fprintf(stderr, "Could not open /dev/null to redirect stdin to: %s (%d)\n",
		        get_errno_name(errno), errno);

		goto cleanup;
	}

	stdout_fd = log_file->base.handle;

	if (dup2(stdin_fd, STDIN_FILENO) != STDIN_FILENO) {
		fprintf(stderr, "Could not redirect stdin: %s (%d)\n",
		        get_errno_name(errno), errno);

		goto cleanup;
	}

	if (dup2(stdout_fd, STDOUT_FILENO) != STDOUT_FILENO) {
		fprintf(stderr, "Could not redirect stdout: %s (%d)\n",
		        get_errno_name(errno), errno);

		goto cleanup;
	}

	if (dup2(stdout_fd, STDERR_FILENO) != STDERR_FILENO) {
		fprintf(stderr, "Could not redirect stderr: %s (%d)\n",
		        get_errno_name(errno), errno);

		goto cleanup;
	}

	success = 1;

cleanup:
	if (stdin_fd >= 0) {
		close(stdin_fd);
	}

	if (double_fork) {
		do {
			rc = write(status_pipe[1], &success, sizeof(success));
		} while (rc < 0 && errno == EINTR);

		if (rc < 0) {
			fprintf(stderr, "Could not write to status pipe: %s (%d)",
			        get_errno_name(errno), errno);
		}

		close(status_pipe[1]);
	}

	if (success == 0) {
		if (log_file_created) {
			log_set_output(previous_log_output);
			file_destroy(log_file);
		}

		if (pid_fd >= 0) {
			close(pid_fd);
		}

		return -1;
	}

	return pid_fd;
}
