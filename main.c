/*
 * llmnrd -- LLMNR (RFC 4705) responder daemon.
 *
 * Copyright (C) 2014-2015 Tobias Klauser <tklauser@distanz.ch>
 *
 * This file is part of llmnrd.
 *
 * llmnrd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * llmnrd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with llmnrd.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "compiler.h"
#include "log.h"
#include "util.h"

#include "iface.h"
#include "llmnr.h"

static const char *short_opts = "H:p:dh";
static const struct option long_opts[] = {
	{ "hostname",	required_argument,	NULL, 'H' },
	{ "port",	required_argument,	NULL, 'p' },
	{ "daemonize",	no_argument,		NULL, 'd' },
	{ "help",	no_argument,		NULL, 'h' },
	{ NULL,		0,			NULL, 0 },
};

static void __noreturn usage_and_exit(int status)
{
	fprintf(stdout, "Usage: llmnrd [OPTIONS]\n");
	exit(status);
}

static void signal_handler(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		log_info("Interrupt received. Stopping llmnrd.\n");
		iface_stop();
		llmnr_stop();
		break;
	case SIGHUP:
	default:
		/* ignore */
		break;
	}
}

static void register_signal(int sig, void (*handler)(int))
{
	sigset_t block_mask;
	struct sigaction saction;

	sigfillset(&block_mask);

	saction.sa_handler = handler;
	saction.sa_mask = block_mask;

	if (sigaction(sig, &saction, NULL) != 0) {
		log_err("Failed to register signal handler for %s (%d)\n",
			strsignal(sig), sig);
	}
}

int main(int argc, char **argv)
{
	int c, ret = EXIT_FAILURE;
	long num_arg;
	bool daemonize = false;
	char *hostname = "";
	uint16_t port = 0;

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
		case 'd':
			daemonize = true;
			break;
		case 'H':
			hostname = xstrdup(optarg);
			break;
		case 'p':
			num_arg = strtol(optarg, NULL, 0);
			if (num_arg < 0 || num_arg > UINT16_MAX) {
				log_err("Invalid port number: %ld\n", num_arg);
				return EXIT_FAILURE;
			}
			port = num_arg;
		case 'h':
			ret = EXIT_SUCCESS;
			/* fall through */
		default:
			usage_and_exit(ret);
		}
	}

	register_signal(SIGINT, signal_handler);
	register_signal(SIGQUIT, signal_handler);
	register_signal(SIGTERM, signal_handler);
	register_signal(SIGHUP, signal_handler);

	if (hostname[0] == '\0') {
		/* TODO: Consider hostname changing at runtime */
		hostname = xmalloc(255);
		if (gethostname(hostname, 255) != 0) {
			log_err("Failed to get hostname");
			return EXIT_FAILURE;
		}
	}

	if (daemonize) {
		/* TODO */
	}

	if (iface_start_thread() < 0)
		goto out;

	ret = llmnr_run(hostname, port);
out:
	free(hostname);
	return ret;
}
