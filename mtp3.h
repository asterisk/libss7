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
#define SIG_STD_TEST	0x01
#define SIG_SPEC_TEST	0x02
#define SIG_SCCP		0x03
#define SIG_ISUP		0x05
/* Bits 6-5 -- ANSI networks only */
#define PRIORITY_0		0x00
#define PRIORITY_1		0x01
#define PRIORITY_2		0x02
#define PRIORITY_3		0x03

#define SIO_SIZE	1

#define MTP2_LINKSTATE_DOWN		0
#define MTP2_LINKSTATE_INALARM	1
#define MTP2_LINKSTATE_ALIGNING	2
#define MTP2_LINKSTATE_UP		3

#define MTP3_DOWN		0
#define MTP3_UP			1

#define GOT		(1 << 0)
#define SENT	(1 << 1)

/* Prohibited, restricted states */
#define TFP				1
#define TFA				2
#define TFR_NON_ACTIVE	3
#define TFR_ACTIVE		4

/* Net mngs		  h0     h1 */
#define NET_MNG_COO	(0x01 | 0x10)
#define NET_MNG_COA	(0x01 | 0x20)
#define NET_MNG_CBD	(0x01 | 0x50)
#define NET_MNG_CBA	(0x01 | 0x60)

#define NET_MNG_ECO	(0x02 | 0x10)
#define NET_MNG_ECA	(0x02 | 0x20)

#define NET_MNG_RCT	(0x03 | 0x10)
#define NET_MNG_TFC	(0x03 | 0x20)

#define NET_MNG_TFP	(0x04 | 0x10)
#define NET_MNG_TFR	(0x04 | 0x30)
#define NET_MNG_TFA	(0x04 | 0x50)

#define NET_MNG_RST	(0x05 | 0x10)
#define NET_MNG_RSR	(0x05 | 0x20)

#define NET_MNG_LIN	(0x06 | 0x10)
#define NET_MNG_LUN	(0x06 | 0x20)
#define NET_MNG_LIA	(0x06 | 0x30)
#define NET_MNG_LUA	(0x06 | 0x40)
#define NET_MNG_LID	(0x06 | 0x50)
#define NET_MNG_LFU	(0x06 | 0x60)
#define NET_MNG_LLT	(0x06 | 0x70)
#define NET_MNG_LRT	(0x06 | 0x80)

#define NET_MNG_TRA	(0x07 | 0x10)

#define NET_MNG_DLC	(0x08 | 0x10)
#define NET_MNG_CSS	(0x08 | 0x20)
#define NET_MNG_CNS	(0x08 | 0x30)
#define NET_MNG_CNP	(0x08 | 0x40)

#define NET_MNG_UPU	(0x0a | 0x10)

 /* INHIBIT states */
#define INHIBITED_REMOTELY	(1 << 0)
#define INHIBITED_LOCALLY	(1 << 1)

/* Got, Sent netmsgs */
#define SENT_LUN	(1 << 0)
#define SENT_LIN	(1 << 1)
#define SENT_COO	(1 << 2)
#define SENT_ECO	(1 << 3)
#define SENT_CBD	(1 << 4)
#define SENT_LFU	(1 << 5)

/* Chaneover states */
#define NO_CHANGEOVER			0
#define CHANGEOVER_INITIATED	1
#define CHANGEOVER_IN_PROGRESS	2
#define CHANGEOVER_COMPLETED	3
#define CHANGEBACK_INITIATED	4
#define CHANGEBACK				5

/* MTP3 timers */
#define MTP3_TIMER_T1	1
#define MTP3_TIMER_T2	2
#define MTP3_TIMER_T3	3
#define MTP3_TIMER_T4	4
#define MTP3_TIMER_T5	5
#define MTP3_TIMER_T6	6
#define MTP3_TIMER_T7	7
#define MTP3_TIMER_T8	8
#define MTP3_TIMER_T10	9
#define MTP3_TIMER_T12	10
#define MTP3_TIMER_T13	11
#define MTP3_TIMER_T14	12
#define MTP3_TIMER_T19	13
#define MTP3_TIMER_T21	14
#define MTP3_TIMER_T22	15
#define MTP3_TIMER_T23	16

#define MTP3_TIMER_Q707_T1	17
#define MTP3_TIMER_Q707_T2	18

#define AUTORL(rl, link)		\
	struct routing_label rl;	\
	rl.sls = link->net_mng_sls;	\
	rl.dpc = link->dpc;			\
	rl.opc = link->master->pc


struct net_mng_message {
	int h0;
	int h1;
	char *name;
};

struct mtp3_route {
	int state;
	unsigned int dpc;
	int t6;
	int t10;
	struct ss7_msg *q;
	struct adjacent_sp *owner;
	struct mtp3_route *next;
};

struct adjacent_sp {
	int state;
	unsigned int adjpc;
	struct mtp2 *links[SS7_MAX_LINKS];
	unsigned int numlinks;
	int timer_t19;
	int timer_t21;
	unsigned int tra;
	struct ss7 *master;
	struct mtp3_route *routes;
};

int net_mng_send(struct mtp2 *link, unsigned char h0h1, struct routing_label rl, unsigned int param);

/* Process any MTP2 events that occur */
ss7_event* mtp3_process_event(struct ss7 *ss7, ss7_event *e);

/* The main receive function for MTP3 */
int mtp3_receive(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

int mtp3_dump(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

/* Transmit */
int mtp3_transmit(struct ss7 *ss7, unsigned char userpart, struct routing_label rl, int priority, struct ss7_msg *m, struct mtp2 *link);

void mtp3_alarm(struct ss7 *ss7, int fd);

void mtp3_noalarm(struct ss7 *ss7, int fd);

void mtp3_start(struct ss7 *ss7);

unsigned char ansi_sls_next(struct ss7 *ss7);

int set_routinglabel(unsigned char *sif, struct routing_label *rl);

unsigned char sls_next(struct ss7 *ss7);

char * mtp3_timer2str(int mtp3_timer);

void mtp3_add_adj_sp(struct mtp2 *link);

void mtp3_free_co(struct mtp2 *link);

void mtp3_destroy_all_routes(struct adjacent_sp *adj_sp);

#endif /* _MTP3_H */
