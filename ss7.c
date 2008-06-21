/*
libss7: An implementation of Signalling System 7

Written by Matthew Fredrickson <creslin@digium.com>

Copyright Digium, Inc (C), 2006
All Rights Reserved.

This program is free software; you can redistribute it under the
terms of the GNU General Public License as published by the Free
Software Foundation

Contains user interface to ss7 library
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#if 0
#include <zaptel/zaptel.h>
#endif
#include <sys/ioctl.h>
#include <sys/poll.h>
#include "libss7.h"
#include "ss7_internal.h"
#include "mtp2.h"
#include "isup.h"
#include "mtp3.h"


static void (*__ss7_message)(struct ss7 *ss7, char *message);
static void (*__ss7_error)(struct ss7 *ss7, char *message);

void ss7_set_message(void (*func)(struct ss7 *ss7, char *message))
{
	__ss7_message = func;
}

void ss7_set_error(void (*func)(struct ss7 *ss7, char *message))
{
	__ss7_error = func;
}

void ss7_message(struct ss7 *ss7, char *fmt, ...)
{
	char tmp[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (__ss7_message)
		__ss7_message(ss7, tmp);
	else
		fputs(tmp, stdout);
}

void ss7_error(struct ss7 *ss7, char *fmt, ...)
{
	char tmp[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (__ss7_error)
		__ss7_error(ss7, tmp);
	else
		fputs(tmp, stdout);
}

void ss7_set_debug(struct ss7 *ss7, unsigned int flags)
{
	ss7->debug = flags;
}

void ss7_dump_buf(struct ss7 *ss7, int tabs, unsigned char *buf, int len)
{
	int i, j = 0;
	char tmp[1024];

	for (i = 0; i < tabs; i++)
		snprintf(&tmp[i], sizeof(tmp)-i, "\t");
	snprintf(&tmp[i], sizeof(tmp)-i, "[ ");
	j = i + 2;                                                            /* some TAB + "[ " */
	for (i = 0; i < len; i++) {
		snprintf(&tmp[3*i]+j, sizeof(tmp)-3*i-j, "%02x ", buf[i]);    /* &tmp[3*i]+j - for speed optimization, don't change format! */
	}
	ss7_message(ss7, "%s]\n", tmp);
}

void ss7_dump_msg(struct ss7 *ss7, unsigned char *buf, int len)
{
	int i;
	char tmp[1024];

	for (i = 0; i < len; i++) {
		snprintf(&tmp[3*i], sizeof(tmp)-3*i, "%02x ", buf[i]);        /* &tmp[3*i] - for speed optimization, don't change format! */
	}
	ss7_message(ss7, "Len = %d [ %s]\n", len, tmp);
}

void ss7_msg_free(struct ss7_msg *m)
{
	free(m);
}

struct ss7_msg * ss7_msg_new(void)
{
	return calloc(1, sizeof(struct ss7_msg));
}

unsigned char * ss7_msg_userpart(struct ss7_msg *msg)
{
	return msg->buf + MTP2_SIZE + SIO_SIZE;
}

void ss7_msg_userpart_len(struct ss7_msg *msg, int len)
{
	msg->size = MTP2_SIZE + SIO_SIZE + len;
	return;
}

ss7_event * ss7_next_empty_event(struct ss7 *ss7)
{
	ss7_event *e;

	if (ss7->ev_len == MAX_EVENTS) {
		ss7_error(ss7, "Event queue full!\n");
		return NULL;
	}

	e = &ss7->ev_q[(ss7->ev_h + ss7->ev_len) % MAX_EVENTS];
	ss7->ev_len += 1;

	return e;
}

ss7_event * ss7_check_event(struct ss7 *ss7)
{
	ss7_event *e;

	if (!ss7->ev_len)
		return NULL;
	else
		e = &ss7->ev_q[ss7->ev_h];
	ss7->ev_h += 1;
	ss7->ev_h %= MAX_EVENTS;
	ss7->ev_len -= 1;

	return mtp3_process_event(ss7, e);
}		

int  ss7_start(struct ss7 *ss7)
{
	mtp3_start(ss7);
	return 0;
}

void ss7_link_alarm(struct ss7 *ss7, int fd)
{
	mtp3_alarm(ss7, fd);
}

void ss7_link_noalarm(struct ss7 *ss7, int fd)
{
	mtp3_noalarm(ss7, fd);
}

int ss7_add_link(struct ss7 *ss7, int transport, int fd)
{
	struct mtp2 *m;

	if (ss7->numlinks >= SS7_MAX_LINKS)
		return -1;

	if (transport == SS7_TRANSPORT_TCP) {
	}

	if (transport == SS7_TRANSPORT_ZAP) {
		int zapmtp2 = 0;
#ifdef ZT_SIG_MTP2
		struct zt_params z;
		int res;

		res = ioctl(fd, ZT_GET_PARAMS, &z);
		if (res)
			return res;

		if (z.sigtype == ZT_SIG_MTP2) {
			printf("Found zapmtp2\n");
			zapmtp2 = 1;
		}

#endif /* ZT_SIG_MTP2 */
		m = mtp2_new(fd, ss7->switchtype);
		
		if (!m)
			return -1;

		m->slc = ss7->numlinks;
		ss7->numlinks += 1;
		m->master = ss7;
		if (zapmtp2)
			m->flags |= MTP2_FLAG_ZAPMTP2;

		ss7->links[ss7->numlinks - 1] = m;
	}

	return 0;
}

int ss7_pollflags(struct ss7 *ss7, int fd)
{
	int i;
	int winner = -1;
	int flags = POLLPRI | POLLIN;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd) {
			winner = i;
			break;
		}
	}
	
	if (winner < 0)
		return -1;

	if (ss7->links[winner]->flags & MTP2_FLAG_ZAPMTP2) {
		if (ss7->links[winner]->flags & MTP2_FLAG_WRITE)
			flags |= POLLOUT;
	} else
		flags |= POLLOUT;

	return flags;
}

/* TODO: Add entry to routing table instead */
int ss7_set_adjpc(struct ss7 *ss7, int fd, unsigned int pc)
{
	int i;
	int winner = -1;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd)
			winner = i;
	}
	if (winner > -1)
		ss7->links[winner]->dpc = pc;
	else
		return -1;

	return 0;
}

int ss7_set_pc(struct ss7 *ss7, unsigned int pc)
{
	ss7->pc = pc;
	return 0;
}

int ss7_set_network_ind(struct ss7 *ss7, int ni)
{
	ss7->ni = ni;
	return 0;
}

char * ss7_event2str(int event)
{
	switch (event) {
		case SS7_EVENT_UP:
			return "SS7_EVENT_UP";
		case SS7_EVENT_DOWN:
			return "SS7_EVENT_DOWN";
		case ISUP_EVENT_IAM:
			return "ISUP_EVENT_IAM";
		case ISUP_EVENT_ACM:
			return "ISUP_EVENT_ACM";
		case ISUP_EVENT_ANM:
			return "ISUP_EVENT_ANM";
		case ISUP_EVENT_REL:
			return "ISUP_EVENT_REL";
		case ISUP_EVENT_RLC:
			return "ISUP_EVENT_RLC";
		case ISUP_EVENT_GRS:
			return "ISUP_EVENT_GRS";
		case ISUP_EVENT_GRA:
			return "ISUP_EVENT_GRA";
		case ISUP_EVENT_CON:
			return "ISUP_EVENT_CON";
		default:
			return "Unknown Event";
	}

}

struct ss7 *ss7_new(int switchtype)
{
	struct ss7 *s;
	s = calloc(1, sizeof(struct ss7));

	if (!s)
		return NULL;

	/* Initialize the event queue */
	s->ev_h = 0;
	s->ev_len = 0;
	s->state = SS7_STATE_DOWN;

	if ((switchtype == SS7_ITU) || (switchtype == SS7_ANSI))
		s->switchtype = switchtype;
	else {
		free(s);
		return NULL;
	}

	return s;
}

int ss7_write(struct ss7 *ss7, int fd)
{
	int res, i, winner = -1;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd) {
			winner = i;
			break;
		}
	}
	
	if (winner < 0)
		return -1;

	res = mtp2_transmit(ss7->links[winner]);

	return res;
}

int ss7_read(struct ss7 *ss7, int fd)
{
	unsigned char buf[1024];
	int res;
	int winner = -1;
	int i;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd) {
			winner = i;
			break;
		}
	}
	
	if (winner < 0)
		return -1;

	res = read(ss7->links[winner]->fd, buf, sizeof(buf));
	if (res <= 0) {
		return res;
	}

	res = mtp2_receive(ss7->links[winner], buf, res);

	return res;
}
