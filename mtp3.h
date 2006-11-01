/*
libss7: An implementation of Signaling System 7

Written by Matthew Fredrickson <matt@fredricknet.net>

Copyright Digium, Inc. (C), 2006
All Rights Reserved.

This program is free software; you can redistribute it under the
terms of the GNU General Public License as published by the Free
Software Foundation

*/
#ifndef _MTP3_H
#define _MTP3_H

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

typedef unsigned int point_code;

struct routing_label {
	unsigned int type;
	point_code dpc;
	point_code opc;
	unsigned char sls;
};

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

unsigned char sls_next(struct ss7 *ss7);

int set_routinglabel(unsigned char *sif, struct routing_label *rl);

unsigned char sls_next(struct ss7 *ss7);

#endif /* _MTP3_H */
