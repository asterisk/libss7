/*
libss7: An implementation of Signalling System 7

Written by Matthew Fredrickson <matt@fredricknet.net>

Copyright (C), 2005
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
	ss7->debug |= flags;
}

void ss7_dump_buf(struct ss7 *ss7, unsigned char *buf, int len)
{
	int i;

	ss7_message(ss7, "Len = %d [ ", len);
	for (i = 0; i < len; i++) {
		ss7_message(ss7, "%02x ", buf[i]);
	}
	ss7_message(ss7, "]\n");
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

	if (ss7->ev_len == MAX_EVENTS)
		return NULL;

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

int ss7_start(struct ss7 *ss7)
{
	int i = 0;

	for (i = 0; i < ss7->numlinks; i++)
		mtp2_start(ss7->links[i]);

	return 0;
}

int ss7_add_link(struct ss7 *ss7, int fd)
{
	struct mtp2 *m;

	if (ss7->numlinks >= SS7_MAX_LINKS)
		return -1;

	m = mtp2_new(fd, ss7->switchtype);
	
	if (!m)
		return -1;

	m->slc = ss7->numlinks;
	ss7->numlinks += 1;
	m->master = ss7;

	ss7->links[ss7->numlinks - 1] = m;

	return 0;
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

int ss7_set_default_dpc(struct ss7 *ss7, unsigned int pc)
{
	ss7->def_dpc = pc;
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
	if (res < 0) {
		return res;
	}

	res = mtp2_receive(ss7->links[winner], buf, res);

	return res;
}
