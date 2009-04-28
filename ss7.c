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
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
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

void ss7_set_notinservice(void (*func)(struct ss7 *ss7, int cic, unsigned int dpc))
{
	ss7_notinservice = func;
}

void ss7_set_hangup(int (*func)(struct ss7 *ss7, int cic, unsigned int dpc, int cause, int do_hangup))
{
	ss7_hangup = func;
}

/* not called in normal operation */
void ss7_set_call_null(void (*func)(struct ss7 *ss7, struct isup_call *c, int lock))
{
	ss7_call_null = func;
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

int ss7_start(struct ss7 *ss7)
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

/* TODO: Add entry to routing table instead */
static int ss7_set_adjpc(struct mtp2 *mtp2, unsigned int pc)
{
	mtp2->dpc = pc;
	mtp3_add_adj_sp(mtp2);
	return 0;
}

int ss7_set_link_fd(struct ss7 *ss7, char *name, int fd)
{
	int i;

	for (i = 0; i < ss7->numlinks; i++) {
		if (!strcasecmp(ss7->links[i]->name, name)) {
			ss7->links[i]->fd = fd;
			break;
		}
	}

	return 0;
}

struct mtp2* ss7_add_slave_link(struct ss7 *ss7, int transport, char *name, int slc, unsigned int adjpc)
{
	struct mtp2 *m;

	if (ss7->numlinks >= SS7_MAX_LINKS)
		return NULL;

	m = mtp2_new(name, ss7->switchtype);
	
	if (!m)
		return NULL;

	m->master = ss7;

	if ((transport == SS7_TRANSPORT_DAHDIDCHAN) || (transport == SS7_TRANSPORT_DAHDIMTP2) ||
			(transport == SS7_TRANSPORT_TCP)) {
		if (slc > -1)
			m->slc = slc;
		else
			m->slc = ss7->numslavelinks;
		
		ss7->numslavelinks += 1;

		if (transport == SS7_TRANSPORT_DAHDIMTP2)
			m->flags |= MTP2_FLAG_ZAPMTP2;

		if (transport == SS7_TRANSPORT_TCP)
			m->flags |= MTP2_FLAG_TCP;

		ss7->slavelinks[ss7->numslavelinks - 1] = m;

		ss7_set_adjpc(ss7->slavelinks[ss7->numslavelinks-1], adjpc);

		return ss7->slavelinks[ss7->numslavelinks-1];

	} else {
		return NULL;
	}

	return NULL;
}

int ss7_add_link(struct ss7 *ss7, int transport, char *name, int slc, unsigned int adjpc)
{
	struct mtp2 *m;

	if (ss7->numlinks >= SS7_MAX_LINKS)
		return -1;

	if (!name && (name[0] == 0))
		return -1;

	m = mtp2_new(name, ss7->switchtype);
	
	if (!m)
		return -1;

	m->master = ss7;

	if ((transport == SS7_TRANSPORT_DAHDIDCHAN) || (transport == SS7_TRANSPORT_DAHDIMTP2) ||
		(transport == SS7_TRANSPORT_TCP)) {

		if (slc > -1)
			m->slc = slc;
		else
			m->slc = ss7->numlinks;
		
		ss7->numlinks += 1;

		if (transport == SS7_TRANSPORT_DAHDIMTP2)
			m->flags |= MTP2_FLAG_ZAPMTP2;

		if (transport == SS7_TRANSPORT_TCP)
			m->flags |= MTP2_FLAG_TCP;

		ss7->links[ss7->numlinks - 1] = m;

		ss7_set_adjpc(ss7->links[ss7->numlinks-1], adjpc);

		if (transport == SS7_TRANSPORT_TCP) {
			ss7->links[ss7->numlinks - 1]->adj_sp->state = MTP3_UP;
			ss7->mtp2_linkstate[ss7->numlinks - 1] = MTP2_LINKSTATE_UP;
			ss7->links[ss7->numlinks - 1]->tcpreadstate = TCPSTATE_NEED_LEN;
		}

	} else {
		return -1;
	}

	return 0;
}

int ss7_pollflags(struct ss7 *ss7, int fd)
{
	int i;
	struct mtp2 *winner = NULL;
	int flags = POLLPRI | POLLIN;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd) {
			winner = ss7->links[i];
			break;
		}
	}
	
	/* Maybe it's a lite link */
	for (i = 0; i < ss7->numslavelinks; i++) {
		if (ss7->slavelinks[i]->fd == fd) {
			winner = ss7->links[i];
			break;
		}
	}

	if (!winner)
		return -1;

	if (winner->flags & MTP2_FLAG_ZAPMTP2) {
		if (winner->flags & MTP2_FLAG_WRITE)
			flags |= POLLOUT;
	} else if (winner->flags & MTP2_FLAG_TCP) {
		flags = POLLIN;
	} else {
		flags |= POLLOUT;
	}

	return flags;
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
	int x;
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

	for (x = 0; x < ISUP_MAX_TIMERS; x++) {
		s->isup_timers[x] = 0;
	}
	
	s->linkset_up_timer = -1;
	
	s->flags = SS7_ISDN_ACCESS_INDICATOR;
	s->sls_shift = 0;

	return s;
}

void ss7_destroy(struct ss7 *ss7)
{
	int i;
	
	if (!ss7)
		return;

	/* ISUP */
	isup_free_all_calls(ss7);
	
	/* MTP3 */
	for (i = 0; i > ss7->numsps; i++) {
		mtp3_destroy_all_routes(ss7->adj_sps[i]);
		free(ss7->adj_sps[i]);
	}
	
	for (i = 0; i > ss7->numlinks; i++) {
		flush_bufs(ss7->links[i]);
		mtp3_free_co(ss7->links[i]);
		free(ss7->links[i]);
	}
	
	free(ss7);
}

void ss7_set_sls_shift(struct ss7 *ss7, unsigned char shift)
{
	if (!ss7)
		return;

	ss7->sls_shift = shift;
}

void ss7_set_flags(struct ss7 *ss7, unsigned int flags)
{
	if (!ss7)
		return;
	ss7->flags |= flags;
}

void ss7_clear_flags(struct ss7 *ss7, unsigned int flags)
{
	if (!ss7)
		return;
	ss7->flags &= ~flags;
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
	int res = -1;
	int winner = -1;
	int i;

	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->fd == fd) {
			winner = i;
			break;
		}
	}

	if (winner != -1) {
		if (ss7->links[winner]->flags & MTP2_FLAG_TCP) {
			res = isup_ip_receive(ss7->links[winner]);
			goto exit;
		} else {
			res = mtp2_receive(ss7->links[winner]);
			goto exit;
		}

	}


	/* Maybe it's a lite link */
	for (i = 0; i < ss7->numslavelinks; i++) {
		if (ss7->slavelinks[i]->fd == fd) {
			winner = i;
			break;
		}
	}

	if (winner != -1) {
		res = isup_ip_receive(ss7->slavelinks[winner]);
	}

exit:
	

	return res;
}

static inline char * changeover2str(int state)
{
	switch(state) {
		case NO_CHANGEOVER:
			return "NO";
		case CHANGEOVER_COMPLETED:
			return "COMPLETED";
		case CHANGEOVER_IN_PROGRESS:
			return "IN PROGRESS";
		case CHANGEOVER_INITIATED:
			return "CHANGEOVER INITIATED";
		case CHANGEBACK_INITIATED:
			return "CHANGEBACK INITIATED";
		case CHANGEBACK:
			return "IN CHANGEBACK";
		default:
			return "UNKNOWN";
	}
}

static inline char * got_sent2str(char * buf, unsigned int got_sent)
{
	buf[0] = '\0';

	if (got_sent & SENT_LIN)
		strcat(buf, " sentLIN");
	if (got_sent & SENT_LUN)
		strcat(buf, " sentLUN");
	if (got_sent & SENT_COO)
		strcat(buf, " sentCOO");
	if (got_sent & SENT_ECO)
		strcat(buf, " sentECO");
	if (got_sent & SENT_CBD)
		strcat(buf, " sentCBD");
	if (got_sent & SENT_LFU)
		strcat(buf, " sentLFU");
	
	return buf;
}

static inline char * mtp2state2str(struct ss7 *ss7, struct mtp2 *link)
{
	int mtp2state, i;
	
	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i] == link) {
			mtp2state = ss7->mtp2_linkstate[i];
			break;
		}
	}
	
	if (i == ss7->numlinks)
		return "UNKNOWN";
	
	
	switch (ss7->mtp2_linkstate[i]) {
		case MTP2_LINKSTATE_DOWN:
			return "DOWN";
		case MTP2_LINKSTATE_INALARM:
			return "INALARM";
		case MTP2_LINKSTATE_ALIGNING:
			return "ALIGNING";
		case MTP2_LINKSTATE_UP:
			return "UP";
		default:
			return "UNKNOWN";
	}
}

static inline char * mtp3_state(int state)
{
	switch (state) {
	case MTP3_DOWN:
		return "DOWN";
	case MTP3_LOCAL_RESTART:
		return "LOCAL RESTARTING";
	case MTP3_REMOTE_RESTART:
		return "REMOTE RESTARTING";
	case MTP3_ALIGN:
		return "ALIGN";
	case MTP3_UP:
		return "UP";
	default:
		return "UNKNOWN";
	}
}

static inline char * route_state(int state)
{
	switch (state) {
	case TFP:
		return "TFP";
	case TFA:
		return "TFA";
	case TFR_NON_ACTIVE:
		return "TFR NON ACTIVE";
	case TFR_ACTIVE:
		return "TFR ACTIVE";
	default:
		return "Unknown";	
	}
}

void ss7_pc_to_str(int ss7type, unsigned int pc, char *str)
{
	if (ss7type == SS7_ITU) {
		sprintf(str, "%d", pc);
	} else {
		sprintf(str, "%d-%d-%d", (pc >> 16) & 0xff, (pc >> 8) & 0xff, pc & 0xff);
	}
}

void ss7_show_linkset(struct ss7 *ss7, void (* cust_printf)(int fd, const char *format, ...), int fd)
{
	int j, i, x;
	char *p;
	char got_sent_buf[256], timers[512];
	struct adjacent_sp *adj_sp;
	struct mtp2 *link;
	struct mtp3_route *cur;
	char pc_str[64];

	ss7_pc_to_str(ss7->switchtype, ss7->pc, pc_str);
	
	cust_printf(fd, "Switch type: %s\n", (ss7->switchtype == SS7_ITU) ? "ITU" : "ANSI");
	cust_printf(fd, "Our point code: %s\n", pc_str);
	cust_printf(fd, "SLS shift: %i\n", ss7->sls_shift);
	cust_printf(fd, "numlinks: %i\n", ss7->numlinks);
	cust_printf(fd, "numsps: %i\n", ss7->numsps);


	for (j = 0; j < ss7->numsps; j++) {
		adj_sp = ss7->adj_sps[j];
		ss7_pc_to_str(ss7->switchtype, adj_sp->adjpc, pc_str);
		cust_printf(fd, "  ---------------------------------\n  Adjacent SP PC: %s STATE: %s\n", pc_str, mtp3_state(adj_sp->state));
		cust_printf(fd, "  TRA:  %s%s    T19: %s\n", (adj_sp->tra & GOT) ? "GOT " : "", (adj_sp->tra & SENT) ? "SENT" : "",
				(adj_sp->timer_t19 > -1) ? "running" : "not running");

		cust_printf(fd, "  Routes:\n");
		cust_printf(fd, "    DPC       State        T6       T10\n");
		cur = adj_sp->routes;
		while (cur) {
			ss7_pc_to_str(ss7->switchtype, cur->dpc, pc_str);
			cust_printf(fd, "%s %12s%10s%10s\n", pc_str, route_state(cur->state), (cur->t6 > -1) ? "running" : "-",
					(cur->t10 > -1) ? "running" : "-");
			cur = cur->next;
		}
		for (i = 0; i < adj_sp->numlinks; i++) {
			link = adj_sp->links[i];
			timers[0] = '\0';
			p = timers;
			for (x = 0; x < MTP3_MAX_TIMERS; x++) {
				if (link->mtp3_timer[x] > -1) {
					strcpy(p, mtp3_timer2str(x));
					p += strlen(p);
					sprintf(p, "(%lis)%c", ss7->ss7_sched[ss7->links[i]->mtp3_timer[x]].when.tv_sec - time(NULL),
						ss7->ss7_sched[ss7->links[i]->mtp3_timer[x]].callback ? ' ' : '!');
					p += strlen(p);
				}
			}

			ss7_pc_to_str(ss7->switchtype, link->adj_sp->adjpc, pc_str);
			cust_printf(fd, "  Link ADJ_PC:SLC: %s:%i\n", pc_str, link->slc);
			cust_printf(fd, "    State:      %s,  %s\n", linkstate2strext(link->state), mtp2state2str(ss7, link));
			cust_printf(fd, "    STD Test:  %s\n", link->std_test_passed ? "passed" : "failed");
			cust_printf(fd, "    Got, sent :%s\n", got_sent2str(got_sent_buf, link->got_sent_netmsg));
			cust_printf(fd, "    Inhibit:    %s%s\n", (link->inhibit & INHIBITED_LOCALLY) ? "Locally " : "        ", 
					(ss7->links[i]->inhibit & INHIBITED_REMOTELY) ? "Remotely" : "");
			cust_printf(fd, "    Changeover: %s\n", changeover2str(link->changeover));
			cust_printf(fd, "    Tx buffer:  %i\n", len_buf(link->tx_buf));
			cust_printf(fd, "    Tx queue:   %i\n", len_buf(link->tx_q));
			cust_printf(fd, "    Retrans pos %i\n", len_buf(link->retransmit_pos));
			cust_printf(fd, "    CO buffer:  %i\n", len_buf(link->co_buf));
			cust_printf(fd, "    CB buffer:  %i\n", len_buf(link->cb_buf));
			cust_printf(fd, "    Last FSN:   %i\n", link->lastfsnacked);
			cust_printf(fd, "    MTP3timers: %s\n", timers);
		} /* links */
	} /* sps */
}

