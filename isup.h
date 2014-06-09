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

#ifndef _SS7_ISUP_H
#define _SS7_ISUP_H

#include "ss7_internal.h"

/* ISUP messages */
#define ISUP_IAM	0x01	/*!< Initial address */
#define ISUP_SAM	0x02	/*!< Subsequent address */
#define ISUP_INR	0x03	/*!< Information request (national use) */
#define ISUP_INF	0x04	/*!< Information (national use) */
#define ISUP_COT	0x05	/*!< Continuity */
#define ISUP_ACM	0x06	/*!< Address complete */
#define ISUP_CON	0x07	/*!< Connect */
#define ISUP_FOT	0x08	/*!< Forward transfer */
#define ISUP_ANM	0x09	/*!< Answer */
#define ISUP_REL	0x0c	/*!< Release */
#define ISUP_SUS	0x0d	/*!< Suspend */
#define ISUP_RES	0x0e	/*!< Resume */
#define ISUP_RLC	0x10	/*!< Release complete */
#define ISUP_CCR	0x11	/*!< Continuity check request */
#define ISUP_RSC	0x12	/*!< Reset circuit */
#define ISUP_BLO	0x13	/*!< Blocking */
#define ISUP_UBL	0x14	/*!< Unblocking */
#define ISUP_BLA	0x15	/*!< Blocking acknowledgement */
#define ISUP_UBA	0x16	/*!< Unblocking acknowledgement */
#define ISUP_GRS	0x17	/*!< Circuit group reset */
#define ISUP_CGB	0x18	/*!< Circuit group blocking */
#define ISUP_CGU	0x19	/*!< Circuit group unblocking */
#define ISUP_CGBA	0x1a	/*!< Circuit group blocking acknowledgement */
#define ISUP_CGUA	0x1b	/*!< Circuit group unblocking acknowledgement */
#define ISUP_CMR	0x1c	/*!< Reserved (used in 1988 version) */
#define ISUP_CMC	0x1d	/*!< Reserved (used in 1988 version) */
#define ISUP_CMRJ	0x1e	/*!< Reserved (used in 1988 version) */
#define ISUP_FAR	0x1f	/*!< Facility request */
#define ISUP_FAA	0x20	/*!< Facility accepted */
#define ISUP_FRJ	0x21	/*!< Facility reject */
#define ISUP_FAD	0x22	/*!< Reserved (used in 1984 version) */
#define ISUP_FAI	0x23	/*!< Reserved (used in 1984 version) */
#define ISUP_LPA	0x24	/*!< Loop back acknowledgement (national use) */
#define ISUP_CSVR	0x25	/*!< Reserved (used in 1984 version) */
#define ISUP_CSVS	0x26	/*!< Reserved (used in 1984 version) */
#define ISUP_DRS	0x27	/*!< Reserved (used in 1988 version) */
#define ISUP_PAM	0x28	/*!< Pass-along (national use) */
#define ISUP_GRA	0x29	/*!< Circuit group reset acknowledgement */
#define ISUP_CQM	0x2a	/*!< Circuit group query (national use) */
#define ISUP_CQR	0x2b	/*!< Circuit group query response (national use) */
#define ISUP_CPG	0x2c	/*!< Call progress */
#define ISUP_USR	0x2d	/*!< User-to-user information */
#define ISUP_UCIC	0x2e	/*!< Unequipped CIC (national use) */
#define ISUP_CFN	0x2f	/*!< Confusion */
#define ISUP_OLM	0x30	/*!< Overload (national use) */
#define ISUP_CRG	0x31	/*!< Charge information (national use) */
#define ISUP_FAC	0x33	/*!< Facility */
#define ISUP_CRA	0xe9	/*!< ??? */
#define ISUP_CRM	0xea	/*!< ??? */
#define ISUP_CVR	0xeb	/*!< ???Used??? */
#define ISUP_CVT	0xec	/*!< ???Used??? */
#define ISUP_EXM	0xed	/*!< ??? */


/* ISUP Parameters ITU-T Q.763 */
#define ISUP_PARM_CALL_REF						0x01
#define ISUP_PARM_TRANSMISSION_MEDIUM_REQS		0x02
#define ISUP_PARM_ACCESS_TRANS					0x03
#define ISUP_PARM_CALLED_PARTY_NUM				0x04
#define ISUP_PARM_SUBSEQUENT_NUMBER				0x05
#define ISUP_PARM_NATURE_OF_CONNECTION_IND		0x06
#define ISUP_PARM_FORWARD_CALL_IND				0x07
#define ISUP_PARM_OPT_FORWARD_CALL_INDICATOR	0x08
#define ISUP_PARM_CALLING_PARTY_CAT				0x09
#define ISUP_PARM_CALLING_PARTY_NUM				0x0a
#define ISUP_PARM_REDIRECTING_NUMBER			0x0b
#define ISUP_PARM_REDIRECTION_NUMBER			0x0c
#define ISUP_PARM_CONNECTION_REQ				0x0d
#define ISUP_PARM_INR_IND						0x0e
#define ISUP_PARM_INF_IND						0x0f
#define ISUP_PARM_CONTINUITY_IND				0x10
#define ISUP_PARM_BACKWARD_CALL_IND				0x11
#define ISUP_PARM_CAUSE							0x12
#define ISUP_PARM_REDIRECTION_INFO				0x13
/* 0x14 is Reserved / Event information */
#define ISUP_PARM_CIRCUIT_GROUP_SUPERVISION_IND	0x15
#define ISUP_PARM_RANGE_AND_STATUS				0x16
#define ISUP_PARM_CALL_MODIFICATION_IND			0x17
#define ISUP_PARM_FACILITY_IND					0x18
/* 0x19 is Reserved */
#define ISUP_PARM_CUG_INTERLOCK_CODE			0x1a
/* 0x1b is Reserved */
/* 0x1c is Reserved */
#define ISUP_PARM_USER_SERVICE_INFO				0x1d
#define ISUP_PARM_SIGNALLING_PC					0x1e
/* 0x1f is Reserved */
#define ISUP_PARM_USER_TO_USER_INFO				0x20
#define ISUP_CONNECTED_NUMBER					0x21
#define ISUP_PARM_SUSPEND_RESUME_IND			0x22
#define ISUP_PARM_TRANSIT_NETWORK_SELECTION		0x23
#define ISUP_PARM_EVENT_INFO					0x24
#define ISUP_PARM_CIRCUIT_ASSIGNMENT_MAP		0x25
#define ISUP_PARM_CIRCUIT_STATE_IND				0x26
#define ISUP_PARAM_AUTOMATIC_CONGESTION_LEVEL	0x27
#define ISUP_PARM_ORIGINAL_CALLED_NUM			0x28
#define ISUP_PARM_OPT_BACKWARD_CALL_IND			0x29
#define ISUP_PARM_USER_TO_USER_IND				0x2a
#define ISUP_PARM_ORIGINATION_ISC_PC			0x2b
#define ISUP_PARM_GENERIC_NOTIFICATION_IND		0x2c
#define ISUP_PARM_CALL_HISTORY_INFO				0x2d
#define ISUP_PARM_ACCESS_DELIVERY_INFO			0x2e
#define ISUP_PARM_NETWORK_SPECIFIC_FACILITY		0x2f
#define ISUP_PARM_USER_SERVICE_INFO_PRIME		0x30
#define ISUP_PARM_PROPAGATION_DELAY				0x31
#define ISUP_PARM_REMOTE_OPERATIONS				0x32
#define ISUP_PARM_SERVICE_ACTIVATION			0x33
#define ISUP_PARM_USER_TELESERVICE_INFO			0x34
#define ISUP_PARM_TRANSMISSION_MEDIUM_USED		0x35
#define ISUP_PARM_CALL_DIVERSION_INFO			0x36
#define ISUP_PARM_ECHO_CONTROL_INFO				0x37
#define ISUP_PARM_MESSAGE_COMPAT_INFO			0x38
#define ISUP_PARM_PARAMETER_COMPAT_INFO			0x39
#define ISUP_PARM_MLPP_PRECEDENCE				0x3a
#define ISUP_PARM_MCID_REQUEST_IND				0x3b
#define ISUP_PARM_MCID_RESPONSE_IND				0x3c
#define ISUP_PARM_HOP_COUNTER					0x3d
#define ISUP_PARM_TRANSMISSION_MEDIUM_REQ_PRIME	0x3e
#define ISUP_PARM_LOCATION_NUMBER				0x3f

#define ISUP_PARM_REDIRECTION_NUM_RESTRICTION	0x40

#define ISUP_PARM_CALL_TRANSFER_REFERENCE		0x43
#define ISUP_PARM_LOOP_PREVENTION_IND			0x44
#define ISUP_PARM_CALL_TRANSFER_NUMBER			0x45

#define ISUP_PARM_CCSS							0x4b
#define ISUP_PARM_FORWARD_GVNS					0x4c
#define ISUP_PARM_BACKWARD_GVNS					0x4d
#define ISUP_PARM_REDIRECT_CAPABILITY			0x4e

#define ISUP_PARM_NETWORK_MANAGEMENT_CONTROL	0x5b

#define ISUP_PARM_CORRELATION_ID				0x65
#define ISUP_PARM_SCF_ID						0x66

#define ISUP_PARM_CALL_DIVERSION_TREATMENT_IND	0x6e
#define ISUP_PARM_CALLED_IN_NUMBER				0x6f
#define ISUP_PARM_CALL_OFFERING_TREATMENT_IND	0x70
#define ISUP_PARM_CHARGED_PARTY_IDENT			0x71
#define ISUP_PARM_CONFERENCE_TREATMENT_IND		0x72
#define ISUP_PARM_DISPLAY_INFO					0x73
#define ISUP_PARM_UID_ACTION_IND				0x74
#define ISUP_PARM_UID_CAPABILITY_IND			0x75

#define ISUP_PARM_REDIRECT_COUNTER				0x77
#define ISUP_PARM_APPLICATION_TRANSPORT			0x78
#define ISUP_PARM_COLLECT_CALL_REQUEST			0x79
#define ISUP_PARM_CCNR_POSSIBLE_IND				0x7a
#define ISUP_PARM_PIVOT_CAPABILITY				0x7b
#define ISUP_PARM_PIVOT_ROUTING_IND				0x7c
#define ISUP_PARM_CALLED_DIRECTORY_NUMBER		0x7d

#define ISUP_PARM_ORIGINAL_CALLED_IN_NUM		0x7f
/* 0x80 reserved for future extension */
#define ISUP_PARM_CALLING_GEODETIC_LOCATION		0x81
#define ISUP_PARM_HTR_INFO						0x82

#define ISUP_PARM_NETWORK_ROUTING_NUMBER		0x84
#define ISUP_PARM_QUERY_ON_RELEASE_CAPABILITY	0x85
#define ISUP_PARM_PIVOT_STATUS					0x86
#define ISUP_PARM_PIVOT_COUNTER					0x87
#define ISUP_PARM_PIVOT_ROUTING_FORWARD_IND		0x88
#define ISUP_PARM_PIVOT_ROUTING_BACKWARD_IND	0x89
#define ISUP_PARM_REDIRECT_STATUS				0x8a
#define ISUP_PARM_REDIRECT_FORWARD_INFO			0x8b
#define ISUP_PARM_REDIRECT_BACKWARD_INFO		0x8c
#define ISUP_PARM_NUM_PORTABILITY_FORWARD_INFO	0x8d

#define ISUP_PARM_GENERIC_ADDR					0xc0
#define ISUP_PARM_GENERIC_DIGITS				0xc1

#define ISUP_PARM_EGRESS_SERV					0xc3
#define ISUP_PARM_JIP							0xc4
#define ISUP_PARM_CARRIER_ID					0xc5
#define ISUP_PARM_BUSINESS_GRP					0xc6
#define ISUP_PARM_GENERIC_NAME					0xc7

#define ISUP_PARM_LOCAL_SERVICE_PROVIDER_IDENTIFICATION	0xe4

#define ISUP_PARM_ORIG_LINE_INFO				0xea
#define ISUP_PARM_CHARGE_NUMBER					0xeb

#define ISUP_PARM_SELECTION_INFO				0xee


/* ISUP TIMERS  */
#define ISUP_TIMER_T1	1
#define ISUP_TIMER_T2	2
#define ISUP_TIMER_T5	5
#define ISUP_TIMER_T6	6
#define ISUP_TIMER_T7	7
#define ISUP_TIMER_T8	8
#define ISUP_TIMER_T10	10
#define ISUP_TIMER_T12	12
#define ISUP_TIMER_T13	13
#define ISUP_TIMER_T14	14
#define ISUP_TIMER_T15	15
#define ISUP_TIMER_T16	16
#define ISUP_TIMER_T17	17
#define ISUP_TIMER_T18	18
#define ISUP_TIMER_T19	19
#define ISUP_TIMER_T20	20
#define ISUP_TIMER_T21	21
#define ISUP_TIMER_T22	22
#define ISUP_TIMER_T23	23
#define ISUP_TIMER_T27	27
#define ISUP_TIMER_T33	33
#define ISUP_TIMER_T35	35

/* ISUP Parameter Pseudo-type */
struct isup_parm_opt {
	unsigned char type;
	unsigned char len;
	unsigned char data[0];
};

struct isup_h {
	unsigned char cic[2];
	unsigned char type;
	unsigned char data[0]; /* This is the contents of the message */
};

#define CIC_SIZE 2
#define ISUP_MAX_NUM 64
/* From GR-317 for the generic name filed: 15 + 1 */
#define ISUP_MAX_NAME 16

struct mtp2;

struct isup_call {
	char called_party_num[ISUP_MAX_NUM];
	unsigned char called_nai;
	char calling_party_num[ISUP_MAX_NUM];
	unsigned char calling_party_cat;
	unsigned char calling_nai;
	unsigned char presentation_ind;
	unsigned char screening_ind;
	char charge_number[ISUP_MAX_NUM];
	unsigned char charge_nai;
	unsigned char charge_num_plan;
	unsigned char gen_add_num_plan;
	unsigned char gen_add_nai;
	char gen_add_number[ISUP_MAX_NUM];
	unsigned char gen_add_pres_ind;
	unsigned char gen_add_type;
	char gen_dig_number[ISUP_MAX_NUM];
	unsigned char gen_dig_type;
	unsigned char gen_dig_scheme;
	char jip_number[ISUP_MAX_NUM];
	unsigned char lspi_type;
	unsigned char lspi_scheme;
	unsigned char lspi_context;
	unsigned char lspi_spare;
	char lspi_ident[ISUP_MAX_NUM];
	int oli_ani2;
	unsigned int call_ref_ident;
	unsigned int call_ref_pc;
	char orig_called_num[ISUP_MAX_NUM];
	unsigned char orig_called_nai;
	unsigned char orig_called_pres_ind;
	unsigned char orig_called_screening_ind;
	char redirecting_num[ISUP_MAX_NUM];
	unsigned char redirecting_num_nai;
	unsigned char redirecting_num_presentation_ind;
	unsigned char redirecting_num_screening_ind;
	unsigned char redirect_counter;
	unsigned char redirect_info;
	unsigned char redirect_info_ind;
	unsigned char redirect_info_orig_reas;
	unsigned char redirect_info_counter;
	unsigned char redirect_info_reas;
	unsigned char generic_name_typeofname;
	unsigned char generic_name_avail;
	unsigned char generic_name_presentation;
	char connected_num[ISUP_MAX_NUM];
	unsigned char connected_nai;
	unsigned char connected_presentation_ind;
	unsigned char connected_screening_ind;
	char generic_name[ISUP_MAX_NAME];
	int range;
	unsigned char sent_cgb_status[255];
	unsigned char sent_cgu_status[255];
	unsigned char status[255];
	int transcap;
	int l1prot;
	int cause;
	int causecode;
	int causeloc;
	int cot_check_passed;
	int cot_check_required;
	int cot_performed_on_previous_cic;
	int cicgroupsupervisiontype;
	unsigned char event_info;
	unsigned short cic;
	unsigned char sls;
	unsigned long got_sent_msg;	/* flags for sent msgs */
	int sent_cgb_type;
	int sent_cgu_type;
	int sent_grs_endcic;
	int sent_cgb_endcic;
	int sent_cgu_endcic;
	struct isup_call *next;
	/* set DPC according to CIC's DPC, not linkset */
	unsigned int dpc;
	/* Backward Call Indicator variables */
	unsigned char called_party_status_ind;
	unsigned char local_echocontrol_ind;
	unsigned char echocontrol_ind;
	/* Suspend/Resume Indicator */
	int network_isdn_indicator;
	unsigned char inr_ind[2];
	unsigned char inf_ind[2];
	unsigned char cug_indicator;
	unsigned col_req;
	char cug_interlock_ni[5];
	unsigned short cug_interlock_code;
	unsigned char interworking_indicator;
	unsigned char forward_indicator_pmbits;
	int timer[ISUP_MAX_TIMERS];
};

int isup_receive(struct ss7 *ss7, struct mtp2 *sl, struct routing_label *rl, unsigned char *sif, int len);

int isup_dump(struct ss7 *ss7, struct mtp2 *sl, unsigned char *sif, int len);

void isup_free_all_calls(struct ss7 *ss7);

#endif /* _SS7_ISUP_H */
