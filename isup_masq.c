/*
libss7: An implementation of Signalling System 7

Written by Matthew Fredrickson <creslin@digium.com>

Copyright Digium, Inc. (C) 2006
All Rights Reserved.

This program is free software; you can redistribute it under the
terms of the GNU General Public License as published by the Free
Software Foundation

Contains definitions and data structurs for the ISUP portion of SS7
*/


#include "libss7.h"
//#include "isuptcp.h"
#include "ss7_internal.h"
#include "isup.h"
#include "mtp3.h"
#include "mtp2.h"

#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


static void isup_set_event(struct ss7 *ss7, int event)
{
	/* A little worker function to put events into the event q */
	
	ss7_event *e = ss7_next_empty_event(ss7);
	e->e = event;
	
	return;
}	

static int isup_ip_dump(struct mtp2 *link, char prefix, unsigned char * msg, int len)
{

	if (!(link->master->debug & SS7_DEBUG_MTP2))
		return 0;

	ss7_dump_msg(link->master, msg, len);

	ss7_message(link->master, "%c\n", prefix);

#if 0
	if (prefix == '<')
		mtp3_dump(link->master, NULL, &msg[1], len - 1);
	else
		mtp3_dump(link->master, NULL, &msg[0], len);
#endif
	mtp3_dump(link->master, NULL, &msg[1], len - 1);

	return 0;
}

static int isup_ip_receive_buffer(struct mtp2 *mtp2, unsigned char *msg, int len)
{  
	unsigned char *buf = (unsigned char *)msg;
	int msgstate = (int) buf[0];
	unsigned char *newmsg;
	
	/*Finds second byte of msg and determines how to handle message. Used to let client know
      if the ss7 link is down on server. SS7 socket server will send the
	correct state byte for each message. It does not need to keep sending the UP or DOWN states. Only
	needs to send them when it's own ss7 link is in alarm. So during normal UP state all messages will 
	be of CASE 5 */
	
	switch (msgstate) {
		case SS7_EVENT_UP:
			isup_set_event(mtp2->master, SS7_EVENT_UP);
			return 1;
		case SS7_EVENT_DOWN:
			isup_set_event(mtp2->master, SS7_EVENT_DOWN);
			return 1;
		case 3:	
		case 4:
			return 0;
		case 5: /*This is the case for all ISUP messages such as IAM's, RLC,ANMs,ACMs..etc */
			newmsg = &buf[1]; /*Get the buf minus the event byte */
			isup_ip_dump(mtp2, '<', msg, len);
			mtp3_receive(mtp2->master, mtp2, (void *)newmsg, len-1);
			return 1; 	
		default:
			return 0;
	}
}	

static void isup_ip_link_down(struct mtp2 *mtp2)
{
	ss7_event *e = ss7_next_empty_event(mtp2->master);

	if (!e) {
		ss7_message(mtp2->master, "Could not allocate new event in isup_ip_link_down\n");
		return;
	}

	e->e = MTP2_LINK_DOWN;
	e->gen.data = mtp2->fd;

	mtp2->fd = -1;
}

static int get_msg_length(struct mtp2 *mtp2)
{
	unsigned char lenbuf[2];
	int res;

	res = read(mtp2->fd, lenbuf, sizeof(lenbuf));

	if (res == 0) {
		isup_ip_link_down(mtp2);
		return 0;
	}

	if (res != 2) {
		ss7_message(mtp2->master, "Read len of %d:%s\n", res, strerror(errno));
		return -1;
	}

	return (lenbuf[0] << 8) | lenbuf[1];
}

int isup_ip_receive(struct mtp2 *mtp2)
{
	unsigned char msg[1024];
	int len;
	int res = 0;

	if (mtp2->tcpreadstate == TCPSTATE_NEED_LEN) {
		len = get_msg_length(mtp2);

		if (len < 0)
			return -1;

		if (len > 277) {
			ss7_message(mtp2->master, "Got read length of %d?!\n", len);
			return 0;
		}

		mtp2->tcplen = len;
		mtp2->tcpreadstate = TCPSTATE_NEED_MSG;

	} else {
		len = mtp2->tcplen;

		res = read(mtp2->fd, msg, len);

		if ((res < 0) && (errno == 104)) {
			ss7_message(mtp2->master, "Link dropped\n");
		}


#if 0
	if (res == 0) {
		isup_ip_link_down(mtp2);
	}
#endif
		if (res < 0) {
			ss7_message(mtp2->master, "Read problem from socket: %s\n", strerror(errno));
			return -1;
		}

		if (res != len) {
			ss7_message(mtp2->master, "Short read from socket (%d): %s\n", res, strerror(errno));
			return -1;
		}

		msg[res] = 0;
		
		res = isup_ip_receive_buffer(mtp2, msg, len);

		mtp2->tcpreadstate = TCPSTATE_NEED_LEN;
	}

	return res;
}

#if 0
static unsigned char * itoa(int val, int base)
{
	static unsigned char buf[32] = {0};
	int i = 30;
	
	for(; val && i ; --i, val /= base)
		buf[i] = "0123456789abcdef"[val % base];
	
	return &buf[i+1];
}  
#endif

static int isup_carefulwrite(int fd, unsigned char *s, int len, int timeoutms) 
{
	/*This was copied from AST_CAREFULWRITE
	 Try to write string, but wait no more than ms milliseconds
	   before timing out*/
	int res = 0;
	struct pollfd fds[1];
	while (len) {
		res = write(fd, s, len);
		if ((res < 0) && (errno != EAGAIN)) {
			return -1;
		}
		if (res < 0)
			res = 0;
		len -= res;
		s += res;
		res = 0;
		if (len) {
			fds[0].fd = fd;
			fds[0].events = POLLOUT;
			/*Wait until writable again */
			res = poll(fds, 1, timeoutms);
			if (res < 1)
				return -1;
		}
	}
	return res;
}

static int isup_ip_send(struct mtp2 *mtp2, unsigned char *data, int len)
{
	int i;
	unsigned char lenbuf[2];

	isup_ip_dump(mtp2, '>', data, len);

	lenbuf[0] = (len >> 8) & 0xff;
	lenbuf[1] = len & 0xff;
	
	/* Send the length of entire ISUP message first */
	i = isup_carefulwrite(mtp2->fd, lenbuf, 2, 50);
	
	return isup_carefulwrite(mtp2->fd, data, len, 50);
}

int isup_ip_msu(struct mtp2 *mtp2, struct ss7_msg *msg)
{
	int res;

	msg->buf[MTP2_SIZE - 1] = 5;
	
	res = isup_ip_send(mtp2, msg->buf + MTP2_SIZE - 1, msg->size - MTP2_SIZE + 1);

	ss7_msg_free(msg);
	
	return res;
}

int isup_masquerade_set_route_fd(struct ss7 *ss7, int fd, int startcic, int endcic, unsigned int opc)
{
	struct isup_masq *masq_table = &ss7->isup_masq_table;
	int i;

	for (i = 0; i < masq_table->numentries; i++) {
		if ((masq_table->routes[i].startcic == startcic)
			&& (masq_table->routes[i].endcic == endcic)
			&& (masq_table->routes[i].opc == opc)) {

			masq_table->routes[i].mtp2->fd = fd;
			return 0;
		}
	}

	return -1;
}

int isup_masquerade_add_route(struct ss7 *ss7, int startcic, int endcic, unsigned int opc)
{
	struct isup_masq *masq_table = &ss7->isup_masq_table;
	int i;

	if (!ss7)
		return -1;

	if (masq_table->numentries >= ISUP_MASQ_MAX_ENTRIES)
		return -1;

	i = masq_table->numentries++;

	masq_table->routes[i].startcic = startcic;
	masq_table->routes[i].endcic = endcic;
	masq_table->routes[i].opc = opc;

	masq_table->routes[i].mtp2 = ss7_add_slave_link(ss7, SS7_TRANSPORT_TCP, "slave_link", 0, 0);

	if (!masq_table->routes[i].mtp2)
		return -1;

	return 0;
}

static int isup_masquerade_transmit(struct mtp2 *link, struct routing_label *rl, unsigned char *buf, int len)
{
	/* Rebuild our message here */
	unsigned char txbuf[512];
	int rlsize;

	txbuf[0] = 5; /* ISUP MESSAGE */
	txbuf[1] = (link->master->ni << 6) | SIG_ISUP;
	rlsize = set_routinglabel(&txbuf[2], rl);

	memcpy(&txbuf[2 + rlsize], buf, len);

	isup_ip_send(link, txbuf, len + 2 + rlsize);

	return 0;
}

int isup_needs_masquerade(struct ss7 *ss7, struct routing_label *rl, unsigned int cic, unsigned char *buf, int len)
{
	struct isup_masq * isup_masq_table = &ss7->isup_masq_table;
	int i;

	for (i = 0; i <  isup_masq_table->numentries; i++) {
		if ((isup_masq_table->routes[i].opc == rl->opc)
		&& (cic >= isup_masq_table->routes[i].startcic)
		&& (cic <= isup_masq_table->routes[i].endcic)) {
			if (isup_masq_table->routes[i].mtp2->fd != -1) {
				isup_masquerade_transmit(isup_masq_table->routes[i].mtp2, rl, buf, len);
			}
			return 1;
		}
	}

	return 0;
}

