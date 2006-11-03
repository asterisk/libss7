/*
libss7: An implementation of Signaling System 7

Written by Matthew Fredrickson <creslin@digium.com>

Copyright Digium, Inc. (C), 2006
All Rights Reserved.

This program is free software; you can redistribute it under the
terms of the GNU General Public License as published by the Free
Software Foundation

*/
#ifndef _SS7_H
#define _SS7_H

#include <sys/time.h>
#include <stdio.h>
#include "libss7.h"
#include "mtp2.h"
#include "mtp3.h"

/* ISUP parameters */

/* Information Transfer Capability */
#define ISUP_TRANSCAP_SPEECH 0x00
#define ISUP_TRANSCAP_UNRESTRICTED_DIGITAL 0x08
#define ISUP_TRANSCAP_RESTRICTED_DIGITAL 0x09
#define ISUP_TRANSCAP_31KHZ_AUDIO 0x10
#define ISUP_TRANSCAP_7KHZ_AUDIO 0x11

/* User Information layer 1 protocol types */
#define ISUP_L1PROT_G711ULAW 0x02

#define MAX_EVENTS		16
#define MAX_SCHED		64
#define SS7_MAX_LINKS		4

#define SS7_STATE_DOWN	0
#define SS7_STATE_UP 1

struct ss7_msg {
	unsigned char buf[MTP_MAX_SIZE];
	unsigned int size;
	struct ss7_msg *next;
};

struct ss7_sched {
	struct timeval when;
	void (*callback)(void *data);
	void *data;
};

struct ss7 {
	unsigned int switchtype;
	unsigned int numlinks;

	/* Simple "routing table" */
	point_code def_dpc;
	/* Our point code */
	point_code pc;

	unsigned char ni;
	unsigned char sls;
	int state;

	unsigned int debug;
	/* event queue */
	int ev_h;
	int ev_t;
	int ev_len;
	ss7_event ev_q[MAX_EVENTS];

	struct ss7_sched ss7_sched[MAX_SCHED];
	struct isup_call *calls;

	unsigned int mtp2_linkstate[SS7_MAX_LINKS];
	struct mtp2 *links[SS7_MAX_LINKS];
};

/* Getto hacks for developmental purposes */
struct ss7_msg * ss7_msg_new(void);

void ss7_msg_free(struct ss7_msg *m);

/* Scheduler functions */
int ss7_schedule_event(struct ss7 *ss7, int ms, void (*function)(void *data), void *data);

ss7_event * ss7_next_empty_event(struct ss7 * ss7);

void ss7_schedule_del(struct ss7 *ss7,int *id);

unsigned char *ss7_msg_userpart(struct ss7_msg *m);

void ss7_msg_userpart_len(struct ss7_msg *m, int len);

void ss7_message(struct ss7 *ss7, char *fmt, ...);

void ss7_error(struct ss7 *ss7, char *fmt, ...);

void ss7_dump_buf(struct ss7 *ss7, unsigned char *buf, int len);

#endif /* _SS7_H */
