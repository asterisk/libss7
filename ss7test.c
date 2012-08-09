/*
 * libss7: An implementation of Signalling System 7
 *
 * Written by Matthew Fredrickson <creslin@digium.com>
 *
 * scheduling routines taken from libpri by Mark Spencer <markster@digium.com>
 *
 * Copyright (C) 2006-2008, Digium, Inc
 * All Rights Reserved.
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 *
 * In addition, when this program is distributed with Asterisk in
 * any form that would qualify as a 'combined work' or as a
 * 'derivative work' (but not mere aggregation), you can redistribute
 * and/or modify the combination under the terms of the license
 * provided with that copy of Asterisk, instead of the license
 * terms granted here.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <dahdi/user.h>
#include "libss7.h"

struct linkset {
	struct ss7 *ss7;
	int linkno;
	int fd;
} linkset[2];

int linknum = 1;

#define NUM_BUFS 32

static void *ss7_run(void *data)
{
	int res = 0;
	struct timeval *next = NULL, tv;
	struct linkset *linkset = (struct linkset *) data;
	struct ss7 *ss7 = linkset->ss7;
	int ourlink = linknum;
	ss7_event *e = NULL;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;

	printf("Starting link %d\n", linknum++);
	ss7_start(ss7);

	while(1) {
		if ((next = ss7_schedule_next(ss7))) {
			gettimeofday(&tv, NULL);
			tv.tv_sec = next->tv_sec - tv.tv_sec;
			tv.tv_usec = next->tv_usec - tv.tv_usec;
			if (tv.tv_usec < 0) {
				tv.tv_usec += 1000000;
				tv.tv_sec -= 1;
			}
			if (tv.tv_sec < 0) {
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			}
		}
		FD_ZERO(&rfds);
		FD_SET(linkset->fd, &rfds);
		FD_ZERO(&wfds);
		FD_SET(linkset->fd, &wfds);
		FD_ZERO(&efds);
		FD_SET(linkset->fd, &efds);
		res = select(linkset->fd + 1, &rfds, &wfds, &efds, next ? &tv : NULL);
		if (res < 0) {
			printf("next->tv_sec = %d\n", (int) next->tv_sec);
			printf("next->tv_usec = %d\n", (int) next->tv_usec);
			printf("tv->tv_sec = %d\n", (int) tv.tv_sec);
			printf("tv->tv_usec = %d\n", (int) tv.tv_usec);
			perror("select");
		}
		else if (!res)
			ss7_schedule_run(ss7);

#if LINUX
		if (FD_ISSET(linkset->fd, &efds)) {
			int x;
			if (ioctl(linkset->fd, DAHDI_GETEVENT, &x)) {
				perror("Error in exception retrieval!\n");
				exit(-1);
			}
			printf("Got exception %d!\n", x);
		}
#endif

		if (FD_ISSET(linkset->fd, &rfds))
			res = ss7_read(ss7, linkset->fd);
		if (FD_ISSET(linkset->fd, &wfds)) {
			res = ss7_write(ss7, linkset->fd);
			if (res < 0) {
				perror("Error in write");
			}
		}

		if (res < 0)
			exit(-1);

		while ((e = ss7_check_event(ss7))) {
			if (e) {
				switch (e->e) {
					case SS7_EVENT_UP:
						printf("[%d] --- SS7 Up ---\n", linkset->linkno);
						break;
					case MTP2_LINK_UP:
						printf("[%d] MTP2 link up\n", linkset->linkno);
						break;
					default:
						printf("Unknown event %d\n", e->e);
						break;
				}
			}
		}

		if (ourlink == 1) {
			/* Our demo call */
		}
	}
}

static void myprintf(struct ss7 *ss7, char *fmt)
{
	int i = 0;
	for (i = 0; i < 2; i++) {
		if (linkset[i].ss7 == ss7)
			break;
	}
	if (i == 0)
		printf("SS7[%d] %s", i, fmt);
	else
		printf("\t\t\t\t\tSS7[%d] %s", i, fmt);
}

#ifdef LINUX
static int dahdi_open(int devnum)
{
	int fd;
	struct dahdi_bufferinfo bi;
	fd = open("/dev/dahdi/channel", O_RDWR|O_NONBLOCK, 0600);
	if ((fd < 0) || (ioctl(fd, DAHDI_SPECIFY, &devnum) == -1)) {
		printf("Could not open device %d: %s\n", strerror(errno));
		return -1;
	}
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.numbufs = NUM_BUFS;
	bi.bufsize = 512;
	if (ioctl(fd, DAHDI_SET_BUFINFO, &bi)) {
		close(fd);
		return -1;
	}
	return fd;
}
#endif

int main(int argc, char *argv[])
{
	int fds[2];
	struct ss7 *ss7;
	pthread_t tmp, tmp2;

	if (argc == 2) {
		if (!strcasecmp(argv[1], "socketpair")) {
			if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, fds)) {
				perror("socketpair");
				exit(1);
			}
#ifdef LINUX
		} else if (!strcasecmp(argv[1], "live")) {
			fds[0] = dahdi_open(24);
			if (fds[0] < 0) {
				return -1;
			}

			fds[1] = dahdi_open(48);
			if (fds[1] < 0) {
				return -1;
			}
#endif
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	if (!(ss7 = ss7_new(SS7_ITU))) {
		perror("ss7_new");
		exit(1);
	}
	linkset[0].ss7 = ss7;
	linkset[0].fd = fds[0];
	linkset[0].linkno = 0;

	ss7_set_message(myprintf);
	ss7_set_error(myprintf);

	ss7_set_debug(ss7, 0xffffffff);
	if ((ss7_add_link(ss7, SS7_TRANSPORT_DAHDIDCHAN, fds[0], -1, 0))) {
		perror("ss7_add_link");
		exit(1);
	}

	if (pthread_create(&tmp, NULL, ss7_run, &linkset[0])) {
		perror("thread(0)");
		exit(1);
	}

	if (!(ss7 = ss7_new(SS7_ITU))) {
		perror("ss7_new");
		exit(1);
	}
	ss7_set_debug(ss7, 0xffffffff);
	linkset[1].linkno = 1;

	if ((ss7_add_link(ss7, SS7_TRANSPORT_DAHDIDCHAN, fds[1], -1, 1))) {
		perror("ss7_add_link");
		exit(1);
	}

	linkset[1].ss7 = ss7;
	linkset[1].fd = fds[1];

	if (pthread_create(&tmp2, NULL, ss7_run, &linkset[1])) {
		perror("thread(0)");
		exit(1);
	}


	pthread_join(tmp, NULL);
	pthread_join(tmp2, NULL);

	return 0;
}
