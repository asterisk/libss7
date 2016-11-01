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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libss7.h"
#include "ss7_internal.h"
#include "mtp2.h"
#include "mtp3.h"
#include "isup.h"

char testmessage[] = "2564286288";

#define mtp3_size(ss7) (((ss7)->switchtype == SS7_ITU) ? 5 : 8)
/* Routing label size */
#define rl_size(ss7) (((ss7)->switchtype == SS7_ITU) ? 4 : 7)

static struct mtp2 * slc_to_mtp2(struct ss7 *ss7, unsigned int slc);

static char * userpart2str(unsigned char userpart)
{
	switch (userpart) {
		case SIG_NET_MNG:
			return "NET_MNG";
		case SIG_STD_TEST:
			return "STD_TEST";
		case SIG_SPEC_TEST:
			return "SPEC_TEST";
		case SIG_SCCP:
			return "SCCP";
		case SIG_ISUP:
			return "ISUP";
		default:
			return "Unknown";
	}
}

static char * ss7_ni2str(unsigned char ni)
{
	switch (ni) {
		case SS7_NI_INT:
			return "international";
		case SS7_NI_INT_SPARE:
			return "international_spare";
		case SS7_NI_NAT:
			return "national";
		case SS7_NI_NAT_SPARE:
			return "national_spare";
		default:
			return "Unknown";
	}
}

static int get_routinglabel(unsigned int switchtype, unsigned char *sif, struct routing_label *rl)
{
	unsigned char *buf = sif;
	rl->type = switchtype;

	if (switchtype == SS7_ANSI) {
		rl->dpc = buf[0] | (buf[1] << 8) | (buf[2] << 16);
		rl->opc = buf[3] | (buf[4] << 8) | (buf[5] << 16);
		rl->sls = buf[6];
		return 7;
	} else { /* ITU style */
		/* more complicated.  Stupid ITU with their programmer unfriendly point codes. */
		rl->dpc = (buf[0] | (buf[1] << 8)) & 0x3fff; /* 14 bits long */
		rl->opc = ((buf[1] >> 6) | (buf[2] << 2) | (buf[3] << 10)) & 0x3fff;
		rl->sls = buf[3] >> 4;
		return 4;
	}
}

unsigned char ansi_sls_next(struct ss7 *ss7)
{
	unsigned char res = ss7->sls;

	ss7->sls = (ss7->sls + 1) % 256;

	return res;
}

static unsigned char get_userpart(unsigned char sio)
{
	return sio & 0xf;
}

static inline unsigned char get_ni(unsigned char sio)
{
	return (sio >> 6) & 0x3;
}

static inline unsigned char get_priority(unsigned char sio)
{
	return (sio >> 4) & 0x3;
}

int set_routinglabel(unsigned char *sif, struct routing_label *rl)
{
	unsigned char *buf = sif;
	switch (rl->type) {
		case SS7_ANSI: /* Cake */
			buf[0] = rl->dpc & 0xff;
			buf[1] = (rl->dpc >> 8) & 0xff;
			buf[2] = (rl->dpc >> 16) & 0xff;
			buf[3] = rl->opc & 0xff;
			buf[4] = (rl->opc >> 8) & 0xff;
			buf[5] = (rl->opc >> 16) & 0xff;
			buf[6] = rl->sls & 0xff;
			return 7;
		case SS7_ITU:
			/* Stupid ITU point codes.  This would be a lot easier
			if compilers could come up with a standard for doing bit
			field packing within data structures.  But alas, they all
			have their differences.  So bit shifting it is.  */
			buf[0] = rl->dpc & 0xff;
			buf[1] = ((rl->dpc >> 8) & 0x3f) | ((rl->opc << 6) & 0xc0);
			buf[2] = (rl->opc >> 2) & 0xff;
			buf[3] = ((rl->opc >> 10) & 0x0f) | ((rl->sls << 4) & 0xf0);
			return 4;
		default:
			return -1;
	}
}

/* Hopefully it'll be the bottom 4 bits */
static inline void set_h0(unsigned char *h0, unsigned char val)
{
	(*h0) |= (val & 0xf);
}

static inline unsigned char get_h0(unsigned char *byte)
{
	return (*byte) & 0xf;
}

static inline void set_h1(unsigned char *h1, unsigned char val)
{
	(*h1) |= ((val << 4) & 0xf0);
}

static inline unsigned char get_h1(unsigned char *byte)
{
	return (((*byte) & 0xf0) >> 4);
}

static inline int link_available(struct ss7 *ss7, int linkid, struct ss7_msg ***buffer, struct routing_label rl)
{
	if ((ss7->mtp2_linkstate[linkid] == MTP2_LINKSTATE_UP &&
			ss7->links[linkid]->adj_sp->state == MTP3_UP &&
			ss7->links[linkid]->changeover != CHANGEOVER_COMPLETED) ||
			(ss7->links[linkid]->changeover == CHANGEOVER_IN_PROGRESS) ||
			ss7->links[linkid]->changeover == CHANGEBACK_INITIATED) {

		struct mtp3_route *route = ss7->links[linkid]->adj_sp->routes;
		while (route) {
			if (route->dpc == rl.dpc) {
				if (route->t6 > -1) {
					/* T6 is running, buffering */
					*buffer = &route->q;
					return 1;
				}
				if (route->state == TFR_NON_ACTIVE || route->state == TFA) {
					break;
				} else {
					*buffer = NULL;
					return 0;
				}
			}
			route = route->next;
		}

		switch (ss7->links[linkid]->changeover) {
			case CHANGEOVER_IN_PROGRESS:
			case CHANGEOVER_INITIATED:
				*buffer = &ss7->links[linkid]->co_buf;
				return 1;
			case CHANGEBACK_INITIATED:
			case CHANGEBACK:
				*buffer = &ss7->links[linkid]->cb_buf;
				return 1;
			default:
				*buffer = NULL;
				return 1;
		}

	} else {
		*buffer = NULL;
		return 0;
	}
}

static inline struct mtp2 * rl_to_link(struct ss7 *ss7, struct routing_label rl, struct ss7_msg ***buffer)
{
	int linkid;

	linkid = (rl.sls >> ss7->sls_shift) % ss7->numlinks;

	if (link_available(ss7, linkid, buffer, rl)) {
		return ss7->links[linkid];
	} else {
		struct mtp2 *winner = NULL;
		int i;

		for (i = 0; i < ss7->numlinks; i++) {
			if (link_available(ss7, i, buffer, rl)) {
				winner = ss7->links[i];
				break;
			}
		}

		return winner;
	}
}

struct net_mng_message net_mng_messages[] = {
	{ 1, 1, "COO"},
	{ 1, 2, "COA"},
	{ 1, 5, "CBD"},
	{ 1, 6, "CBA"},
	{ 2, 1, "ECO"},
	{ 2, 2, "ECA"},
	{ 3, 1, "RCT"},
	{ 3, 2, "TFC"},
	{ 4, 1, "TFP"},
	{ 4, 2, "TCP"},
	{ 4, 3, "TFR"},
	{ 4, 4, "TCR"},
	{ 4, 5, "TFA"},
	{ 4, 6, "TCA"},
	{ 5, 1, "RST/RSP"},
	{ 5, 2, "RSR"},
	{ 5, 3, "RCP"},
	{ 5, 4, "RCR"},
	{ 6, 1, "LIN"},
	{ 6, 2, "LUN"},
	{ 6, 3, "LIA"},
	{ 6, 4, "LUA"},
	{ 6, 5, "LID"},
	{ 6, 6, "LFU"},
	{ 6, 7, "LLT/LLI"},
	{ 6, 8, "LRT/LRI"},
	{ 7, 1, "TRA"},
	{ 7, 2, "TRW"},
	{ 8, 1, "DLC"},
	{ 8, 2, "CSS"},
	{ 8, 3, "CNS"},
	{ 8, 4, "CNP"},
	{ 0xa, 1, "UPU"},
};

static void mtp3_setstate_mtp2link(struct ss7 *ss7, struct mtp2 *link, int newstate)
{
	int i;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i] == link) {
			ss7->mtp2_linkstate[i] = newstate;
		}
	}
}

static char * net_mng_message2str(int h0, int h1)
{
	int i;

	for (i = 0; i < (sizeof(net_mng_messages) / sizeof(struct net_mng_message)); i++) {
		if ((net_mng_messages[i].h0 == h0) && (net_mng_messages[i].h1 == h1)) {
			return net_mng_messages[i].name;
		}
	}

	return "Unknown";
}

static int net_mng_dump(struct ss7 *ss7, struct mtp2 *mtp2, unsigned char userpart, unsigned char *buf, int len)
{
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char h1, h0;

	h1 = h0 = *headerptr;

	h1 = get_h1(headerptr);
	h0 = get_h0(headerptr);

	ss7_message(ss7, "\tH0: %x H1: %x\n", h0, h1);
	if (userpart == SIG_NET_MNG) {
		ss7_message(ss7, "\tMessage type: %s\n", net_mng_message2str(h0, h1));
	}
	ss7_dump_buf(ss7, 1, headerptr, len - rl_size(ss7));
	return 0;
}

static void q707_t1_expiry(void *data);

static void std_test_send(struct mtp2 *link)
{
	struct ss7_msg *m;
	unsigned char *layer4;
	struct routing_label rl;
	struct ss7 *ss7 = link->master;
	int rllen = 0;
	unsigned char testlen = strlen(testmessage);

	m = ss7_msg_new();
	if (!m) {
		ss7_error(link->master, "Malloc failed on ss7_msg!.  Unable to transmit STD_TEST\n");
		return;
	}

	layer4 = ss7_msg_userpart(m);
	rl.type = ss7->switchtype;
	rl.opc = ss7->pc;
	rl.dpc = link->dpc;
	rl.sls = link->net_mng_sls;

	rllen = set_routinglabel(layer4, &rl);
	layer4 += rllen;

	set_h0(layer4, 1);
	set_h1(layer4, 1);
	if (ss7->switchtype == SS7_ANSI) {
		layer4[1] = (testlen << 4) | (link->slc & 0xf);
	} else {
		layer4[1] = (testlen << 4);
	}

	memcpy(&layer4[2], testmessage, testlen);

	ss7_msg_userpart_len(m, rllen + testlen + 2);

	if (mtp3_transmit(link->master, (ss7->switchtype == SS7_ITU) ? SIG_STD_TEST : SIG_SPEC_TEST, rl, 3, m, link) > -1 &&
		link->master->mtp3_timers[MTP3_TIMER_Q707_T1] > 0) {
		if (link->mtp3_timer[MTP3_TIMER_Q707_T1] > -1) {
			ss7_schedule_del(ss7, &link->mtp3_timer[MTP3_TIMER_Q707_T1]);
		}
		link->mtp3_timer[MTP3_TIMER_Q707_T1] = ss7_schedule_event(ss7, link->master->mtp3_timers[MTP3_TIMER_Q707_T1], q707_t1_expiry, link);
	}
}

static void mtp3_event_link_down(struct mtp2 *link);

static void q707_t1_expiry(void *data)
{
	struct mtp2 *link = data;
	link->q707_t1_failed++;
	link->mtp3_timer[MTP3_TIMER_Q707_T1] = -1;
	if (link->q707_t1_failed > 1) {
		ss7_error(link->master, "Q707 T1 timer expired 2nd time on link SLC: %i PC: %i\n", link->slc, link->dpc);
		link->q707_t1_failed = 0;
		if (link->mtp3_timer[MTP3_TIMER_Q707_T2] > -1) {
			ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_Q707_T2]);
		}
		mtp3_event_link_down(link);
	}

	std_test_send(link);
}

static inline void ss7_linkset_up_event(struct ss7 *ss7)
{
	ss7_event *e = ss7_next_empty_event(ss7);
	if (!e) {
		return;
	}
	e->e = SS7_STATE_UP;
}

static void linkset_up_expired(void *data)
{
	struct ss7 *ss7 = data;

	ss7->linkset_up_timer = -1;
	ss7_linkset_up_event(ss7);
}

static int available_links(struct ss7 *ss7, int dpc, int ignore_inhibit)
{
	int res = 0, i;
	struct mtp3_route *route;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->std_test_passed &&
				((ignore_inhibit && ss7->links[i]->inhibit) ? 1 :
				(ss7->links[i]->changeover == NO_CHANGEOVER || ss7->links[i]->changeover == CHANGEBACK))) {

			if (dpc == -1) {
				res++;
			} else {
				route = ss7->links[i]->adj_sp->routes;
				while (route) {
					if (route->dpc == dpc ) {
						if (route->state == TFA || route->state == TFR_NON_ACTIVE) {
							res++;
						}
						break;
					}
					route = route->next;
				}
			}
		}
	}
	return res;
}

static int ss7_check(struct ss7 *ss7)
{
	int i, j, mtp3_up = 0;
	int av_links = available_links(ss7, -1, 0);

	for (i = 0; i < ss7->numsps; i++) {
		if (ss7->adj_sps[i]->state == MTP3_UP) {
			mtp3_up = 1;
			break;
		}
	}

	if (!av_links && mtp3_up) {
		/* find a locally inhibited link */
		for (i = 0; i < ss7->numlinks; i++) {
			if (ss7->links[i]->inhibit & INHIBITED_LOCALLY) {
				if (!(ss7->links[i]->got_sent_netmsg & SENT_LUN)) {
					AUTORL(rl, ss7->links[i]);
					net_mng_send(ss7->links[i], NET_MNG_LUN, rl, 0);
					ss7_message(ss7, "Uninhibiting locally inhibited link (no more signalling links are in service) SLC: %i ADJPC: %i\n", ss7->links[i]->slc, ss7->links[i]->dpc);
				}
				if (ss7->links[i]->inhibit & INHIBITED_REMOTELY) {
					i = ss7->numlinks; /* in this case force remote uninhibit too ! */
				}
				break;
			}
		}
		/* try force uninhibit */
		if (i == ss7->numlinks) {
			for (i = 0; i < ss7->numlinks; i++) {
				if (ss7->links[i]->inhibit & INHIBITED_REMOTELY) {
					if (!(ss7->links[i]->got_sent_netmsg & SENT_LFU)) {
						AUTORL(rl, ss7->links[i]);
						net_mng_send(ss7->links[i], NET_MNG_LFU, rl, 0);
						ss7_message(ss7, "Forced uninhibiting remotely inhibited link (no more signalling links are in service) SLC: %i ADJPC: %i\n", ss7->links[i]->slc, ss7->links[i]->dpc);
					}
					break;
				}
			}
		}
	}

	if (ss7->state != SS7_STATE_UP && mtp3_up) {
		ss7->state = SS7_STATE_UP;

		if (LINKSET_UP_DELAY > -1) {
			if (ss7->linkset_up_timer > -1) {
				ss7_schedule_del(ss7, &ss7->linkset_up_timer);
			}
			ss7->linkset_up_timer = ss7_schedule_event(ss7, LINKSET_UP_DELAY, &linkset_up_expired, ss7);
			ss7_message(ss7, "LINKSET UP DELAYING RESETTING\n");
		} else {
			ss7_linkset_up_event(ss7);
		}

		/* Set links to changeover state which are not came up yet */
		for (i = 0; i < ss7->numlinks; i++) {
			if (!ss7->links[i]->std_test_passed) {
				ss7->links[i]->changeover = CHANGEOVER_COMPLETED;
			}
		}
	}

	if (ss7->state != SS7_STATE_DOWN && !mtp3_up) {
		ss7->state = SS7_STATE_DOWN;
		if (ss7->linkset_up_timer == -1) {
			struct mtp2 *link;
			ss7_event *e = ss7_next_empty_event(ss7);

			if (!e) {
				return -1;
			}
			e->e = SS7_EVENT_DOWN;
			isup_free_all_calls(ss7);

			for (i = 0; i < ss7->numlinks; i++) {
				link = ss7->links[i];
				if (link->inhibit & INHIBITED_LOCALLY) {
					link->changeover = CHANGEOVER_COMPLETED; /* because will be stopped all of the timers and flushed the buffers */
				} else {
					link->changeover = NO_CHANGEOVER;
				}

				mtp3_free_co(link);
				for (j = 0; j < MTP3_MAX_TIMERS; j++) {
					if (link->mtp3_timer[j] > -1) {
						ss7_schedule_del(ss7, &link->mtp3_timer[j]);
					}
				}
			}

		} else {
			ss7_schedule_del(ss7, &ss7->linkset_up_timer);
		}
	}

	return 0;
}

static void mtp3_stop_all_timers_except_cocb(struct mtp2 *link)
{
	int x;

	for (x = 0; x < MTP3_MAX_TIMERS; x++) {
		if (link->mtp3_timer[x] > -1 && x != MTP3_TIMER_T1 && x != MTP3_TIMER_T3 &&
				x != MTP3_TIMER_T2 && x != MTP3_TIMER_T4 && x != MTP3_TIMER_T5 &&
				x != MTP3_TIMER_T22 && x != MTP3_TIMER_T23) {
			ss7_schedule_del(link->master, &link->mtp3_timer[x]);
			ss7_debug_msg(link->master, SS7_DEBUG_MTP3, "Stopped MTP3 timer %s on link SLC: %i PC: %i\n", mtp3_timer2str(x), link->slc, link->dpc);
		}
	}
}

static void mtp3_timer_q707_t2_expiry(void *data)
{
	struct mtp2 *link = data;

	std_test_send(link);
	link->mtp3_timer[MTP3_TIMER_Q707_T2] = ss7_schedule_event(link->master, link->master->mtp3_timers[MTP3_TIMER_Q707_T2], mtp3_timer_q707_t2_expiry, link);
}

static void mtp3_event_link_up(struct mtp2 * link)
{
	std_test_send(link);
	if (link->master->mtp3_timers[MTP3_TIMER_Q707_T2] > 0) {
		if (link->mtp3_timer[MTP3_TIMER_Q707_T2] > -1) {
			ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_Q707_T2]);
		}
		link->mtp3_timer[MTP3_TIMER_Q707_T2] = ss7_schedule_event(link->master, link->master->mtp3_timers[MTP3_TIMER_Q707_T2], mtp3_timer_q707_t2_expiry, link);
	}
}

static void mtp3_move_buffer(struct ss7 *ss7, struct mtp2 *link, struct ss7_msg **from, struct ss7_msg **to, int dpc, int fsn)
{
	struct ss7_msg *cur, *prev, *next, *dst;
	unsigned char *buf;
	unsigned char userpart;
	struct routing_label rl;

	if (fsn != -1) {
		update_txbuf(NULL, from, fsn);
	}

	prev = NULL;
	cur = *from;

	if (to && *to) {
		for (dst = *to; dst->next; dst = dst->next);
	} else {
		dst = NULL;
	}

	while (cur) {
		buf = cur->buf;
		userpart = get_userpart(buf[MTP2_SIZE]);
		get_routinglabel(ss7->switchtype, buf + MTP2_SIZE + 1, &rl);
		next = cur->next;

		if (userpart > 3 && (dpc == -1 || rl.dpc == dpc)) {
			if (cur == link->retransmit_pos) {
				link->retransmit_pos = cur->next;
			}

			if (*from == link->tx_buf || *from == link->tx_q ||
					*from == link->co_tx_buf || *from == link->co_tx_q) {
				cur->size -= 2;	/* mtp2_msu increased it before!!! */
			}

			if (prev) {
				prev->next = cur->next;
			} else {
				*from = cur->next;
			}

			if (to) {
				if (dst) {
					dst->next = cur;
					dst = dst->next;
				} else {
					*to = cur;
					dst = cur;
				}
				dst->next = NULL;
			} else {
				free (cur);
			}
		} else {
			prev = cur;
		}

		cur = next;
	}
}

static void mtp3_transmit_buffer(struct ss7 *ss7, struct ss7_msg **buf)
{
	unsigned char userpart;
	struct routing_label rl;
	struct ss7_msg *cur = *buf, *next;
	int priority = -1;

	while (cur) {
		next = cur->next;
		userpart = get_userpart(cur->buf[MTP2_SIZE]);
		priority = get_priority(cur->buf[MTP2_SIZE]);
		get_routinglabel(ss7->switchtype, cur->buf + MTP2_SIZE + 1, &rl);
		mtp3_transmit(ss7, userpart, rl, priority, cur, NULL);
		cur = next;
	}
	*buf = NULL;
}

void mtp3_free_co(struct mtp2 *link)
{
	struct ss7_msg *cur;

	while (link->co_tx_buf) {
		cur = link->co_tx_buf;
		link->co_tx_buf = link->co_tx_buf->next;
		free(cur);
	}

	while (link->co_tx_q) {
		cur = link->co_tx_q;
		link->co_tx_q = link->co_tx_q->next;
		free(cur);
	}
}

static void mtp3_cancel_changeover(struct mtp2 *link)
{
	if (link->mtp3_timer[MTP3_TIMER_T1] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T1]);
	}
	if (link->mtp3_timer[MTP3_TIMER_T2] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T2]);
	}
	link->got_sent_netmsg &= ~(SENT_COO | SENT_ECO);
	mtp3_move_buffer(link->master, link, &link->co_tx_q, &link->cb_buf, -1, -1);
	mtp3_move_buffer(link->master, link, &link->co_buf, &link->cb_buf, -1, -1);
	link->changeover = NO_CHANGEOVER;
	mtp3_free_co(link);
	ss7_message(link->master, "Changeover cancelled on link SLC %i PC %i\n", link->slc, link->dpc);
}

static void mtp3_check(struct adjacent_sp *adj_sp)
{
	if (!adj_sp) {
		return;
	}

	struct ss7 *ss7 = adj_sp->master;
	int i, count = 0;

	for (i = 0; i < adj_sp->numlinks; i++) {
		if (adj_sp->links[i]->std_test_passed) {
			count++;
		}
	}

	if (!count && adj_sp->state != MTP3_DOWN) {
		adj_sp->state = MTP3_DOWN;
		adj_sp->tra = 0;

		if (adj_sp->timer_t19 > -1) {
			ss7_schedule_del(ss7, &adj_sp->timer_t19);
			ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T19 timer stopped PC: %i\n", adj_sp->adjpc);
		}

		if (adj_sp->timer_t21 > -1) {
			ss7_schedule_del(ss7, &adj_sp->timer_t21);
			ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T21 timer stopped PC: %i\n", adj_sp->adjpc);
		}

		for (i = 0; i < adj_sp->numlinks; i++) {
			adj_sp->links[i]->got_sent_netmsg = 0;
			mtp3_stop_all_timers_except_cocb(adj_sp->links[i]);
			adj_sp->links[i]->inhibit &= ~INHIBITED_REMOTELY;
		}
		mtp3_destroy_all_routes(adj_sp);
		ss7_error(ss7, "Adjacent SP PC: %i DOWN!!!\n", adj_sp->adjpc);
		ss7_check(ss7);
		return;
	}

	if (count && adj_sp->state != MTP3_UP && adj_sp->tra & GOT && adj_sp->tra & SENT) {
		adj_sp->state = MTP3_UP;
		ss7_message(ss7, "Adjacent SP PC: %i UP!!!\n", adj_sp->adjpc);
		ss7_check(ss7);
	}
}

static int mtp3_init(struct mtp2 *link)
{
	int res;
	struct adjacent_sp *adj_sp = link->adj_sp;

	if (adj_sp->state == MTP3_DOWN) {
		AUTORL(rl, link);
		if (link->inhibit & INHIBITED_LOCALLY) {
			return net_mng_send(link, NET_MNG_LIN, rl, 0);
		} else if (!(adj_sp->tra & SENT)) {
			res = net_mng_send(link, NET_MNG_TRA, rl, 0);
			mtp3_check(adj_sp);
			return res;
		}
	}
	return 0;
}

static void mtp3_restart(struct mtp2 *link)
{
	struct adjacent_sp *adj_sp = link->adj_sp;
	int i;

	adj_sp->state = MTP3_DOWN;
	adj_sp->tra = 0;
	for (i = 0; i < adj_sp->numlinks; i++) {
		adj_sp->links[i]->inhibit &= ~INHIBITED_REMOTELY ;
		adj_sp->links[i]->got_sent_netmsg = 0;
		mtp3_stop_all_timers_except_cocb(adj_sp->links[i]);
	}

	mtp3_destroy_all_routes(adj_sp);

	for (i = 0; i < adj_sp->numlinks; i++) {
		if (!mtp3_init(adj_sp->links[i])) {
			break;
		}
	}

	ss7_check(link->master);
}

void mtp3_init_restart(struct ss7 *ss7, int slc)
{
	struct mtp2 *link = slc_to_mtp2(ss7, slc);

	if (!link) {
		ss7_error(ss7, "signalling link does not exist\n");
		return;
	}

	{
		struct adjacent_sp *adj_sp = link->adj_sp;

		if (adj_sp->timer_t19 > -1) {
			ss7_schedule_del(ss7, &adj_sp->timer_t19);
		}

		if (adj_sp->timer_t21 > -1) {
			ss7_schedule_del(ss7, &adj_sp->timer_t21);
		}
	}

	mtp3_restart(link);
}

static void mtp3_t3_expired(void *data)
{
	struct mtp2 *link = data;

	link->mtp3_timer[MTP3_TIMER_T3] = -1;
	link->changeover = NO_CHANGEOVER;
	mtp3_transmit_buffer(link->master, &link->cb_buf);
	ss7_check(link->master);
	ss7_message(link->master, "Changeback completed on link SLC: %i PC: %i\n", link->slc, link->dpc);
	mtp3_free_co(link);
}

static void mtp3_changeback(struct mtp2 *link)
{
	if (link->inhibit) {
		ss7_message(link->master, "Change back requested inhibited link, ignore SLC: %i PC: %i\n", link->slc, link->dpc);
		return;
	}

	if (link->changeover == CHANGEOVER_IN_PROGRESS ||
			link->changeover == CHANGEOVER_INITIATED) {
		mtp3_cancel_changeover(link);
	} else if (link->changeover != CHANGEBACK && link->changeover != NO_CHANGEOVER) {
		mtp3_move_buffer(link->master, link, &link->tx_q, &link->cb_buf, -1, -1);
		link->changeover = CHANGEBACK;
		link->mtp3_timer[MTP3_TIMER_T3] = ss7_schedule_event(link->master, link->master->mtp3_timers[MTP3_TIMER_T3], &mtp3_t3_expired, link);
		ss7_message(link->master, "Changeback started on link SLC %i PC %i\n", link->slc, link->dpc);
	}
	mtp3_check(link->adj_sp);
}

static void mtp3_cancel_changeback(struct mtp2 *link)
{
	mtp3_move_buffer(link->master, link, &link->cb_buf, &link->co_buf, -1, -1);
	link->changeover = NO_CHANGEOVER;
	if (link->mtp3_timer[MTP3_TIMER_T3] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T3]);
	}
	if (link->mtp3_timer[MTP3_TIMER_T4] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T4]);
	}
	if (link->mtp3_timer[MTP3_TIMER_T5] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T5]);
	}
	ss7_message(link->master, "Changeback cancelled on link SLC %i PC %i\n", link->slc, link->dpc);
	mtp3_check(link->adj_sp);
}

static void mtp3_t1_expired(void *data)
{
	struct mtp2 *link = data;

	link->mtp3_timer[MTP3_TIMER_T1] = -1;
	link->changeover = CHANGEOVER_COMPLETED;
	mtp3_transmit_buffer(link->master, &link->co_buf);
	ss7_message(link->master, "Changeover completed on link SLC: %i PC: %i\n", link->slc, link->dpc);
	mtp3_free_co(link);
}

static void mtp3_timed_changeover(struct mtp2 *link)
{
	if (link->changeover == CHANGEBACK || link->changeover == CHANGEBACK_INITIATED) {
		mtp3_cancel_changeback(link);
	}
	if (link->changeover == NO_CHANGEOVER) {
		link->changeover = CHANGEOVER_IN_PROGRESS;
		mtp3_move_buffer(link->master, link, &link->tx_q, &link->co_buf, -1, -1);
		ss7_message(link->master, "Time controlled changeover initiated on link SLC: %i PC: %i\n", link->slc, link->dpc);
		link->changeover = CHANGEOVER_IN_PROGRESS;
		if (link->mtp3_timer[MTP3_TIMER_T1] > -1) {
			ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_T1]);
		}
		link->mtp3_timer[MTP3_TIMER_T1] = ss7_schedule_event(link->master, link->master->mtp3_timers[MTP3_TIMER_T1], &mtp3_t1_expired, link);
		mtp3_free_co(link);
	}
}

static void mtp3_changeover(struct mtp2 *link, unsigned char fsn)
{
	struct ss7_msg *tmp = NULL;

	if (link->changeover == CHANGEBACK || link->changeover == CHANGEBACK_INITIATED) {
		mtp3_cancel_changeback(link);
	}
	if (link->changeover == NO_CHANGEOVER ||
			link->changeover == CHANGEOVER_INITIATED) {
		mtp3_move_buffer(link->master, link, &link->co_tx_buf, &tmp, -1, fsn);
		mtp3_move_buffer(link->master, link, &link->co_tx_q, &tmp, -1, -1);
		mtp3_move_buffer(link->master, link, &link->co_buf, &tmp, -1, -1);
		mtp3_transmit_buffer(link->master, &tmp);
		link->changeover = CHANGEOVER_COMPLETED;
		ss7_message (link->master, "Changeover completed on link SLC: %i PC: %i FSN: %i\n", link->slc, link->dpc, fsn);
		mtp3_free_co(link);
		mtp3_check(link->adj_sp);
	}
}

static void mtp3_prepare_changeover(struct mtp2 *link)
{
	if (link->changeover == CHANGEBACK || link->changeover == CHANGEBACK_INITIATED) {
		mtp3_cancel_changeback(link);
	}
	if (link->changeover != CHANGEOVER_INITIATED) {
		link->changeover = CHANGEOVER_INITIATED;
		link->co_lastfsnacked = link->lastfsnacked;
		link->co_tx_buf = link->tx_buf;
		link->tx_buf = NULL;
		link->co_tx_q = link->tx_q;
		link->tx_q = NULL;
		link->retransmit_pos = NULL;
	}
#if 0
	ss7_message(link->master, "Prepare changeover co_tx_buf:%i (%i) co_buf:%i (%i) %i\n",
			link->co_tx_buf, len_buf(link->co_tx_buf), link->co_buf, len_buf(link->co_buf), link->co_tx_q);
#endif
}

static void mtp3_t19_expiry(void * data)
{
	struct adjacent_sp *adj_sp = data;

	adj_sp->timer_t19 = -1;
	ss7_debug_msg(adj_sp->master, SS7_DEBUG_MTP3, "MTP3 T19 timer expired PC:%i\n", adj_sp->adjpc);
}

static void mtp3_t21_expiry(void * data)
{
	struct adjacent_sp *adj_sp = data;

	adj_sp->timer_t21 = -1;
	adj_sp->tra |= GOT;
	ss7_debug_msg(adj_sp->master, SS7_DEBUG_MTP3,
		"MTP3 T21 timer expired on PC:%i. Started accepting traffic.\n", adj_sp->adjpc);
	mtp3_check(adj_sp);
}

static void mtp3_t22_expired(void *data)
{
	struct mtp2 *link = data;
	struct ss7 *ss7 = link->master;

	if (link->inhibit & INHIBITED_LOCALLY) {
		AUTORL(rl, link);
		net_mng_send(link, NET_MNG_LLT, rl, 0);
		link->mtp3_timer[MTP3_TIMER_T22] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T22], &mtp3_t22_expired, link);
	} else {
		link->mtp3_timer[MTP3_TIMER_T22] = -1;
	}
}

static void mtp3_t23_expired(void *data)
{
	struct mtp2 *link = data;
	struct ss7 *ss7 = link->master;

	if (link->inhibit & INHIBITED_REMOTELY) {
		AUTORL(rl, link);
		net_mng_send(link, NET_MNG_LRT, rl, 0);
		link->mtp3_timer[MTP3_TIMER_T23] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T23], &mtp3_t23_expired, link);
	} else {
		link->mtp3_timer[MTP3_TIMER_T23] = -1;
	}
}

static inline unsigned int pc2int(unsigned int switchtype, unsigned char *p)
{
	/* from get_routinglabel() */
	if (switchtype == SS7_ANSI) {
		return  p[0] | (p[1] << 8) | (p[2] << 16);
	} else {
		return (p[0] | (p[1] << 8)) & 0x3fff;
	}
}

static void mtp3_t10_expired(void *data)
{
	struct mtp3_route *route = data;
	struct adjacent_sp *adj_sp = route->owner;
	struct ss7 *ss7 = adj_sp->master;
	struct routing_label rl;

	rl.dpc = adj_sp->adjpc;
	rl.opc = ss7->pc;
	rl.sls = adj_sp->links[0]->net_mng_sls;

	net_mng_send(adj_sp->links[0], NET_MNG_RST, rl, route->dpc);
	route->t10 = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T10], &mtp3_t10_expired, route);
}

static void mtp3_forced_reroute(struct adjacent_sp *adj_sp, struct mtp3_route *route)
{
	int i = 0;
	struct ss7 *ss7 = adj_sp->master;
	struct mtp2 *link;

	for (i = 0; i < adj_sp->numlinks; i++) {
		link = adj_sp->links[i];
		mtp3_move_buffer(ss7, link, &link->tx_q, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->co_tx_q, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->co_buf, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->cb_buf, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->tx_buf, NULL, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->co_tx_buf, NULL, route->dpc, -1);
	}

	if (route->t6 > -1) {
		ss7_schedule_del(ss7, &route->t6);
	}

	if (route->t10 > -1) {
		ss7_schedule_del(ss7, &route->t10);
	}

	if (ss7->mtp3_timers[MTP3_TIMER_T10] > 0) {
		route->t10 = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T10], &mtp3_t10_expired, route);
	}

	mtp3_transmit_buffer(ss7, &route->q);
}

static void mtp3_destroy_route(struct adjacent_sp *adj_sp, struct mtp3_route *route)
{
	struct mtp3_route *prev;

	if (route == adj_sp->routes) {
		adj_sp->routes = route->next;
	} else {
		prev = adj_sp->routes;
		while (prev) {
			if (prev->next == route) {
				prev->next = route->next;
				break;
			}
			prev = prev->next;
		}
	}

	if (route->t6 > -1) {
		ss7_schedule_del(adj_sp->master, &route->t6);
	}
	if (route->t10 > -1) {
		ss7_schedule_del(adj_sp->master, &route->t10);
	}

	mtp3_move_buffer(adj_sp->master, adj_sp->links[0], &route->q, NULL, -1, -1);
	free(route);
}

static void mtp3_t6_expired(void *data)
{
	struct mtp3_route *route = data;
	struct adjacent_sp *adj_sp = route->owner;

	route->t6 = -1;
	mtp3_transmit_buffer(adj_sp->master, &route->q);

	if (route->state == TFA) {
		mtp3_destroy_route(adj_sp, route);
	}
}

static void mtp3_controlled_reroute(struct adjacent_sp *adj_sp, struct mtp3_route *route)
{
	int i = 0;
	struct ss7 *ss7 = adj_sp->master;
	struct mtp2 *link;

	for (i = 0; i < adj_sp->numlinks; i++) {
		link = adj_sp->links[i];
		mtp3_move_buffer(ss7, link, &link->tx_q, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->co_buf, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->cb_buf, &route->q, route->dpc, -1);
		mtp3_move_buffer(ss7, link, &link->co_tx_q, &route->q, route->dpc, -1);
	}

	if (route->t6 > -1) {
		ss7_schedule_del(ss7, &route->t6);
	}

	if (route->t10 > -1) {
		ss7_schedule_del(ss7, &route->t10);
	}

	route->t6 = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T6], &mtp3_t6_expired, route);
}

static void mtp3_add_set_route(struct adjacent_sp *adj_sp, unsigned short dpc, int state)
{
	struct mtp3_route *cur = adj_sp->routes;
	struct mtp3_route *prev = adj_sp->routes;
	int set = 0;

	while (cur) {
		if (cur->dpc == dpc) {
			set = 1;
			cur->state = state;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if (!set && state == TFA) {
		return;
	}

	if (!set) {
		cur = calloc(1, sizeof(struct mtp3_route));
		if (!cur) {
			ss7_error(adj_sp->master, "calloc failed!!!\n");
			return;
		}
		if (prev) {
			prev->next = cur;
		} else {
			adj_sp->routes = cur;
		}

		cur->owner = adj_sp;
		cur->dpc = dpc;
		cur->state = state;
		cur->t6 = -1;
		cur->t10 = -1;

		cur->next = NULL;
	}

	if (state == TFP) {
		mtp3_forced_reroute(adj_sp, cur);
	} else if (state == TFA || state == TFR_ACTIVE) {
		mtp3_controlled_reroute(adj_sp, cur);
	}
}

static struct mtp2 * netmng_adjpc_sls_to_mtp2(struct ss7 *ss7, unsigned int adjpc, unsigned int sls)
{
	int i = 0;
	struct mtp2 *link = NULL;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->net_mng_sls == sls && ss7->links[i]->dpc == adjpc) {
			link = ss7->links[i];
		}
	}

	return link;
}

static int net_mng_receive(struct ss7 *ss7, struct mtp2 *mtp2, struct routing_label *rl, unsigned char *buf, int len)
{
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char *paramptr = headerptr + 1;
	struct routing_label rlr;
	struct mtp2 *winner = netmng_adjpc_sls_to_mtp2(mtp2->master, rl->opc, rl->sls); /* changeover, changeback!!! */

	if (!winner && *headerptr != NET_MNG_TRA && *headerptr != NET_MNG_TFA && *headerptr != NET_MNG_TFP && *headerptr != NET_MNG_TFR) {
		/* Could not find the given slc and the message is not allowed to any slc as per Q.704 13.3.1 */
		/* http://lists.digium.com/pipermail/asterisk-ss7/2009-November/003197.html */
		return 0;
	}

	rlr.sls = rl->sls;
	rlr.dpc = rl->opc;
	rlr.opc = rl->dpc;

	switch (*headerptr) {
		case NET_MNG_TRA:
			if (mtp2->adj_sp->timer_t19 == -1 && mtp2->adj_sp->state != MTP3_DOWN) {
				ss7_message(ss7, "***** MTP3 restart initiated by remote peer on SLC: %i PC: %i\n", mtp2->slc, mtp2->dpc);
				mtp2->adj_sp->tra = GOT;
				mtp3_restart(mtp2);
			}

			if (mtp2->adj_sp->timer_t19 > -1) {
				ss7_message(ss7, "Got TRA while T19 has been running on link SLC: %i PC: %i ignoring\n", mtp2->slc, mtp2->dpc);
				break;
			}

			if (ss7->mtp3_timers[MTP3_TIMER_T19] > 0 && mtp2->adj_sp->timer_t19 == -1) {
				mtp2->adj_sp->timer_t19 = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T19], mtp3_t19_expiry, mtp2->adj_sp);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T19 timer started PC: %i\n", mtp2->adj_sp->adjpc);
			}

			if (mtp2->adj_sp->timer_t21 > -1) {
				ss7_schedule_del(ss7, &mtp2->adj_sp->timer_t21);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T21 timer stopped PC: %i\n", mtp2->adj_sp->adjpc);
			}

			mtp2->adj_sp->tra |= GOT;
			mtp3_check(mtp2->adj_sp);
			return 0;
		case NET_MNG_COO:
			if (winner->changeover == NO_CHANGEOVER || winner->changeover == CHANGEOVER_INITIATED) {
				net_mng_send(mtp2, NET_MNG_COA, rlr,
						(winner->changeover == CHANGEOVER_INITIATED) ? winner->co_lastfsnacked : winner->lastfsnacked);
			} else {
				net_mng_send(mtp2, NET_MNG_ECA, rlr, 0);
			}
			if (winner->changeover != CHANGEOVER_COMPLETED || winner->changeover != CHANGEOVER_INITIATED) {
				mtp3_prepare_changeover(winner);
				mtp3_changeover(winner, *paramptr);
			}
			return 0;
		case NET_MNG_COA:
			if (!(winner->got_sent_netmsg & (SENT_COO | SENT_ECO))) {
				ss7_error(ss7, "Got COA on SLC %i ADJPC %i but we haven't sent COO or ECO\n",
					winner->slc, winner->dpc);
				return -1;
			}
			if (winner->mtp3_timer[MTP3_TIMER_T2] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T2]);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T2 timer stopped on link SLC: %i ADJPC: %i\n",
						winner->slc, winner->dpc);
			}
			winner->got_sent_netmsg &= ~(SENT_COO | SENT_ECO);
			mtp3_changeover(winner, *paramptr);
			return 0;
		case NET_MNG_CBD:
			net_mng_send(mtp2, NET_MNG_CBA, rlr, (unsigned int) *paramptr);
			mtp3_changeback(winner);
			return 0;
		case NET_MNG_CBA:
			if (!(winner->got_sent_netmsg & SENT_CBD)) {
				ss7_error(ss7, "Got CBA on SLC %i ADJPC %i but we haven't sent CBD\n", winner->slc, winner->dpc);
				return -1;
			}
			winner->got_sent_netmsg &= ~SENT_CBD;
			if (winner->mtp3_timer[MTP3_TIMER_T4] > -1) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T4 timer stopped on link SLC: %i ADJPC: %i\n", winner->slc, winner->dpc);
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T4]);
			}
			if (winner->mtp3_timer[MTP3_TIMER_T5] > -1) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T5 timer stopped on link SLC: %i ADJPC: %i\n", winner->slc, winner->dpc);
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T5]);
			}
			mtp3_changeback(winner);
			return 0;
		case NET_MNG_ECO:
			/* ECO cancel the COO!!! */
			if ((winner->got_sent_netmsg & SENT_ECO) && winner->mtp3_timer[MTP3_TIMER_T2] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T2]);
			}
			if (winner->mtp3_timer[MTP3_TIMER_T1] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T1]);
			}
			winner->got_sent_netmsg &= ~(SENT_CBD | SENT_COO);
			if (winner->got_sent_netmsg & SENT_ECO) {
				net_mng_send(mtp2, NET_MNG_ECA, rlr, 0);	/* If we sent previously ECO we must answer now with ECA!!! */
			} else {
				if (winner->changeover == NO_CHANGEOVER) {
					net_mng_send(mtp2, NET_MNG_COA, rlr, winner->lastfsnacked);
				} else {
					net_mng_send(mtp2, NET_MNG_ECA, rlr, 0);
				}
			}
			mtp3_timed_changeover(winner);
			return 0;
		case NET_MNG_ECA:
			if (winner->mtp3_timer[MTP3_TIMER_T2] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T2]);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T2 timer stopped on link SLC: %i ADJPC: %i\n",
						winner->slc, winner->dpc);
			}
			winner->got_sent_netmsg &= ~(SENT_ECO | SENT_COO);
			mtp3_timed_changeover(winner);
			return 0;
		case NET_MNG_LIN:
			if (available_links(ss7, -1, 0) > 1 || winner->inhibit || !winner->std_test_passed) {
				net_mng_send(mtp2, NET_MNG_LIA, rlr, 0);
				winner->inhibit |= INHIBITED_REMOTELY;
				mtp3_timed_changeover(winner);
				if (ss7->mtp3_timers[MTP3_TIMER_T23] > 0) {
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T23 timer started on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
					winner->mtp3_timer[MTP3_TIMER_T23] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T23], &mtp3_t23_expired, winner);
				}
			} else {
				ss7_error(ss7, "Link inhibit requested on link SLC: %i ADJPC: %i - denied\n", winner->slc, winner->dpc);
				net_mng_send(mtp2, NET_MNG_LID, rlr, 0);
			}
			return 0;
		case NET_MNG_LFU:
			if (!(winner->got_sent_netmsg & SENT_LUN)) {	/* dont send LUN again */
				net_mng_send(mtp2, NET_MNG_LUN, rlr, 0);
			}
			return 0;
		case NET_MNG_LUN:
			winner->got_sent_netmsg &= ~SENT_LFU;
			net_mng_send(mtp2, NET_MNG_LUA, rlr, (unsigned int) *paramptr);
			winner->inhibit &= ~INHIBITED_REMOTELY;
			mtp3_changeback(winner);
			if (winner->mtp3_timer[MTP3_TIMER_T23] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T23]);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T23 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
			}
			if (winner->mtp3_timer[MTP3_TIMER_T13] > -1) {
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T13]);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T13 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
			}
			return 0;
		case NET_MNG_LIA:
			if (winner->got_sent_netmsg & SENT_LIN) {
				mtp3_timed_changeover(winner);
				winner->got_sent_netmsg &= ~SENT_LIN;
				winner->inhibit |= INHIBITED_LOCALLY;
				mtp3_timed_changeover(winner);
				if (!(mtp2->adj_sp->tra & SENT) && mtp2 == winner) {
					net_mng_send(mtp2, NET_MNG_TRA, rlr, 0);
					ss7_check(ss7);
				}
				if (ss7->mtp3_timers[MTP3_TIMER_T22] > 0) {
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T22 timer started on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
					winner->mtp3_timer[MTP3_TIMER_T22] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T22], &mtp3_t22_expired, winner);
				}
				if (winner->mtp3_timer[MTP3_TIMER_T14] > 0) {
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T14 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
					ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T14]);
				}
			}
			return 0;
		case NET_MNG_LUA:
			if (winner->got_sent_netmsg & SENT_LUN) {
				winner->got_sent_netmsg &= ~SENT_LUN;
				winner->inhibit &= ~INHIBITED_LOCALLY;
				mtp3_changeback(winner);
				if (winner->mtp3_timer[MTP3_TIMER_T12] > -1) {
					ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T12]);
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T12 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
				}
				if (winner->mtp3_timer[MTP3_TIMER_T22] > -1) {
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T22 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
					ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T22]);
				}
			}
			return 0;
		case NET_MNG_LLT:
			if (!(winner->inhibit & INHIBITED_REMOTELY)) {
				ss7_message(ss7, "Link SLC: %i ADJPC: %i not inhibited remotely, forced uninhibit\n", winner->slc, winner->dpc);
				net_mng_send(mtp2, NET_MNG_LFU, rlr, 0);
			}
			return 0;
		case NET_MNG_LRT:
			if (!(winner->inhibit & INHIBITED_LOCALLY)) {
				ss7_message(ss7, "Link SLC: %i ADJPC: %i not inhibited locally, uninhibit\n", winner->slc, winner->dpc);
				net_mng_send(mtp2, NET_MNG_LUN, rlr, 0);
			}
			return 0;
		case NET_MNG_LID:
			ss7_error(ss7, "Our inhibit request denied on link SLC: %i ADJPC: %i\n", winner->slc, winner->dpc);
			winner->got_sent_netmsg &= ~SENT_LIN;
			if (winner->mtp3_timer[MTP3_TIMER_T14] > 0) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T14 timer stopped on link SLC: %i ADJPC %i\n", winner->slc, winner->dpc);
				ss7_schedule_del(ss7, &winner->mtp3_timer[MTP3_TIMER_T14]);
			}
			return 0;
		case NET_MNG_TFP:
			mtp3_add_set_route(mtp2->adj_sp, pc2int(ss7->switchtype, paramptr), TFP);
			return 0;
		case NET_MNG_TFR:
			return 0;
		case NET_MNG_TFA:
			mtp3_add_set_route(mtp2->adj_sp, pc2int(ss7->switchtype, paramptr), TFA);
			return 0;
		default:
			ss7_error(ss7, "Unkonwn NET MNG %u on link SLC: %i from ADJPC: %i\n", *headerptr, winner->slc, winner->dpc);

	}
#if 0
	ss7_message(ss7, "NET MNG message type %s received\n", net_mng_message2str(h0, h1));
#endif
	return 0;
}

static void mtp3_t2_expired(void * data)
{
	struct mtp2 *link = data;
	struct ss7_msg *tmp = NULL;

	link->mtp3_timer[MTP3_TIMER_T2] = -1;
	link->got_sent_netmsg &= ~(SENT_COO | SENT_ECO);
	mtp3_move_buffer(link->master, link, &link->co_tx_q, &tmp, -1, -1);
	mtp3_move_buffer(link->master, link, &link->co_buf, &tmp, -1, -1);
	mtp3_transmit_buffer(link->master, &tmp);
	link->changeover = CHANGEOVER_COMPLETED;
	mtp3_free_co(link);
	mtp3_check(link->adj_sp);
	ss7_message(link->master, "MTP3 T2 timer expired on link SLC: %i ADJPC: %i changeover completed\n",
			link->slc, link->dpc);
}

static void mtp3_t5_expired(void *data)
{
	struct mtp2 *link = data;
	struct ss7 *ss7 = link->master;

	link->mtp3_timer[MTP3_TIMER_T5] = -1;
	link->got_sent_netmsg &= ~SENT_CBD;
	ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T5 timer expired on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	mtp3_t3_expired(link);
}

static void mtp3_t4_expired(void *data)
{
	struct mtp2 *link = data;
	struct ss7 *ss7 = link->master;

	ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T4 timer expired on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	AUTORL(rl, link);
	net_mng_send(link, NET_MNG_CBD, rl, link->cb_seq);
	link->mtp3_timer[MTP3_TIMER_T4] = -1;
	if (ss7->mtp3_timers[MTP3_TIMER_T5] > 0) {
		link->mtp3_timer[MTP3_TIMER_T5] = ss7_schedule_event(link->master, ss7->mtp3_timers[MTP3_TIMER_T5], &mtp3_t5_expired, link);
		ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T5 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	}
}

static void mtp3_t14_expired(void *data)
{
	struct mtp2 *link = data;

	AUTORL(rl, link);
	net_mng_send(link, NET_MNG_LIN, rl, 0);
}

static void mtp3_t14_expired_2nd(void *data)
{
	struct mtp2 *link = data;

	ss7_error(link->master, "MTP3 T14 timer expired 2nd time on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	link->mtp3_timer[MTP3_TIMER_T14] = -1;
	link->inhibit &= ~INHIBITED_LOCALLY;
	link->got_sent_netmsg &= ~SENT_LIN;
}

static void mtp3_t13_expired(void *data)
{
	struct mtp2 *link = data;

	AUTORL(rl, link);
	net_mng_send(link, NET_MNG_LFU, rl, 0);
}

static void mtp3_t13_expired_2nd(void *data)
{
	struct mtp2 *link = data;

	ss7_error(link->master, "MTP3 T13 timer expired 2nd time on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	link->mtp3_timer[MTP3_TIMER_T13] = -1;
}

static void mtp3_t12_expired(void *data)
{
	struct mtp2 *link = data;

	AUTORL(rl, link);
	net_mng_send(link, NET_MNG_LUN, rl, 0);
}

static void mtp3_t12_expired_2nd(void *data)
{
	struct mtp2 *link = data;

	ss7_error(link->master, "MTP3 T12 timer expired 2nd time on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
	link->mtp3_timer[MTP3_TIMER_T12] = -1;
	link->got_sent_netmsg &= ~SENT_LUN;
}

int net_mng_send(struct mtp2 *link, unsigned char h0h1, struct routing_label rl, unsigned int param)
{
	struct ss7_msg *m;
	unsigned char *layer4;
	struct ss7 *ss7 = link->master;
	int rllen = 0;
	int i, res;

	m = ss7_msg_new();
	if (!m) {
		ss7_error(link->master, "Malloc failed on ss7_msg!.  Unable to transmit NET_MNG\n");
		return -1;
	}

	layer4 = ss7_msg_userpart(m);
	rl.type = ss7->switchtype;
	rl.opc = ss7->pc;

	rllen = set_routinglabel(layer4, &rl);
	layer4 += rllen;
	*layer4 = h0h1;
	layer4++;

	switch (h0h1) {
		case NET_MNG_CBD:
			link->got_sent_netmsg |= SENT_CBD;
			if (ss7->mtp3_timers[MTP3_TIMER_T4] > 0 &&
				link->mtp3_timer[MTP3_TIMER_T4] == -1) {	/* if 0 called from mtp3_t4_expired() */
					link->mtp3_timer[MTP3_TIMER_T4] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T4], &mtp3_t4_expired, link);
					ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T4 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
			}
			link->cb_seq = (unsigned char) param;		/* save the CBD sequence, we may need on retransmit */
			/* No break here */
		case NET_MNG_CBA:
			*layer4 = (unsigned char) param;		/* CB code */
			ss7_msg_userpart_len(m, rllen + 1 + 1);		/* rl + CB code */
			break;
		case NET_MNG_COO:
			link->got_sent_netmsg |= SENT_COO;
			if (ss7->mtp3_timers[MTP3_TIMER_T2]) {
				if (link->mtp3_timer[MTP3_TIMER_T2] > 0) {
					ss7_schedule_del(ss7, &link->mtp3_timer[MTP3_TIMER_T2]);
				}
				link->mtp3_timer[MTP3_TIMER_T2] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T2], &mtp3_t2_expired, link);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T2 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
			}
			/* No break here */
		case NET_MNG_COA:
			*layer4 = (unsigned char) param;		/* FSN of last accepted MSU */
			ss7_msg_userpart_len(m, rllen + 1 + 1);		/* rl + FSN of last accepted MSU */
			break;
		case NET_MNG_LUN:
			link->got_sent_netmsg |= SENT_LUN;
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			if (ss7->mtp3_timers[MTP3_TIMER_T14] > 0) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T12 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
				if (link->mtp3_timer[MTP3_TIMER_T12] == -1) {
					link->mtp3_timer[MTP3_TIMER_T12] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T12],
							&mtp3_t12_expired, link);
				} else {
					link->mtp3_timer[MTP3_TIMER_T12] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T12],
							&mtp3_t12_expired_2nd, link);
				}
			}
			break;
		case NET_MNG_LIN:
			link->got_sent_netmsg |= SENT_LIN;
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			if (ss7->mtp3_timers[MTP3_TIMER_T14] > 0) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T14 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
				if (link->mtp3_timer[MTP3_TIMER_T14] == -1) {
					link->mtp3_timer[MTP3_TIMER_T14] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T14],
							&mtp3_t14_expired, link);
				} else {
					link->mtp3_timer[MTP3_TIMER_T14] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T14],
							&mtp3_t14_expired_2nd, link);
				}
			}
			break;
		case NET_MNG_TRA:
			/* we are not an stp, so we can start the T21 now */
			if (ss7->mtp3_timers[MTP3_TIMER_T21] > 0 && link->adj_sp->timer_t21 == -1) {
				link->adj_sp->timer_t21 = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T21],
					&mtp3_t21_expiry, link->adj_sp);
			}
			link->adj_sp->tra |= SENT;
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			break;
		case NET_MNG_ECO:
			link->got_sent_netmsg |= SENT_ECO;
			if (ss7->mtp3_timers[MTP3_TIMER_T2]) {
				if (link->mtp3_timer[MTP3_TIMER_T2] > -1) {
					ss7_schedule_del(ss7, &link->mtp3_timer[MTP3_TIMER_T2]);
				}
				link->mtp3_timer[MTP3_TIMER_T2] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T2], &mtp3_t2_expired, link);
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T2 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
			}
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			break;
		case NET_MNG_ECA:
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			break;
		case NET_MNG_LFU:
			link->got_sent_netmsg |= SENT_LFU;
			if (ss7->mtp3_timers[MTP3_TIMER_T13] > 0) {
				ss7_debug_msg(ss7, SS7_DEBUG_MTP3, "MTP3 T13 timer started on link SLC: %i ADJPC: %i\n", link->slc, link->dpc);
				if (link->mtp3_timer[MTP3_TIMER_T13] == -1) {
					link->mtp3_timer[MTP3_TIMER_T13] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T13],
							&mtp3_t13_expired, link);
				} else {
					link->mtp3_timer[MTP3_TIMER_T13] = ss7_schedule_event(ss7, ss7->mtp3_timers[MTP3_TIMER_T13],
							&mtp3_t13_expired_2nd, link);
				}
			}
			break;
		case NET_MNG_LIA:
		case NET_MNG_LUA:
		case NET_MNG_LID:
		case NET_MNG_LLT:
		case NET_MNG_LRT:
			ss7_msg_userpart_len(m, rllen + 1);		/* no more params */
			break;
		case NET_MNG_TFP:
		case NET_MNG_TFA:
		case NET_MNG_TFR:
		case NET_MNG_RST:
		case NET_MNG_RSR:
			/* TODO: ANSI!!!! */
			layer4[0] = (unsigned char) (param & 0xff);
			layer4[1] = (unsigned char) ((param >> 8) & 0xff);
			ss7_msg_userpart_len(m, rllen + 1 + 2);
			break;
		default:
			ss7_error(link->master, "Invalid or unimplemented NET MSG!\n");
			free (m);
			return -1;
	}

	if (link->std_test_passed) {
		return mtp3_transmit(ss7, SIG_NET_MNG, rl, 3, m, link);
	} else {
		/* we may use another link to the same adjacent sp */
		res = -1;
		for (i = 0; i < link->adj_sp->numlinks; i++) {
			if (link->adj_sp->links[i]->std_test_passed) {
				res = mtp3_transmit(ss7, SIG_NET_MNG, rl, 3, m, link->adj_sp->links[i]);
			}
		}

		if (res != -1) {
			return res;
		} else {
			/* if we couldn't send changeover.... */
			if (link->got_sent_netmsg & (SENT_COO | SENT_ECO)) {
				link->got_sent_netmsg &= ~(SENT_COO | SENT_ECO);
				if (link->mtp3_timer[MTP3_TIMER_T2] > 0) {
					ss7_schedule_del(ss7, &link->mtp3_timer[MTP3_TIMER_T2]);
				}

				ss7_message(ss7, "No more signalling link to adjacent sp %d, timed changeover initiated\n", link->dpc);
				mtp3_timed_changeover(link);
				free (m);
				return -1;
			}
			/* cancel changeback */
			if (link->got_sent_netmsg & SENT_CBD) {
				link->got_sent_netmsg &= ~SENT_CBD;
				mtp3_cancel_changeback(link);
			}
		}
	}

	ss7_error(link->master, "No signalling link available for NET MNG: %s !!!\n", net_mng_message2str(h0h1 & 0x0f, h0h1 >> 4));
	free (m);
	return -1;
}

static void mtp3_link_failed(struct mtp2 *link)
{
	link->std_test_passed = 0;
	if (link->master->numlinks > 1 && available_links(link->master, -1, 1) &&
			(link->changeover == NO_CHANGEOVER || link->changeover == CHANGEBACK)) {
		AUTORL(rl, link);
		mtp3_prepare_changeover(link);
		net_mng_send(link, NET_MNG_COO, rl, link->co_lastfsnacked);
	}
	/* stop sending SLTM */
	if (link->mtp3_timer[MTP3_TIMER_Q707_T1] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_Q707_T1]);
	}
	if (link->mtp3_timer[MTP3_TIMER_Q707_T2] > -1) {
		ss7_schedule_del(link->master, &link->mtp3_timer[MTP3_TIMER_Q707_T2]);
	}
	mtp3_check(link->adj_sp);
}

static int std_test_receive(struct ss7 *ss7, struct mtp2 *mtp2, struct routing_label rl, unsigned char *buf, int len)
{
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char h1, h0;
	int testpatsize = 0;
	struct routing_label drl;

	/* just reverse the routing label - mtp3_receive() have checked it is correct */
	drl.type = rl.type;
	drl.dpc = rl.opc;
	drl.opc = rl.dpc;
	drl.sls = rl.sls;

	h1 = h0 = *headerptr;

	h1 = get_h1(headerptr);
	h0 = get_h0(headerptr);

	if (h0 != 1) {
		goto fail;
	}

	if (h1 == 1) {
		struct ss7_msg *m;
		unsigned char *layer4;
		int rllen;

		m = ss7_msg_new();
		if (!m) {
			ss7_error(ss7, "Unable to allocate message buffer!\n");
			return -1;
		}

		layer4 = ss7_msg_userpart(m);

		rllen = set_routinglabel(layer4, &drl);
		layer4 += rllen;

		testpatsize = (headerptr[1] >> 4) & 0xf;
		/* Success! */
		set_h0(layer4, 1);
		set_h1(layer4, 2);
		if (ss7->switchtype == SS7_ANSI) {
			layer4[1] = (testpatsize << 4) | (mtp2->slc & 0xf);
		} else {
			layer4[1] = (testpatsize << 4);
		}
		memcpy(&layer4[2], &headerptr[2], testpatsize);

		ss7_msg_userpart_len(m, rllen + testpatsize + 2);

		mtp3_transmit(ss7, (ss7->switchtype == SS7_ITU) ? SIG_STD_TEST : SIG_SPEC_TEST, drl, 3, m, mtp2);

		/* Update linkstate */
		mtp3_setstate_mtp2link(ss7, mtp2, MTP2_LINKSTATE_UP);
		return 0;
	} else if (h1 == 2) {
		if (mtp2->mtp3_timer[MTP3_TIMER_Q707_T1] > -1) {
			ss7_schedule_del(ss7, &mtp2->mtp3_timer[MTP3_TIMER_Q707_T1]);
		}
		if (!memcmp(&headerptr[2], testmessage, (headerptr[1] >> 4) & 0xf)) {
			mtp2->q707_t1_failed = 0;
			if (!mtp2->std_test_passed) {
				mtp2->std_test_passed = 1;
				if (!mtp2->inhibit && mtp2->changeover != NO_CHANGEOVER &&
						mtp2->changeover != CHANGEBACK && mtp2->changeover != CHANGEBACK_INITIATED) {
					net_mng_send(mtp2, NET_MNG_CBD, drl, ss7->cb_seq);
					ss7->cb_seq++;
					if (mtp2->changeover == CHANGEOVER_IN_PROGRESS || mtp2->changeover == CHANGEOVER_INITIATED) {
						mtp3_cancel_changeover(mtp2);
					}
					mtp2->changeover = CHANGEBACK_INITIATED;
				}
			}
			if (mtp2->adj_sp->state == MTP3_DOWN) {
				return mtp3_init(mtp2);
			}
		} else {
			ss7_error(ss7, "Mismatch the SLTA on link SLC: %i ADJPC %i\n", mtp2->slc, mtp2->dpc);
			mtp2->std_test_passed = 0;
			mtp2->q707_t1_failed++;
			mtp3_link_failed(mtp2);
			if (ss7->mtp3_timers[MTP3_TIMER_Q707_T2] > 0 && mtp2->mtp3_timer[MTP3_TIMER_Q707_T2] == -1) {
				mtp2->mtp3_timer[MTP3_TIMER_Q707_T2] = ss7_schedule_event(ss7,
						ss7->mtp3_timers[MTP3_TIMER_Q707_T2], mtp3_timer_q707_t2_expiry, mtp2);
			}
		}
		return 0;
	} else {
		ss7_error(ss7, "Unhandled STD_TEST message: h0 = %x h1 = %x", h0, h1);
	}

fail:
	return -1;
}

static int mtp3_to_buffer(struct ss7_msg **buf, struct ss7_msg *m)
{
	m->next = NULL;

	if (!(*buf)) {
		*buf = m;
		return 0;
	}

	{
		struct ss7_msg *cur = *buf;
		for (cur = *buf; cur->next; cur = cur->next);
		cur->next = m;
	}

	return 0;
}

int mtp3_transmit(struct ss7 *ss7, unsigned char userpart, struct routing_label rl, int priority, struct ss7_msg *m, struct mtp2 *link)
{
	unsigned char *sio;
	struct mtp2 *winner;
	struct ss7_msg **buffer = NULL;

	sio = m->buf + MTP2_SIZE;

	if (userpart == SIG_ISUP) {
		winner = rl_to_link(ss7, rl, &buffer);
	} else {
		winner = link;
	}

	if (ss7->switchtype == SS7_ITU) {
		(*sio) = (ss7->ni << 6) | userpart;
	} else {
		(*sio) = (ss7->ni << 6) | (priority << 4) | userpart;
	}

	if (winner) {
		if (buffer) {
			return mtp3_to_buffer(buffer, m);
		} else {
			return mtp2_msu(winner, m);
		}
	} else {
		ss7_error(ss7, "No siganlling link available sending message!\n");
		free(m);
		return -1;
	}
}

int mtp3_dump(struct ss7 *ss7, struct mtp2 *link, void *msg, int len)
{
	unsigned char *buf = (unsigned char *)msg;
	unsigned char *sio = &buf[0];
	unsigned char *sif = &buf[1];
	unsigned char ni = get_ni(*sio);
	unsigned char priority = get_priority(*sio);
	unsigned char userpart = get_userpart(*sio);
	struct routing_label rl;
	unsigned int siflen = len - 1;
	int rlsize;


	ss7_message(ss7, "\tNetwork Indicator: %d Priority: %d User Part: %s (%d)\n", ni, priority, userpart2str(userpart), userpart);
	ss7_dump_buf(ss7, 1, sio, 1);
	rlsize = get_routinglabel(ss7->switchtype, sif, &rl);
	if (ss7->switchtype == SS7_ANSI) {
		ss7_message(ss7, "\tOPC %d-%d-%d DPC %d-%d-%d SLS %d\n", (rl.opc >> 16) & 0xff, (rl.opc >> 8) & 0xff, rl.opc & 0xff,
				(rl.dpc >> 16) & 0xff, (rl.dpc >> 8) & 0xff, rl.dpc & 0xff, rl.sls);
	} else {
		ss7_message(ss7, "\tOPC %d DPC %d SLS %d\n", rl.opc, rl.dpc, rl.sls);
	}
	ss7_dump_buf(ss7, 1, sif, rlsize);

	/* Pass it to the correct user part */
	switch (userpart) {
		case SIG_NET_MNG:
		case SIG_STD_TEST:
		case SIG_SPEC_TEST:
			return net_mng_dump(ss7, link, userpart, sif, siflen);
		case SIG_ISUP:
			return isup_dump(ss7, link, sif + rlsize, siflen - rlsize);
		case SIG_SCCP:
		default:
			return 0;
	}
	return 0;
}

int mtp3_receive(struct ss7 *ss7, struct mtp2 *link, void *msg, int len)
{
	unsigned char *buf = (unsigned char *)msg;
	unsigned char *sio = &buf[0];
	unsigned char *sif = &buf[1];
	unsigned int siflen = len - 1;
	unsigned char ni = get_ni(*sio);
	unsigned char userpart = get_userpart(*sio);
	struct routing_label rl;
	int rlsize;

	/* Check NI to make sure it's set correct */
	if (ss7->ni != ni) {
		ss7_error(ss7, "Received MSU with network indicator of %s, but we are %s\n", ss7_ni2str(ni), ss7_ni2str(ss7->ni));
		return -1;
	}

	/* Check point codes to make sure the message is destined for us */
	rlsize = get_routinglabel(ss7->switchtype, sif, &rl);

	if (ss7->pc != rl.dpc) {
		ss7_error(ss7, "Received message destined for point code 0x%x, but we are 0x%x.  Dropping\n", rl.dpc, ss7->pc);
		return -1;
	} else if (userpart == SIG_STD_TEST || userpart == SIG_SPEC_TEST || userpart == SIG_NET_MNG) {
		if (link->dpc != rl.opc) {
			ss7_error(ss7, "Received message from point code 0x%x but we are connected to DPC 0x%x.  Dropping\n", rl.opc, link->dpc);
			return -1;
		}

		if (link->slc != rl.sls) {
			ss7_error(ss7, "Received message for slc 0x%x, but we are 0x%x.  Dropping\n", rl.sls, link->slc);
			return -1;
		}
	}

	/* TODO: find out what to do with the priority in ANSI networks */

	/* Pass it to the correct user part */
	switch (userpart) {
		case SIG_STD_TEST:
		case SIG_SPEC_TEST:
			return std_test_receive(ss7, link, rl, sif, siflen);
		case SIG_ISUP:
			/* Skip the routing label */
			if (link->adj_sp->state == MTP3_UP) {
				return isup_receive(ss7, link, &rl, sif + rlsize, siflen - rlsize);
			} else {
				ss7_error(ss7, "Got ISUP message on link while MTP3 state is not UP!\n");
				return 0;
			}
		case SIG_NET_MNG:
			return net_mng_receive(ss7, link, &rl, sif, siflen);
		case SIG_SCCP:
		default:
			ss7_message(ss7, "Unable to process message destined for userpart %d; dropping message\n", userpart);
			return 0;
	}
}

static void mtp3_event_link_down(struct mtp2 *link)
{
	struct ss7 *ss7 = link->master;

	/* Make sure we notify MTP3 that the link went down beneath us */
	mtp3_setstate_mtp2link(ss7, link, MTP2_LINKSTATE_DOWN);
	mtp3_link_failed(link);
}

static struct mtp2 * slc_to_mtp2(struct ss7 *ss7, unsigned int slc)
{
	int i = 0;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->slc == slc) {
			return ss7->links[i];
		}
	}

	return NULL;
}

ss7_event * mtp3_process_event(struct ss7 *ss7, ss7_event *e)
{
	/* Check to see if there is no event to process */
	if (!e) {
		return NULL;
	}

	switch (e->e) {
		case MTP2_LINK_UP:
			mtp3_event_link_up(e->link.link);
			return e;
		case MTP2_LINK_DOWN:
			mtp3_event_link_down(e->link.link);
			return e;
		default:
			return e;
	}

	return e;
}

void mtp3_start(struct ss7 *ss7)
{
	int i;

	for (i = 0; i < ss7->numlinks; i++) {
		if ((ss7->mtp2_linkstate[i] == MTP2_LINKSTATE_DOWN)) {
			mtp2_start(ss7->links[i], 1);
			ss7->mtp2_linkstate[i] = MTP2_LINKSTATE_ALIGNING;
		}
	}
	
	return;
}

void mtp3_alarm(struct ss7 *ss7, int fd)
{
	if (fd > -1) {
		int winner = ss7_find_link_index(ss7, fd);

		if (winner > -1) {
			ss7->mtp2_linkstate[winner] = MTP2_LINKSTATE_INALARM;
			mtp2_alarm(ss7->links[winner]);
			mtp3_link_failed(ss7->links[winner]);
		}
	}

	ss7_check(ss7);
}

void mtp3_noalarm(struct ss7 *ss7, int fd)
{
	int winner = ss7_find_link_index(ss7, fd);

	if (winner > -1) {
		ss7->mtp2_linkstate[winner] = MTP2_LINKSTATE_ALIGNING;
		mtp2_noalarm(ss7->links[winner]);
		mtp2_start(ss7->links[winner], 1);
	}
}

const char * mtp3_net_mng(struct ss7 *ss7, unsigned int slc, const char *cmd, unsigned int param)
{
	struct mtp2 *link;
	unsigned char h0h1;

	link = slc_to_mtp2(ss7, slc);

	if (!link) {
		return "Invalid slc!\n";
	}

	if (!strcasecmp("coo", cmd)) {
		h0h1 = NET_MNG_COO;
	} else if (!strcasecmp("coa", cmd)) {
		h0h1 = NET_MNG_COA;
	} else if (!strcasecmp("cbd", cmd)) {
		h0h1 = NET_MNG_CBD;
	} else if (!strcasecmp("cba", cmd)) {
		h0h1 = NET_MNG_CBA;
	} else if (!strcasecmp("eco", cmd)) {
		h0h1 = NET_MNG_ECO;
	} else if (!strcasecmp("eco", cmd)) {
		h0h1 = NET_MNG_ECA;
	} else if (!strcasecmp("lin", cmd)) {
		if (available_links(ss7, -1, 0) > 1 || link->inhibit || !link->std_test_passed) {
			h0h1 = NET_MNG_LIN;
		} else {
			return "Inhibit request discarded, no more available links!\n";
		}
	} else if (!strcasecmp("lun", cmd)) {
		h0h1 = NET_MNG_LUN;
	} else if (!strcasecmp("lia", cmd)) {
		h0h1 = NET_MNG_LIA;
	} else if (!strcasecmp("lua", cmd)) {
		h0h1 = NET_MNG_LUA;
	} else if (!strcasecmp("lfu", cmd)) {
		h0h1 = NET_MNG_LFU;
	} else if (!strcasecmp("tfa", cmd)) {
		h0h1 = NET_MNG_TFA;
	} else if (!strcasecmp("tfp", cmd)) {
		h0h1 = NET_MNG_TFP;
	} else if (!strcasecmp("tfr", cmd)) {
		h0h1 = NET_MNG_TFR;
	} else {
		return "Unknown msg\n";
	}

	AUTORL(rl, link);
	net_mng_send(link, h0h1, rl, param);

	return "OK\n";
}

int ss7_set_mtp3_timer(struct ss7 *ss7, char *name, int ms)
{
	if (!strcasecmp(name, "t1")) {
		ss7->mtp3_timers[MTP3_TIMER_T1] = ms;
	} else if (!strcasecmp(name, "t2")) {
		ss7->mtp3_timers[MTP3_TIMER_T2] = ms;
	} else if (!strcasecmp(name, "t3")) {
		ss7->mtp3_timers[MTP3_TIMER_T3] = ms;
	} else if (!strcasecmp(name, "t4")) {
		ss7->mtp3_timers[MTP3_TIMER_T4] = ms;
	} else if (!strcasecmp(name, "t5")) {
		ss7->mtp3_timers[MTP3_TIMER_T5] = ms;
	} else if (!strcasecmp(name, "t6")) {
		ss7->mtp3_timers[MTP3_TIMER_T6] = ms;
	} else if (!strcasecmp(name, "t7")) {
		ss7->mtp3_timers[MTP3_TIMER_T7] = ms;
	} else if (!strcasecmp(name, "t10")) {
		ss7->mtp3_timers[MTP3_TIMER_T10] = ms;
	} else if (!strcasecmp(name, "t12")) {
		ss7->mtp3_timers[MTP3_TIMER_T12] = ms;
	} else if (!strcasecmp(name, "t13")) {
		ss7->mtp3_timers[MTP3_TIMER_T13] = ms;
	} else if (!strcasecmp(name, "t14")) {
		ss7->mtp3_timers[MTP3_TIMER_T14] = ms;
	} else if (!strcasecmp(name, "t19")) {
		ss7->mtp3_timers[MTP3_TIMER_T19] = ms;
	} else if (!strcasecmp(name, "t21")) {
		ss7->mtp3_timers[MTP3_TIMER_T21] = ms;
	} else if (!strcasecmp(name, "t22")) {
		ss7->mtp3_timers[MTP3_TIMER_T22] = ms;
	} else if (!strcasecmp(name, "t23")) {
		ss7->mtp3_timers[MTP3_TIMER_T23] = ms;
	} else if (!strcasecmp(name, "q707_t1")) {
		ss7->mtp3_timers[MTP3_TIMER_Q707_T1] = ms;
	} else if (!strcasecmp(name, "q707_t2")) {
		ss7->mtp3_timers[MTP3_TIMER_Q707_T2] = ms;
	} else {
		ss7_message(ss7, "Unknown MTP3 timer: %s\n", name);
		return 0;
	}

	ss7_message(ss7, "MTP3 timer %s = %ims\n", name, ms);
	return 1;
}

char * mtp3_timer2str(int mtp3_timer)
{
	switch (mtp3_timer) {
		case MTP3_TIMER_T1:
			return "T1";
		case MTP3_TIMER_T2:
			return "T2";
		case MTP3_TIMER_T3:
			return "T3";
		case MTP3_TIMER_T4:
			return "T5";
		case MTP3_TIMER_T6:
			return "T6";
		case MTP3_TIMER_T7:
			return "T7";
		case MTP3_TIMER_T10:
			return "T10";
		case MTP3_TIMER_T12:
			return "T12";
		case MTP3_TIMER_T13:
			return "T13";
		case MTP3_TIMER_T14:
			return "T14";
		case MTP3_TIMER_T19:
			return "T19";
		case MTP3_TIMER_T21:
			return "T21";
		case MTP3_TIMER_T22:
			return "T22";
		case MTP3_TIMER_T23:
			return "T23";
		case MTP3_TIMER_Q707_T1:
			return "Q707_T1";
		case MTP3_TIMER_Q707_T2:
			return "Q707_T2";
	}
	return "Unknown";
}

static inline void mtp3_add_link_adjsps(struct adjacent_sp *adj_sp, struct mtp2 *link)
{
	int i;

	link->adj_sp = adj_sp;
	adj_sp->numlinks++;

	for (i = 0; i < adj_sp->numlinks; i++) {
		if (!adj_sp->links[i]) {
			adj_sp->links[i] = link;
			adj_sp->links[i]->net_mng_sls = adj_sp->numlinks - 1;
			break;
		}
	}
}

static inline void mtp3_new_adjsp(struct ss7 *ss7, struct mtp2 *link)
{
	struct adjacent_sp *new;

	if (ss7->numsps == SS7_MAX_ADJSPS) {
		ss7_error(ss7, "Couldn't add new adjacent sp, reached the %i limit", SS7_MAX_ADJSPS);
		return;
	}

	new = calloc(1, sizeof(struct adjacent_sp));

	if (!new) {
		ss7_error(ss7, "Couldn't allocate new adjacent SP\n");
		return;
	}

	ss7->adj_sps[ss7->numsps] = new;

	new->timer_t19 = -1;
	new->timer_t21 = -1;
	new->master = ss7;
	new->links[0] = link;
	new->numlinks = 1;
	new->adjpc = link->dpc;
	link->adj_sp = new;
	ss7->numsps++;
}

void mtp3_add_adj_sp(struct mtp2 *link)
{
	struct ss7 *ss7 = link->master;
	int i;

	for (i = 0; i < ss7->numsps; i++) {
		if (ss7->adj_sps[i] && link->dpc == ss7->adj_sps[i]->adjpc) {
			mtp3_add_link_adjsps(ss7->adj_sps[i], link);
			break;
		}
	}

	if (i == ss7->numsps) {
		mtp3_new_adjsp(ss7, link);
	}
}

void mtp3_destroy_all_routes(struct adjacent_sp *adj_sp)
{
	struct mtp3_route *next;

	while (adj_sp->routes) {
		next = adj_sp->routes->next;
		mtp3_destroy_route(adj_sp, adj_sp->routes);
		adj_sp->routes = next;
	}
}
