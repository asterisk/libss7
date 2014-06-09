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

/*!
 * Bump this for each libss7 Application Binary Interface (ABI)
 * change that makes the ABI/API incompatible with previous
 * versions.  This is expected to just be the major version of
 * the libss7 branches.
 */
#define LIBSS7_ABI_COMPATIBILITY	2


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
#define ISUP_EVENT_RSC		21	/*!< Reset circuit */
#define ISUP_EVENT_CPG		22	/*!< Call progress */
#define ISUP_EVENT_UCIC		23	/*!< Unequipped CIC (national use) */
#define ISUP_EVENT_LPA		24	/*!< Loop back acknowledgement (national use) */
#define ISUP_EVENT_CQM		25	/*!< Circuit group query (national use) */
#define ISUP_EVENT_FAR		26	/*!< Facility request */
#define ISUP_EVENT_FAA		27	/*!< Facility accepted */
#define ISUP_EVENT_CVT		28	/*!< ???Used??? */
#define ISUP_EVENT_CVR		29	/*!< Not used */
#define ISUP_EVENT_SUS		30	/*!< Suspend */
#define ISUP_EVENT_RES		31	/*!< Resume */
#define ISUP_EVENT_CGBA		32	/*!< Circuit group blocking acknowledgement */
#define ISUP_EVENT_CGUA		33	/*!< Circuit group unblocking acknowledgement */
#define ISUP_EVENT_SAM		34	/*!< Subsequent address */
#define ISUP_EVENT_DIGITTIMEOUT	35	/*!< ISUP T10 expired */
#define ISUP_EVENT_FRJ		36	/*!< Facility rejected */

/* ISUP MSG Flags */
#define ISUP_SENT_GRS	(1 << 0)
#define ISUP_SENT_CGB	(1 << 1)
#define ISUP_SENT_CGU	(1 << 2)
#define ISUP_SENT_RSC	(1 << 3)
#define ISUP_SENT_REL	(1 << 4)
#define ISUP_SENT_BLO	(1 << 5)
#define ISUP_SENT_UBL	(1 << 6)
#define ISUP_SENT_IAM	(1 << 7)
#define ISUP_SENT_FAR	(1 << 8)
#define ISUP_GOT_CCR	(1 << 9)
#define ISUP_GOT_IAM	(1 << 10)
#define ISUP_GOT_ACM	(1 << 11)
#define ISUP_GOT_CON	(1 << 12)
#define ISUP_GOT_ANM	(1 << 13)
#define ISUP_SENT_ACM	(1 << 14)
#define ISUP_GOT_CGB	(1 << 15)
#define ISUP_GOT_CGU	(1 << 16)
#define ISUP_SENT_CON	(1 << 17)
#define ISUP_SENT_ANM	(1 << 18)
#define ISUP_SENT_INR	(1 << 19)
#define ISUP_SENT_GRS2	(1 << 20)
#define ISUP_PENDING_IAM	(1 << 21)

#define ISUP_CALL_CONNECTED	(ISUP_GOT_ACM | ISUP_GOT_ANM | ISUP_GOT_CON |  ISUP_SENT_CON | ISUP_SENT_ACM | ISUP_SENT_ANM)
#define ISUP_CALL_PENDING	(ISUP_GOT_IAM | ISUP_SENT_IAM | ISUP_PENDING_IAM | ISUP_GOT_CCR | ISUP_SENT_INR | ISUP_SENT_FAR)

/* Different SS7 types */
#define SS7_ITU		(1 << 0)
#define SS7_ANSI	(1 << 1)

/* Debug levels */
#define SS7_DEBUG_MTP2	(1 << 0)
#define SS7_DEBUG_MTP3	(1 << 1)
#define SS7_DEBUG_ISUP	(1 << 2)

/* Network indicator */
#define SS7_NI_INT			0x00
#define SS7_NI_INT_SPARE	0x01
#define SS7_NI_NAT			0x02
#define SS7_NI_NAT_SPARE	0x03

/* Nature of Address Indicator */
#define SS7_NAI_SUBSCRIBER		0x01
#define SS7_NAI_UNKNOWN			0x02
#define SS7_NAI_NATIONAL		0x03
#define SS7_NAI_INTERNATIONAL	0x04
#define SS7_NAI_NETWORKROUTED	0x08

/* Charge Number Nature of Address Indicator ANSI */
#define SS7_ANI_CALLING_PARTY_SUB_NUMBER			0x01	/* ANI of the calling party; subscriber number */
#define SS7_ANI_NOTAVAIL_OR_NOTPROVIDED				0x02	/* ANI not available or not provided */
#define SS7_ANI_CALLING_PARTY_NATIONAL_NUMBER		0x03	/* ANI of the calling party; national number */
#define SS7_ANI_CALLED_PARTY_SUB_NUMBER				0x05	/* ANI of the called party; subscriber number */
#define SS7_ANI_CALLED_PARTY_NOT_PRESENT			0x06	/* ANI of the called party; no number present */
#define SS7_ANI_CALLED_PARTY_NATIONAL_NUMBER		0x07	/* ANT of the called patty; national number */

/* Address Presentation */
#define SS7_PRESENTATION_ALLOWED					0x00
#define SS7_PRESENTATION_RESTRICTED					0x01
#define SS7_PRESENTATION_ADDR_NOT_AVAILABLE			0x02

/* Screening */
#define SS7_SCREENING_USER_PROVIDED_NOT_VERIFIED	0x00
#define SS7_SCREENING_USER_PROVIDED					0x01
#define SS7_SCREENING_NETWORK_PROVIDED_FAILED		0x02
#define SS7_SCREENING_NETWORK_PROVIDED				0x03

/* Transmission Medium Requirement */
#define SS7_TMR_SPEECH				0x00
#define SS7_TMR_SPARE				0x01
#define SS7_TMR_64K_UNRESTRICTED	0x02
#define SS7_TMR_3K1_AUDIO			0x03
#define SS7_TMR_N64K_OR_SPARE		0x04

/* CPG parameter types */
#define CPG_EVENT_ALERTING		0x01
#define CPG_EVENT_PROGRESS		0x02
#define CPG_EVENT_INBANDINFO	0x03
#define CPG_EVENT_CFB			0x04
#define CPG_EVENT_CFNR			0x05
#define CPG_EVENT_CFU			0x06

/* SS7 transport types */
#define SS7_TRANSPORT_DAHDIDCHAN	0
#define SS7_TRANSPORT_DAHDIMTP2		1
#define SS7_TRANSPORT_TCP			2

/* What have to do after the hangup */
#define SS7_HANGUP_DO_NOTHING	0
#define SS7_HANGUP_SEND_REL		1
#define SS7_HANGUP_SEND_RSC		2
#define SS7_HANGUP_SEND_RLC		3
#define SS7_HANGUP_FREE_CALL	4
#define SS7_HANGUP_REEVENT_IAM	5

/* Special SS7 Hangupcause */
#define SS7_CAUSE_TRY_AGAIN	256

/* return values from ss7_hangup */
#define SS7_CIC_NOT_EXISTS	0
#define SS7_CIC_USED		1
#define SS7_CIC_IDLE		2

/* Closed user group indicator */
#define ISUP_CUG_NON					0
#define ISUP_CUG_OUTGOING_ALLOWED		2
#define ISUP_CUG_OUTGOING_NOT_ALLOWED	3

/* FLAGS */
#define SS7_INR_IF_NO_CALLING		(1 << 0)	/* request calling num, if the remote party didn't send */
#define SS7_ISDN_ACCESS_INDICATOR	(1 << 1)	/* originating/access indicator */

struct ss7;
struct isup_call;

typedef struct {
	int e;
	int cic;
	int transcap;
	int cot_check_required;
	int cot_performed_on_previous_cic;
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
	unsigned char redirect_counter;
	unsigned char redirect_info;
	unsigned char redirect_info_ind;
	unsigned char redirect_info_orig_reas;
	unsigned char redirect_info_reas;
	unsigned char redirect_info_counter;
	unsigned char generic_name_typeofname;
	unsigned char generic_name_avail;
	unsigned char generic_name_presentation;
	unsigned char echocontrol_ind;
	char generic_name[50];
	int oli_ani2;
	unsigned char cug_indicator;
	char cug_interlock_ni[5];
	unsigned short cug_interlock_code;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_iam;

typedef struct {
	int e;
	int cic;
	int cause;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_rel;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_cic;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
	char connected_num[50];
	unsigned char connected_nai;
	unsigned char connected_presentation_ind;
	unsigned char connected_screening_ind;
	unsigned char echocontrol_ind;
} ss7_event_con;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_rsc;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	char connected_num[50];
	unsigned char connected_nai;
	unsigned char connected_presentation_ind;
	unsigned char connected_screening_ind;
	unsigned long got_sent_msg;
	unsigned char echocontrol_ind;
	struct isup_call *call;
} ss7_event_anm;

typedef struct {
	int e;
	int cic;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
	/* Backward call indicator */
	unsigned char called_party_status_ind;
	unsigned char echocontrol_ind;
} ss7_event_acm;

typedef struct {
	int e;
	int startcic;
	int endcic;
	int sent_endcic;
	int type;
	int sent_type;
	unsigned int opc;
	unsigned char status[255];
	unsigned int sent_status[255];
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_cicrange;

typedef struct {
	int e;
	int cic;
	int passed;
	int cot_performed_on_previous_cic;
	unsigned int opc;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_cot;

typedef struct {
	int e;
	int data;
} ss7_event_generic;

typedef struct {
	int e;
	struct mtp2 *link;
} ss7_event_link;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	unsigned char event;
	unsigned long got_sent_msg;
	unsigned char echocontrol_ind;
	unsigned char connected_nai;
	unsigned char connected_presentation_ind;
	unsigned char connected_screening_ind;
	char connected_num[50];
	struct isup_call *call;
} ss7_event_cpg;

typedef struct {
	int e;
	int cic;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	unsigned int opc;
	struct isup_call *call;
} ss7_event_frj;

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
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_sus_res;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	char called_party_num[50];
	unsigned char called_nai;
	int cot_check_required;
	int cot_check_passed;
	int cot_performed_on_previous_cic;
	unsigned long got_sent_msg;
	struct isup_call *call;
} ss7_event_sam;

typedef struct {
	int e;
	int cic;
	unsigned int opc;
	int cot_check_required;
	int cot_check_passed;
	int cot_performed_on_previous_cic;
	struct isup_call *call;
} ss7_event_digittimeout;


typedef union {
	int e;
	ss7_event_generic gen;
	ss7_event_link link;
	ss7_event_iam iam;
	ss7_event_cicrange grs;
	ss7_event_cicrange cqm;
	ss7_event_cicrange gra;
	ss7_event_cicrange cgb;
	ss7_event_cicrange cgu;
	ss7_event_cicrange cgba;
	ss7_event_cicrange cgua;
	ss7_event_rel rel;
	ss7_event_cic rlc;
	ss7_event_anm anm;
	ss7_event_acm acm;
	ss7_event_frj frj;
	ss7_event_faa faa;
	ss7_event_far far;
	ss7_event_con con;
	ss7_event_cot cot;
	ss7_event_cic ccr;
	ss7_event_cic cvt;
	ss7_event_cic blo;
	ss7_event_cic ubl;
	ss7_event_cic bla;
	ss7_event_cic uba;
	ss7_event_cic ucic;
	ss7_event_rsc rsc;
	ss7_event_cpg cpg;
	ss7_event_sus_res sus;
	ss7_event_sus_res res;
	ss7_event_cic lpa;
	ss7_event_sam sam;
	ss7_event_digittimeout digittimeout;
} ss7_event;

void ss7_set_message(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_error(void (*func)(struct ss7 *ss7, char *message));

void ss7_set_debug(struct ss7 *ss7, unsigned int flags);

void ss7_set_notinservice(void (*func)(struct ss7 *ss7, int cic, unsigned int dpc));

void ss7_set_hangup(int (*func)(struct ss7 *ss7, int cic, unsigned int dpc, int cause, int do_hangup));

void ss7_set_call_null(void (*func)(struct ss7 *ss7, struct isup_call *c, int lock));

/* SS7 Link control related functions */
int ss7_schedule_run(struct ss7 *ss7);

struct timeval *ss7_schedule_next(struct ss7 *ss7);

int ss7_add_link(struct ss7 *ss7, int transport, int fd, int slc, unsigned int adjpc);

int ss7_set_network_ind(struct ss7 *ss7, int ni);

int ss7_set_pc(struct ss7 *ss7, unsigned int pc);

int ss7_set_default_dpc(struct ss7 *ss7, unsigned int pc);

struct ss7 *ss7_new(int switchtype);

void ss7_destroy(struct ss7 *ss7);

void ss7_set_cause_location(struct ss7 *ss7, unsigned char location);

void ss7_set_sls_shift(struct ss7 *ss7, unsigned char shift);

void ss7_set_flags(struct ss7 *ss7, unsigned int flags);

void ss7_clear_flags(struct ss7 *ss7, unsigned int flags);

ss7_event *ss7_check_event(struct ss7 *ss7);

int ss7_start(struct ss7 *ss7);

int ss7_read(struct ss7 *ss7, int fd);

int ss7_write(struct ss7 *ss7, int fd);

void ss7_link_alarm(struct ss7 *ss7, int fd);

void ss7_link_noalarm(struct ss7 *ss7, int fd);

char * ss7_event2str(int event);

const char *ss7_get_version(void);

int ss7_pollflags(struct ss7 *ss7, int fd);

int ss7_set_mtp3_timer(struct ss7 *ss7, char *name, int ms);

/* ISUP call related message functions */
int ss7_set_isup_timer(struct ss7 *ss7, char *name, int ms);

struct isup_call * isup_free_call_if_clear(struct ss7 *ss7, struct isup_call *c);

int isup_start_digittimeout(struct ss7 *ss7, struct isup_call *c);

/* Send an IAM */
int isup_iam(struct ss7 *ss7, struct isup_call *c);

int isup_inr(struct ss7 *ss7, struct isup_call *c, unsigned char ind0, unsigned char ind1);

int isup_inf(struct ss7 *ss7, struct isup_call *c, unsigned char ind0, unsigned char ind1);

int isup_anm(struct ss7 *ss7, struct isup_call *c);

int isup_con(struct ss7 *ss7, struct isup_call *c);

struct isup_call * isup_new_call(struct ss7 *ss7, int cic, unsigned int dpc, int outgoing);

int isup_acm(struct ss7 *ss7, struct isup_call *c);

int isup_frj(struct ss7 *ss7, struct isup_call *c);

int isup_faa(struct ss7 *ss7, struct isup_call *c);

int isup_far(struct ss7 *ss7, struct isup_call *c);

int isup_rel(struct ss7 *ss7, struct isup_call *c, int cause);

int isup_rlc(struct ss7 *ss7, struct isup_call *c);

int isup_sus(struct ss7 *ss7, struct isup_call *c, unsigned char indicator);

int isup_res(struct ss7 *ss7, struct isup_call *c, unsigned char indicator);

int isup_cpg(struct ss7 *ss7, struct isup_call *c, int event);

int isup_lpa(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_gra(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[]);

int isup_grs(struct ss7 *ss7, struct isup_call *c, int endcic);

int isup_cgb(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[], int type);

int isup_cgu(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[], int type);

int isup_cgba(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[]);

int isup_cgua(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[]);

int isup_blo(struct ss7 *ss7, struct isup_call *c);

int isup_ubl(struct ss7 *ss7, struct isup_call *c);

/* int isup_ccr(struct ss7 *ss7, int cic, unsigned int dpc);	FIXME Not Implemented ! */

int isup_bla(struct ss7 *ss7, struct isup_call *c);

int isup_ucic(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_uba(struct ss7 *ss7, struct isup_call *c);

int isup_rsc(struct ss7 *ss7, struct isup_call *c);

int isup_cvr(struct ss7 *ss7, int cic, unsigned int dpc);

int isup_cqr(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char status[]);

int isup_event_iam(struct ss7 *ss7, struct isup_call *c, int opc);

void isup_clear_callflags(struct ss7 *ss7, struct isup_call *c, unsigned long flags);

/* Various call related sets */
void isup_free_call(struct ss7 *ss7, struct isup_call *c);

void isup_set_call_dpc(struct isup_call *c, unsigned int dpc);

void isup_set_called(struct isup_call *c, const char *called, unsigned char called_nai, const struct ss7 *ss7);

void isup_set_calling(struct isup_call *c, const char *calling, unsigned char calling_nai, unsigned char presentation_ind, unsigned char screening_ind);

void isup_set_connected(struct isup_call *c, const char *connected, unsigned char connected_nai, unsigned char connected_presentation_ind, unsigned char connected_screening_ind);

void isup_set_redirecting_number(struct isup_call *c, const char *redirecting_number, unsigned char redirecting_num_nai, unsigned char redirecting_num_presentation_ind, unsigned char redirecting_num_screening_ind);

void isup_set_redirection_info(struct isup_call *c, unsigned char redirect_info_ind, unsigned char redirect_info_orig_reas, unsigned char redirect_info_counter, unsigned char redirect_info_reas);

void isup_set_redirect_counter(struct isup_call *c, unsigned char redirect_counter);

void isup_set_orig_called_num(struct isup_call *c, const char *orig_called_num, unsigned char orig_called_nai, unsigned char orig_called_pres_ind, unsigned char orig_called_screening_ind);

void isup_set_tmr(struct isup_call *c, int tmr);

void isup_set_charge(struct isup_call *c, const char *charge, unsigned char charge_nai, unsigned char charge_num_plan);

void isup_set_oli(struct isup_call *c, int oli_ani2);

void isup_set_gen_address(struct isup_call *c, const char *gen_number, unsigned char gen_add_nai, unsigned char gen_pres_ind, unsigned char gen_num_plan, unsigned char gen_add_type);

void isup_set_gen_digits(struct isup_call *c, const char *gen_number, unsigned char gen_dig_type, unsigned char gen_dig_scheme);

void isup_set_col_req(struct isup_call *c);

void isup_set_cug(struct isup_call *c, unsigned char cug_indicator, const char *cug_interlock_ni, unsigned short cug_interlock_code);

void isup_set_interworking_indicator(struct isup_call *c, unsigned char interworking_indicator);

void isup_set_forward_indicator_pmbits(struct isup_call *c, unsigned char pmbits);

void isup_set_echocontrol(struct isup_call *c, unsigned char ec);

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

void isup_set_calling_party_category(struct isup_call *c, unsigned int category);

/* End of call related sets */

typedef void (*ss7_printf_cb)(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

void isup_show_calls(struct ss7 *ss7, ss7_printf_cb cust_printf, int fd);

void ss7_show_linkset(struct ss7 *ss7, ss7_printf_cb cust_printf, int fd);

/* net mng */
const char * mtp3_net_mng(struct ss7 *ss7, unsigned int slc, const char *cmd, unsigned int param);

void mtp3_init_restart(struct ss7 *ss7, int slc);

int ss7_set_mtp3_timer(struct ss7 *ss7, char *name, int ms);

void ss7_pc_to_str(int ss7type, unsigned int pc, char *str);

#endif /* _LIBSS7_H */
