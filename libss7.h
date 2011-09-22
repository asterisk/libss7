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

#ifndef _LIBSS7_H
#define _LIBSS7_H

/* Internal -- MTP2 events */
#define SS7_EVENT_UP		1	/*!< SS7 link up */
#define SS7_EVENT_DOWN		2	/*!< SS7 link down */
#define MTP2_LINK_UP		3	/*!< MTP layer 2 up */
#define MTP2_LINK_DOWN		4	/*!< MTP layer 2 down */
#define ISUP_EVENT_IAM		5	/*!< Initial address */
#define ISUP_EVENT_ACM		6	/*!< Address complete */
#define ISUP_EVENT_ANM		7	/*!< Answer */
#define ISUP_EVENT_REL		8	/*!< Release */
#define ISUP_EVENT_RLC		9	/*!< Release complete */
#define ISUP_EVENT_GRS		10	/*!< Circuit group reset */
#define ISUP_EVENT_GRA		11	/*!< Circuit group reset acknowledgement */
#define ISUP_EVENT_CON		12	/*!< Connect */
#define ISUP_EVENT_COT		13	/*!< Continuity */
#define ISUP_EVENT_CCR		14	/*!< Continuity check request */
#define ISUP_EVENT_BLO		15	/*!< Blocking */
#define ISUP_EVENT_UBL		16	/*!< Unblocking */
#define ISUP_EVENT_BLA		17	/*!< Blocking acknowledgement */
#define ISUP_EVENT_UBA		18	/*!< Unblocking acknowledgement */
#define ISUP_EVENT_CGB		19	/*!< Circuit group blocking */
#define ISUP_EVENT_CGU		20	/*!< Circuit group unblocking */
#define ISUP_EVENT_CGBA		19	/*!< Circuit group blocking acknowledgement (Not used) */
#define ISUP_EVENT_CGUA		20	/*!< Circuit group unblocking acknowledgement (Not used) */
#define ISUP_EVENT_RSC		21	/*!< Reset circuit */
#define ISUP_EVENT_CPG		22	/*!< Call progress */
#define ISUP_EVENT_UCIC		23	/*!< Unequipped CIC (national use) */
#define ISUP_EVENT_LPA 		24	/*!< Loop back acknowledgement (national use) */
#define ISUP_EVENT_CQM 		25	/*!< Circuit group query (national use) */
#define ISUP_EVENT_FAR		26	/*!< Facility request */
#define ISUP_EVENT_FAA		27	/*!< Facility accepted */
#define ISUP_EVENT_CVT		28	/*!< ???Used??? */
#define ISUP_EVENT_CVR		29	/*!< Not used */
#define ISUP_EVENT_SUS		30	/*!< Suspend */
#define ISUP_EVENT_RES		31	/*!< Resume */

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

/* Nature of Address Indicator */
#define SS7_NAI_SUBSCRIBER             	0x01
#define SS7_NAI_UNKNOWN			0x02
#define SS7_NAI_NATIONAL               	0x03
#define SS7_NAI_INTERNATIONAL          	0x04

/* Charge Number Nature of Address Indicator ANSI */
#define SS7_ANI_CALLING_PARTY_SUB_NUMBER			0x01	/* ANI of the calling party; subscriber number */
#define SS7_ANI_NOTAVAIL_OR_NOTPROVIDED				0x02	/* ANI not available or not provided */
#define SS7_ANI_CALLING_PARTY_NATIONAL_NUMBER			0x03	/* ANI of the calling party; national number */
#define SS7_ANI_CALLED_PARTY_SUB_NUMBER				0x05	/* ANI of the called party; subscriber number */
#define SS7_ANI_CALLED_PARTY_NOT_PRESENT  			0x06	/* ANI of the called party; no number present */
#define SS7_ANI_CALLED_PARTY_NATIONAL_NUMBER 			0x07	/* ANT of the called patty; national number */

/* Address Presentation */
#define SS7_PRESENTATION_ALLOWED                       0x00
#define SS7_PRESENTATION_RESTRICTED                    0x01
#define SS7_PRESENTATION_ADDR_NOT_AVAILABLE            0x02

/* Screening */
#define SS7_SCREENING_USER_PROVIDED_NOT_VERIFIED       0x00
#define SS7_SCREENING_USER_PROVIDED                    0x01
#define SS7_SCREENING_NETWORK_PROVIDED_FAILED          0x02
#define SS7_SCREENING_NETWORK_PROVIDED                 0x03

/* CPG parameter types */
#define CPG_EVENT_ALERTING	0x01
#define CPG_EVENT_PROGRESS	0x02
#define CPG_EVENT_INBANDINFO	0x03
#define CPG_EVENT_CFB		0x04
#define CPG_EVENT_CFNR		0x05
#define CPG_EVENT_CFU		0x06

/* SS7 transport types */
#define SS7_TRANSPORT_DAHDIDCHAN	0
#define SS7_TRANSPORT_DAHDIMTP2		1
#define SS7_TRANSPORT_TCP		2

struct ss7;
struct isup_call;

typedef struct {
	int e;
	int cic;
	int transcap;
	int cot_check_required;
	char called_party_num[50];
	unsigned char called_nai;
	char calling_party_num[50];
	unsigned char calling_party_cat;
	unsigned char calling_nai;
	unsigned char presentation_ind;
	unsigned char screening_ind;
	char charge_number[50];
	unsigned char charge_nai;
	unsigned char charge_num_plan;
	unsigned char gen_add_num_plan;
	unsigned char gen_add_nai;
	char gen_add_number[50];
	unsigned char gen_add_pres_ind;
	unsigned char gen_add_type;
	char gen_dig_number[50];
	unsigned char gen_dig_type;
	unsigned char gen_dig_scheme;
	char jip_number[50];
	unsigned char lspi_type;
	unsigned char lspi_scheme;
	unsigned char lspi_context;
	unsigned char lspi_spare;
	char lspi_ident[50];
	/* If orig_called_num contains a valid number, consider the other orig_called* values valid */
	char orig_called_num[50];
	unsigned char orig_called_nai;
	unsigned char orig_called_pres_ind;
	unsigned char orig_called_screening_ind;
	char redirecting_num[50];
	unsigned char redirecting_num_nai;
	unsigned char redirecting_num_presentation_ind;
	unsigned char redirecting_num_screening_ind;
	unsigned char generic_name_typeofname;
	unsigned char generic_name_avail;
	unsigned char generic_name_presentation;
	char generic_name[50];
	int oli_ani2;
	unsigned int opc;
	
	struct isup_call *call;
} ss7_event_iam;

typedef struct {
	int e;
	int cic;
	int cause;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_rel;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
} ss7_event_ciconly;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_con;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_rsc;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_anm;

typedef struct {
	int e;
	int cic;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	unsigned int opc;
	struct isup_call *call;
	/* Backward call indicator */
	unsigned char called_party_status_ind;
} ss7_event_acm;

typedef struct {
	int e;
	int startcic;
	int endcic;
	int type;
	unsigned int opc;
	unsigned char status[255];
} ss7_event_cicrange;

typedef struct {
	int e;
	int cic;
	int passed;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_cot;

typedef struct {
	int e;
	unsigned int data;
} ss7_event_generic;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	unsigned char event;
} ss7_event_cpg;

typedef struct {
	int e;
	int cic;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_faa;

typedef struct {
	int e;
	int cic;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_far;

typedef struct {
	int e;
	int cic;
	int network_isdn_indicator;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_sus_res;


typedef union {
	int e;
	ss7_event_generic gen;
	ss7_event_iam iam;
	ss7_event_cicrange grs;
	ss7_event_cicrange cqm;
	ss7_event_cicrange gra;
	ss7_event_cicrange cgb;
	ss7_event_cicrange cgu;
	ss7_event_cicrange cgba;
	ss7_event_cicrange cgua;
	ss7_event_rel rel;
	ss7_event_ciconly rlc;
	ss7_event_anm anm;
	ss7_event_acm acm;
	ss7_event_faa faa;
	ss7_event_far far;
	ss7_event_con con;
	ss7_event_cot cot;
	ss7_event_ciconly ccr;
	ss7_event_ciconly cvt;
	ss7_event_ciconly blo;
	ss7_event_ciconly ubl;
	ss7_event_ciconly bla;
	ss7_event_ciconly uba;
	ss7_event_ciconly ucic;
	ss7_event_rsc rsc;
	ss7_event_cpg cpg;
	ss7_event_sus_res sus;
	ss7_event_sus_res res;
	ss7_event_ciconly lpa;
} ss7_event;

void ss7_set_message(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_error(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_debug(struct ss7 *ss7, unsigned int flags);

/* SS7 Link control related functions */
int ss7_schedule_run(struct ss7 *ss7);

struct timeval *ss7_schedule_next(struct ss7 *ss7);

int ss7_add_link(struct ss7 *ss7, int transport, int fd);

int ss7_set_adjpc(struct ss7 *ss7, int fd, unsigned int pc);

int ss7_set_slc(struct ss7 *ss7, int fd, unsigned int slc);

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

const char *ss7_get_version(void);

int ss7_pollflags(struct ss7 *ss7, int fd);

/* ISUP call related message functions */

/* Send an IAM */
int isup_iam(struct ss7 *ss7, struct isup_call *c);

int isup_anm(struct ss7 *ss7, struct isup_call *c);

int isup_con(struct ss7 *ss7, struct isup_call *c);

struct isup_call * isup_new_call(struct ss7 *ss7);

int isup_acm(struct ss7 *ss7, struct isup_call *c);

int isup_faa(struct ss7 *ss7, struct isup_call *c);

int isup_far(struct ss7 *ss7, struct isup_call *c);

int isup_rel(struct ss7 *ss7, struct isup_call *c, int cause);

int isup_rlc(struct ss7 *ss7, struct isup_call *c);

int isup_cpg(struct ss7 *ss7, struct isup_call *c, int event);

int isup_lpa(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_gra(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc);

int isup_grs(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc);

int isup_cgb(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char state[], int type);

int isup_cgu(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char state[], int type);

int isup_cgba(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char state[], int type);

int isup_cgua(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char state[], int type);

int isup_blo(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_ubl(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_ccr(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_bla(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_ucic(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_uba(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_rsc(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_cvr(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_cqr(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char status[]);

/* Various call related sets */
void isup_init_call(struct ss7 *ss7, struct isup_call *c, int cic, unsigned int dpc);

void isup_set_call_dpc(struct isup_call *c, unsigned int dpc);

void isup_set_called(struct isup_call *c, const char *called, unsigned char called_nai, const struct ss7 *ss7);

void isup_set_calling(struct isup_call *c, const char *calling, unsigned char calling_nai, unsigned char presentation_ind, unsigned char screening_ind);

void isup_set_charge(struct isup_call *c, const char *charge, unsigned char charge_nai, unsigned char charge_num_plan);

void isup_set_oli(struct isup_call *c, int oli_ani2);

void isup_set_gen_address(struct isup_call *c, const char *gen_number, unsigned char gen_add_nai, unsigned char gen_pres_ind, unsigned char gen_num_plan, unsigned char gen_add_type);

void isup_set_gen_digits(struct isup_call *c, const char *gen_number, unsigned char gen_dig_type, unsigned char gen_dig_scheme);

enum {
	GEN_NAME_PRES_ALLOWED = 0,
	GEN_NAME_PRES_RESTRICTED = 1,
	GEN_NAME_PRES_BLOCKING_TOGGLE = 2,
	GEN_NAME_PRES_NO_INDICATION = 3,
};

enum {
	GEN_NAME_AVAIL_AVAILABLE = 0,
	GEN_NAME_AVAIL_NOT_AVAILABLE = 1
};

enum {
	GEN_NAME_TYPE_CALLING_NAME = 1,
	GEN_NAME_TYPE_ORIG_CALLED_NAME = 2,
	GEN_NAME_TYPE_REDIRECTING_NAME = 3,
	GEN_NAME_TYPE_CONNECTED_NAME = 4,
};

void isup_set_generic_name(struct isup_call *c, const char *generic_name, unsigned int typeofname, unsigned int availability, unsigned int presentation);

void isup_set_jip_digits(struct isup_call *c, const char *jip_number);

void isup_set_lspi(struct isup_call *c, const char *lspi_ident, unsigned char lspi_type, unsigned char lspi_scheme, unsigned char lspi_context);

void isup_set_callref(struct isup_call *c, unsigned int call_ref_ident, unsigned int call_ref_pc);

/* End of call related sets */



#endif /* _LIBSS7_H */
