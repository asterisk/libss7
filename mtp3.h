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

#ifndef _MTP3_H
#define _MTP3_H

#include "ss7_internal.h"

/* Service Indicator bits for Service Information Octet */
/* Bits 4-1 */
#define SIG_NET_MNG		0x00
#define SIG_STD_TEST		0x01
#define SIG_SPEC_TEST		0x02
#define SIG_SCCP		0x03
#define SIG_ISUP		0x05
/* Bits 6-5 -- ANSI networks only */
#define PRIORITY_0		0x00
#define PRIORITY_1		0x01
#define PRIORITY_2		0x02
#define PRIORITY_3		0x03

#define SIO_SIZE	1

#define MTP2_LINKSTATE_DOWN 	0
#define MTP2_LINKSTATE_INALARM	1
#define MTP2_LINKSTATE_ALIGNING	2
#define MTP2_LINKSTATE_UP	3

struct net_mng_message {
	int h0;
	int h1;
	char *name;
};

/* Process any MTP2 events that occur */
ss7_event* mtp3_process_event(struct ss7 *ss7, ss7_event *e);

/* The main receive function for MTP3 */
int mtp3_receive(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

int mtp3_dump(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

/* Transmit */
int mtp3_transmit(struct ss7 *ss7, unsigned char userpart, unsigned char sls, struct ss7_msg *m);

void mtp3_alarm(struct ss7 *ss7, int fd);

void mtp3_noalarm(struct ss7 *ss7, int fd);

void mtp3_start(struct ss7 *ss7);

unsigned char ansi_sls_next(struct ss7 *ss7);

int set_routinglabel(unsigned char *sif, struct routing_label *rl);

unsigned char sls_next(struct ss7 *ss7);

#endif /* _MTP3_H */
