#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "libss7.h"
#include "ss7_internal.h"
#include "mtp2.h"
#include "mtp3.h"
#include "isup.h"

#define mtp_error ss7_error
#define mtp_message ss7_message

char testmessage[] = "2564286288";

#define mtp3_size(ss7) (((ss7)->switchtype == SS7_ITU) ? 5 : 8) 
/* Routing label size */
#define rl_size(ss7) (((ss7)->switchtype == SS7_ITU) ? 4 : 7)

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

unsigned char sls_next(struct ss7 *ss7)
{
	unsigned char res = ss7->sls;
	ss7->sls = (ss7->sls + 1) % 16;
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
			buf[3] = ((rl->opc >> 10) & 0x0f) | (rl->sls & 0xf0);
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

static inline unsigned char  get_h1(unsigned char *byte)
{
	return (((*byte) & 0xf0) >> 4);
}

static inline struct mtp2 * sls_to_link(struct ss7 *ss7, unsigned char sls)
{
	return ss7->links[sls % ss7->numlinks];
}

static int net_mng_dump(struct ss7 *ss7, struct mtp2 *mtp2, unsigned char *buf, int len)
{
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char h1, h0;

	h1 = h0 = *headerptr;

	h1 = get_h1(headerptr);
	h0 = get_h0(headerptr);

	ss7_message(ss7, "\tH0: %x H1: %x\n", h0, h1);
	return 0;
}

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
	rl.sls = link->slc;

	rllen = set_routinglabel(layer4, &rl);
	layer4 += rllen;

	set_h0(layer4, 1);
	set_h1(layer4, 1);
	layer4[1] = (testlen << 4);
	memcpy(&layer4[2], testmessage, testlen);

	ss7_msg_userpart_len(m, rllen + testlen + 2);

	mtp3_transmit(link->master, SIG_STD_TEST, link->slc, m);
}

static int net_mng_receive(struct ss7 *ss7, struct mtp2 *mtp2, unsigned char *buf, int len)
{
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char h1, h0;

	h1 = h0 = *headerptr;

	h1 = get_h1(headerptr);
	h0 = get_h0(headerptr);

	/* Check to see if it's a TRA */
	if ((h0 == 7) && (h1 == 1)) {
		std_test_send(mtp2);
		return 0;
	} else {
		ss7_error(ss7, "!! Unable to handle SIG_NET_MNG message with H1 = %d and H0 = %d\n", h1, h0);
		return -1;
	}
}

static int std_test_receive(struct ss7 *ss7, struct mtp2 *mtp2, unsigned char *buf, int len)
{
	unsigned char *sif = buf;
	unsigned char *headerptr = buf + rl_size(ss7);
	unsigned char h1, h0;
	int testpatsize = 0;
	struct routing_label rl;

	get_routinglabel(ss7->switchtype, sif, &rl);

	if (rl.dpc != ss7->pc)
		goto fail;

	h1 = h0 = *headerptr;

	h1 = get_h1(headerptr);
	h0 = get_h0(headerptr);

	if (h0 != 1)
		goto fail;

	if (h1 == 1) {
		struct ss7_msg *m;
		struct routing_label drl;
		unsigned char *layer4;
		int rllen;

		m = ss7_msg_new();
		if (!m) {
			ss7_error(ss7, "Unable to allocate message buffer!\n");
			return -1;
		}

		drl.type = ss7->switchtype;
		drl.dpc = rl.opc;
		drl.opc = ss7->pc;
		drl.sls = mtp2->slc;

		layer4 = ss7_msg_userpart(m);

		rllen = set_routinglabel(layer4, &drl);
		layer4 += rllen;

		testpatsize = (headerptr[1] >> 4) & 0xf;
		/* Success! */
		set_h0(layer4, 1);
		set_h1(layer4, 2);
		layer4[1] = (testpatsize << 4);
		memcpy(&layer4[2], &headerptr[2], testpatsize);
		
		ss7_msg_userpart_len(m, rllen + testpatsize + 2);

		return mtp3_transmit(ss7, SIG_STD_TEST, mtp2->slc, m);
	} else if (h1 == 2) {
		/* Event Link up */
		ss7_event *e = ss7_next_empty_event(ss7);

		if (!e) {
			mtp_error(ss7, "Event queue full\n");
			return -1;
		}
		e->e = SS7_EVENT_UP;
		return 0;
	} else
		ss7_error(ss7, "Unhandled STD_TEST message: h0 = %x h1 = %x", h0, h1);

fail:
	return -1;
}

static void net_mng_send_tra(struct mtp2 *link)
{
	struct ss7_msg *m;
	unsigned char *layer4;
	struct routing_label rl;
	struct ss7 *ss7 = link->master;
	int rllen = 0;

	m = ss7_msg_new();
	if (!m) {
		ss7_error(link->master, "Malloc failed on ss7_msg!.  Unable to transmit NET_MNG TRA\n");
		return;
	}

	layer4 = ss7_msg_userpart(m);
	rl.type = ss7->switchtype;
	rl.opc = ss7->pc;
	rl.dpc = link->dpc;
	rl.sls = link->slc;

	rllen = set_routinglabel(layer4, &rl);
	layer4 += rllen;

	set_h0(layer4, 7);
	set_h1(layer4, 1);

	ss7_msg_userpart_len(m, rllen + 1);

	mtp3_transmit(link->master, SIG_NET_MNG, link->slc, m);
}

int mtp3_transmit(struct ss7 *ss7, unsigned char userpart, unsigned char sls, struct ss7_msg *m)
{
	unsigned char *sio;
	unsigned char *sif;
	struct mtp2 *winner;

	sio = m->buf + MTP2_SIZE;
	sif = sio + 1;

	winner = sls_to_link(ss7, sls);

	(*sio) = (ss7->ni << 6) | userpart;

	return mtp2_msu(winner, m);
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


	ss7_message(ss7, "\tNetwork Inidicator 0x%x\n", ni);

	rlsize = get_routinglabel(ss7->switchtype, sif, &rl);

	ss7_message(ss7, "\tOPC 0x%x DPC 0x%x\n", rl.opc, rl.dpc);

	ss7_message(ss7, "\tUser Part: %s (%x) Priority: %d\n", userpart2str(userpart), userpart, priority);

	/* Pass it to the correct user part */
	switch (userpart) {
		case SIG_NET_MNG:
		case SIG_STD_TEST:
			return net_mng_dump(ss7, link, sif, siflen);
		case SIG_ISUP:
			return isup_dump(ss7, link, sif + rlsize, siflen - rlsize);
		case SIG_SPEC_TEST:
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
		mtp_error(ss7, "Received MSU with network indicator of %d, but we are %d\n", ss7->ni, ni);
		return -1;
	}

	/* Check point codes to make sure the message is destined for us */
	rlsize = get_routinglabel(ss7->switchtype, sif, &rl);

	if (ss7->pc != rl.dpc) {
		mtp_error(ss7, "Received message destined for point code 0x%x but we're 0x%x.  Dropping\n", rl.dpc, ss7->pc);
		return -1;
	}

	/* TODO: find out what to do with the priority in ANSI networks */

	/* Pass it to the correct user part */
	switch (userpart) {
		case SIG_STD_TEST:
			return std_test_receive(ss7, link, sif, siflen);
		case SIG_ISUP:
			/* Skip the routing label */
			return isup_receive(ss7, link, sif + rlsize, siflen - rlsize);
		case SIG_NET_MNG:
			return net_mng_receive(ss7, link, sif, siflen);
		case SIG_SPEC_TEST:
		case SIG_SCCP:
		default:
			mtp_message(ss7, "Unable to process message destined for userpart %d; dropping message\n", userpart);
			return 0;
	}
}

static void mtp3_event_link_up(struct mtp2 * link)
{
	net_mng_send_tra(link);
}

static struct mtp2 * slc_to_mtp2(struct ss7 *ss7, unsigned int slc)
{
	int i = 0;

	struct mtp2 *link = NULL;
	for (i = 0; i < ss7->numlinks; i++) {
		if (ss7->links[i]->slc == slc)
			link = ss7->links[i];
	}

	return link;
}

ss7_event * mtp3_process_event(struct ss7 *ss7, ss7_event *e)
{
	struct mtp2 *link;

	/* Check to see if there is no event to process */
	if (!e)
		return NULL;

	switch (e->e) {
		case MTP2_LINK_UP:
			link = slc_to_mtp2(ss7, e->gen.data);
			mtp3_event_link_up(link);
			return e;
		default:
			return e;
	}

	return e;
}
