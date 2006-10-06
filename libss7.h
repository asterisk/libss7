/*
libss7: An implementation of Signaling System 7

Written by Matthew Fredrickson <creslin@digium.com>

Copyright Digium, Inc. (C), 2006
All Rights Reserved.

This program is free software; you can redistribute it under the
terms of the GNU General Public License as published by the Free
Software Foundation

*/
#ifndef _LIBSS7_H
#define _LIBSS7_H

/* Internal -- MTP2 events */
#define SS7_EVENT_UP		1
#define SS7_EVENT_DOWN		2
#define MTP2_LINK_UP		3
#define MTP2_LINK_DOWN		4
#define ISUP_EVENT_IAM		5
#define ISUP_EVENT_ACM		6
#define ISUP_EVENT_ANM		7
#define ISUP_EVENT_REL		8
#define ISUP_EVENT_RLC		9
/* Circuit group reset */
#define ISUP_EVENT_GRS		10
#define ISUP_EVENT_GRA		11
#define ISUP_EVENT_CON		12
#define ISUP_EVENT_COT		13
#define ISUP_EVENT_CCR		14
#define ISUP_EVENT_BLO		15
#define ISUP_EVENT_UBL		16
#define ISUP_EVENT_BLA		17
#define ISUP_EVENT_UBA		18
#define ISUP_EVENT_CGB		19
#define ISUP_EVENT_CGU		20
#define ISUP_EVENT_CGBA		19
#define ISUP_EVENT_CGUA		20
#define ISUP_EVENT_RSC		21
#define ISUP_EVENT_CPG		22

/* Different SS7 types */
#define SS7_ITU		(1 << 0)
#define SS7_ANSI	(1 << 1)

/* Debug levels */
#define SS7_DEBUG_MTP2	(1 << 0)
#define SS7_DEBUG_MTP3	(1 << 1)
#define SS7_DEBUG_ISUP	(1 << 2)

/* Network indicator */
#define SS7_NI_INT			0x00
#define SS7_NI_INT_SPARE		0x01
#define SS7_NI_NAT			0x02
#define SS7_NI_NAT_SPARE		0x03

/* CPG parameter types */
#define CPG_EVENT_ALERTING	0x01
#define CPG_EVENT_PROGRESS	0x02
#define CPG_EVENT_INBANDINFO	0x03
#define CPG_EVENT_CFB		0x04
#define CPG_EVENT_CFNR		0x05
#define CPG_EVENT_CFU		0x06

struct ss7;
struct isup_call;

typedef struct {
	int e;
	int cic;
	int transcap;
	int cot_check_required;
	char called_party_num[50];
	char calling_party_num[50];
	struct isup_call *call;
} ss7_event_iam;

typedef struct {
	int e;
	int cic;
	struct isup_call *call;
} ss7_event_rel;

typedef struct {
	int e;
	int cic;
} ss7_event_ciconly;

typedef struct {
	int e;
	int cic;
	struct isup_call *call;
} ss7_event_con;

typedef struct {
	int e;
	int cic;
	struct isup_call *call;
} ss7_event_rsc;

typedef struct {
	int e;
	int cic;
	struct isup_call *call;
} ss7_event_anm;

typedef struct {
	int e;
	int cic;
	struct isup_call *call;
} ss7_event_acm;

typedef struct {
	int e;
	int startcic;
	int endcic;
	unsigned char status[255];
} ss7_event_cicrange;

typedef struct {
	int e;
	int cic;
	int passed;
	struct isup_call *call;
} ss7_event_cot;

typedef struct {
	int e;
	unsigned int data;
} ss7_event_generic;

typedef struct {
	int e;
	int cic;
	unsigned char event;
} ss7_event_cpg;

typedef union {
	int e;
	ss7_event_generic gen;
	ss7_event_iam iam;
	ss7_event_cicrange grs;
	ss7_event_cicrange gra;
	ss7_event_cicrange cgb;
	ss7_event_cicrange cgu;
	ss7_event_cicrange cgba;
	ss7_event_cicrange cgua;
	ss7_event_rel rel;
	ss7_event_ciconly rlc;
	ss7_event_anm anm;
	ss7_event_acm acm;
	ss7_event_con con;
	ss7_event_cot cot;
	ss7_event_ciconly ccr;
	ss7_event_ciconly blo;
	ss7_event_ciconly ubl;
	ss7_event_ciconly bla;
	ss7_event_ciconly uba;
	ss7_event_rsc rsc;
	ss7_event_cpg cpg;
} ss7_event;

void ss7_set_message(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_error(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_debug(struct ss7 *ss7, unsigned int flags);

/* SS7 Link control related functions */
int ss7_schedule_run(struct ss7 *ss7);

struct timeval *ss7_schedule_next(struct ss7 *ss7);

int ss7_add_link(struct ss7 *ss7, int fd);

int ss7_set_adjpc(struct ss7 *ss7, int fd, unsigned int pc);

int ss7_set_network_ind(struct ss7 *ss7, int ni);

int ss7_set_pc(struct ss7 *ss7, unsigned int pc);

int ss7_set_default_dpc(struct ss7 *ss7, unsigned int pc);

struct ss7 *ss7_new(int switchtype);

ss7_event *ss7_check_event(struct ss7 *ss7);

int ss7_start(struct ss7 *ss7);

int ss7_read(struct ss7 *ss7, int fd);

int ss7_write(struct ss7 *ss7, int fd);

void ss7_link_alarm(struct ss7 *ss7, int fd);

void ss7_link_noalarm(struct ss7 *ss7, int fd);

char * ss7_event2str(int event);

/* ISUP call related message functions */

/* Send an IAM */
int isup_iam(struct ss7 *ss7, struct isup_call *c);

int isup_anm(struct ss7 *ss7, struct isup_call *c);

int isup_con(struct ss7 *ss7, struct isup_call *c);

struct isup_call * isup_new_call(struct ss7 *ss7);

int isup_acm(struct ss7 *ss7, struct isup_call *c);

int isup_rel(struct ss7 *ss7, struct isup_call *c, int cause);

int isup_rlc(struct ss7 *ss7, struct isup_call *c);

int isup_cpg(struct ss7 *ss7, struct isup_call *c, int event);

int isup_gra(struct ss7 *ss7, int begincic, int endcic);

int isup_grs(struct ss7 *ss7, int begincic, int endcic);

int isup_cgb(struct ss7 *ss7, int begincic, int endcic, unsigned char state[]);

int isup_cgu(struct ss7 *ss7, int begincic, int endcic, unsigned char state[]);

int isup_cgba(struct ss7 *ss7, int begincic, int endcic, unsigned char state[]);

int isup_cgua(struct ss7 *ss7, int begincic, int endcic, unsigned char state[]);

int isup_blo(struct ss7 *ss7, int cic);

int isup_ubl(struct ss7 *ss7, int cic);

int isup_bla(struct ss7 *ss7, int cic);

int isup_uba(struct ss7 *ss7, int cic);

int isup_rsc(struct ss7 *ss7, int cic);

void isup_init_call(struct ss7 *ss7, struct isup_call *c, int cic, char *calledpartynum, char *callingpartynum);
#endif /* _LIBSS7_H */
