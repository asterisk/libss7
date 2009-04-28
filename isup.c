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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "libss7.h"
#include "isup.h"
#include "ss7_internal.h"
#include "mtp3.h"
#include "mtp2.h"

#define FUNC_DUMP(name) int ((name))(struct ss7 *ss7, int messagetype, unsigned char *parm, int len)
/* Length here is paramter length */
#define FUNC_RECV(name) int ((name))(struct ss7 *ss7, struct isup_call *c, int messagetype, unsigned char *parm, int len)
/* Length here is maximum length */
#define FUNC_SEND(name) int ((name))(struct ss7 *ss7, struct isup_call *c, int messagetype, unsigned char *parm, int len)

#define PARM_TYPE_FIXED 0x01
#define PARM_TYPE_VARIABLE 0x02
#define PARM_TYPE_OPTIONAL 0x03

#define CODE_CCITT 0x0

#define LOC_PRIV_NET_LOCAL_USER 0x1

struct parm_func {
	int parm;
	char *name;
	FUNC_DUMP(*dump);
	FUNC_RECV(*receive);
	FUNC_SEND(*transmit);
};

struct isup_timer_param {
	struct ss7 *ss7;
	struct isup_call *c;
	int timer;
};

static int iam_params[] = {ISUP_PARM_NATURE_OF_CONNECTION_IND, ISUP_PARM_FORWARD_CALL_IND, ISUP_PARM_CALLING_PARTY_CAT,
	ISUP_PARM_TRANSMISSION_MEDIUM_REQS, ISUP_PARM_CALLED_PARTY_NUM, ISUP_PARM_CALLING_PARTY_NUM, ISUP_PARM_REDIRECTING_NUMBER,
	ISUP_PARM_REDIRECTION_INFO, ISUP_PARM_REDIRECT_COUNTER, ISUP_PARM_ORIGINAL_CALLED_NUM, ISUP_PARM_OPT_FORWARD_CALL_INDICATOR,
	ISUP_PARM_CUG_INTERLOCK_CODE, -1};

static int ansi_iam_params[] = {ISUP_PARM_NATURE_OF_CONNECTION_IND, ISUP_PARM_FORWARD_CALL_IND, ISUP_PARM_CALLING_PARTY_CAT,
	ISUP_PARM_USER_SERVICE_INFO, ISUP_PARM_CALLED_PARTY_NUM, ISUP_PARM_CALLING_PARTY_NUM, ISUP_PARM_CHARGE_NUMBER, 
	ISUP_PARM_ORIG_LINE_INFO, ISUP_PARM_GENERIC_ADDR, ISUP_PARM_GENERIC_DIGITS, ISUP_PARM_GENERIC_NAME, ISUP_PARM_JIP, 
	ISUP_PARM_LOCAL_SERVICE_PROVIDER_IDENTIFICATION, ISUP_PARM_REDIRECTION_INFO, ISUP_PARM_REDIRECTING_NUMBER, ISUP_PARM_REDIRECT_COUNTER,
	ISUP_PARM_ORIGINAL_CALLED_NUM, ISUP_PARM_OPT_FORWARD_CALL_INDICATOR, ISUP_PARM_CUG_INTERLOCK_CODE, -1};


static int acm_params[] = {ISUP_PARM_BACKWARD_CALL_IND, -1};

static int faa_params[] = {ISUP_PARM_FACILITY_IND, ISUP_PARM_CALL_REF, -1};

static int far_params[] = {ISUP_PARM_FACILITY_IND, ISUP_PARM_CALL_REF, -1};

static int anm_params[] = { ISUP_CONNECTED_NUMBER, -1};

static int con_params[] = { ISUP_PARM_BACKWARD_CALL_IND, ISUP_CONNECTED_NUMBER, -1};

static int rel_params[] = { ISUP_PARM_CAUSE, -1};

static int greset_params[] = { ISUP_PARM_RANGE_AND_STATUS, -1};

static int cot_params[] = { ISUP_PARM_CONTINUITY_IND, -1};

static int cpg_params[] = { ISUP_PARM_EVENT_INFO, -1};

static int cicgroup_params[] = { ISUP_PARM_CIRCUIT_GROUP_SUPERVISION_IND, ISUP_PARM_RANGE_AND_STATUS, -1};

static int cqr_params[] = { ISUP_PARM_RANGE_AND_STATUS, ISUP_PARM_CIRCUIT_STATE_IND, -1};

static int susres_params[] = { ISUP_PARM_SUSRES_IND, ISUP_PARM_CALL_REF, -1};

static int inr_params[] = { ISUP_PARM_INR_IND, -1};

static int inf_params[] = { ISUP_PARM_INF_IND, ISUP_PARM_CALLING_PARTY_NUM, ISUP_PARM_CALLING_PARTY_CAT, -1};

static int sam_params[] = { ISUP_PARM_SUBSEQUENT_NUMBER, -1};

static int empty_params[] = { -1};

static struct message_data {
	int messagetype;
	int mand_fixed_params;
	int mand_var_params;
	int opt_params;
	int ansi_priority;
	int *param_list;
} messages[] = {
	{ISUP_IAM, 4, 1, 1, 0, iam_params},
	{ISUP_ACM, 1, 0, 1, 1, acm_params},
	{ISUP_ANM, 0, 0, 1, 2, anm_params},
	{ISUP_CON, 1, 0, 1, -1, con_params},
	{ISUP_REL, 0, 1, 1, 1, rel_params},
	{ISUP_RLC, 0, 0, 1, 2, empty_params},
	{ISUP_GRS, 0, 1, 0, 0, greset_params},
	{ISUP_GRA, 0, 1, 0, 0, greset_params},
	{ISUP_CGB, 1, 1, 0, 0, cicgroup_params},
	{ISUP_CGU, 1, 1, 0, 0, cicgroup_params},
	{ISUP_CGBA, 1, 1, 0, 0, cicgroup_params},
	{ISUP_CGUA, 1, 1, 0, 0, cicgroup_params},
	{ISUP_COT, 1, 0, 0, 1, cot_params},
	{ISUP_CCR, 0, 0, 0, 1, empty_params},
	{ISUP_BLO, 0, 0, 0, 0, empty_params},
	{ISUP_LPA, 0, 0, 0, 1, empty_params},
	{ISUP_UBL, 0, 0, 0, 0, empty_params},
	{ISUP_BLA, 0, 0, 0, 0, empty_params},
	{ISUP_UBA, 0, 0, 0, 0, empty_params},
	{ISUP_RSC, 0, 0, 0, 0, empty_params},
	{ISUP_CVR, 0, 0, 0, 0, empty_params},
	{ISUP_CVT, 0, 0, 0, 0, empty_params},
	{ISUP_CPG, 1, 0, 1, 1, cpg_params},
	{ISUP_UCIC, 0, 0, 0, 1, empty_params},
	{ISUP_CQM, 0, 1, 0, 0, greset_params},
	{ISUP_CQR, 0, 2, 0, 0, cqr_params},
	{ISUP_FAA, 1, 0, 1, -1, faa_params},
	{ISUP_FAR, 1, 0, 1, -1, far_params},
	{ISUP_CFN, 0, 1, 0, 0, rel_params},
	{ISUP_SUS, 1, 0, 1, 1, susres_params},
	{ISUP_RES, 1, 0, 1, 1, susres_params},
	{ISUP_INR, 1, 0, 0, 1, inr_params},
	{ISUP_INF, 1, 0, 2, 1, inf_params},
	{ISUP_SAM, 0, 1, 0, -1, sam_params}
};

static int isup_send_message(struct ss7 *ss7, struct isup_call *c, int messagetype, int parms[]);

static int isup_start_timer(struct ss7 *ss7, struct isup_call *c, int timer);
static void isup_stop_all_timers(struct ss7 *ss7, struct isup_call *c);
static int isup_timer2str(int timer, char *res);
static void isup_stop_timer(struct ss7 *ss7, struct isup_call *c, int timer);

static char * message2str(unsigned char message)
{
	switch (message) {
		case ISUP_IAM:
			return "IAM";
		case ISUP_ACM:
			return "ACM";
		case ISUP_ANM:
			return "ANM";
		case ISUP_CON:
			return "CON";
		case ISUP_REL:
			return "REL";
		case ISUP_RLC:
			return "RLC";
		case ISUP_GRS:
			return "GRS";
		case ISUP_GRA:
			return "GRA";
		case ISUP_COT:
			return "COT";
		case ISUP_CCR:
			return "CCR";
		case ISUP_BLO:
			return "BLO";
		case ISUP_UBL:
			return "UBL";
		case ISUP_BLA:
			return "BLA";
		case ISUP_UBA:
			return "UBA";
		case ISUP_CGB:
			return "CGB";
		case ISUP_CGU:
			return "CGU";
		case ISUP_CGBA:
			return "CGBA";
		case ISUP_CGUA:
			return "CGUA";
		case ISUP_RSC:
			return "RSC";
		case ISUP_CPG:
			return "CPG";
		case ISUP_UCIC:
			return "UCIC";
		case ISUP_LPA:
			return "LPA";
		case ISUP_FAA:
			return "FAA";
		case ISUP_FAR:
			return "FAR";
		case ISUP_FRJ:
			return "FRJ";
		case ISUP_CVT:
			return "CVT";
		case ISUP_CVR:
			return "CVR";
		case ISUP_CFN:
			return "CFN";
		case ISUP_SUS:
			return "SUS";
		case ISUP_RES:
			return "RES";
		case ISUP_INR:
			return "INR";
		case ISUP_INF:
			return "INF";
		case ISUP_SAM:
			return "SAM";
		default:
			return "Unknown";
	}
}

static char char2digit(char localchar)
{
	switch (localchar) {
		case '0':
			return 0;
		case '1':
			return 1;
		case '2':
			return 2;
		case '3':
			return 3;
		case '4':
			return 4;
		case '5':
			return 5;
		case '6':
			return 6;
		case '7':
			return 7;
		case '8':
			return 8;
		case '9':
			return 9;
		case '#':
			return 0xf;
		default:
			return 0;
	}
}

static char digit2char(unsigned char digit)
{
	switch (digit & 0xf) {
		case 0:
			return '0';
		case 1:
			return '1';
		case 2:
			return '2';
		case 3:
			return '3';
		case 4:
			return '4';
		case 5:
			return '5';
		case 6:
			return '6';
		case 7:
			return '7';
		case 8:
			return '8';
		case 9:
			return '9';
		case 15:
			return '#';
		default:
			return 0;
	}
}

static void isup_dump_buffer(struct ss7 *ss7, unsigned char *data, int len)
{
	int i;

	ss7_message(ss7, "[");
	for (i = 0; i < len; i++) {
		ss7_message(ss7, "0x%x ", data[i]);
	}
	ss7_message(ss7, "]\n");
}

static void isup_get_number(char *dest, unsigned char *src, int srclen, int oddeven)
{
	int i;

	if (oddeven < 2) {
		/* BCD odd or even */
		for (i = 0; i < ((srclen * 2) - oddeven); i++)
			dest[i] = digit2char(src[i/2] >> ((i % 2) ? 4 : 0));
	} else {
		/* oddeven = 2 for IA5 characters */
		for (i = 0; i < srclen; i++)
			dest[i] = src[i];
	}
	dest[i] = '\0'; 
}


static void isup_put_generic(unsigned char *dest, char *src, int *len)
{
	int i = 0;
	int numlen = strlen(src);

	*len = numlen;
	
	while (i < numlen) {
		dest[i] = (src[i]);
		i++;
	}
}

static void isup_put_number(unsigned char *dest, char *src, int *len, int *oddeven)
{
	int i = 0;
	int numlen = strlen(src);

	if (numlen % 2) {
		*oddeven = 1;
		*len = numlen/2 + 1;
	} else {
		*oddeven = 0;
		*len = numlen/2;
	}

	while (i < numlen) {
		if (!(i % 2))
			dest[i/2] |= char2digit(src[i]) & 0xf;
		else
			dest[i/2] |= (char2digit(src[i]) << 4) & 0xf0;
		i++;
	}
}

static FUNC_SEND(nature_of_connection_ind_transmit)
{
	parm[0] = 0x00;

	if (c->cot_check_required)
		parm[0] |= 0x04;

	if (c->local_echocontrol_ind)
		parm[0] |= 0x10;

	return 1; /* Length plus size of type header */
}

static FUNC_RECV(nature_of_connection_ind_receive)
{
	unsigned char cci = (parm[0] >> 2) & 0x3;

	if (cci == 0x1)
		c->cot_check_required = 1;
	else
		c->cot_check_required = 0;

	if (cci == 0x2)
		c->cot_performed_on_previous_cic = 1;
	else
		c->cot_performed_on_previous_cic = 0;

	if (parm[0] & 0x10)
		c->echocontrol_ind = 1;
	else
		c->echocontrol_ind = 0;
	
	return 1;
}

static FUNC_DUMP(nature_of_connection_ind_dump)
{
	unsigned char con = parm[0];
	char *continuity;

	ss7_message(ss7, "\t\t\tSatellites in connection: %d\n", con&0x03);
	con>>=2; 
	switch (con & 0x03) {
		case 0:
			continuity = "Check not required";
			break;
		case 1:
			continuity = "Check required on this circuit";
			break;
		case 2:
			continuity = "Check performed on a previous circuit";
			break;
		case 3:
			continuity = "spare";
			break;
	}
	ss7_message(ss7, "\t\t\tContinuity Check: %s (%d)\n", continuity, con & 0x3);
	con>>=2;
	con &= 0x01;

	ss7_message(ss7, "\t\t\tOutgoing half echo control device: %s (%d)\n", con ? "included" : "not included", con);

	return 1;
}


static FUNC_SEND(forward_call_ind_transmit)
{
	parm[0] = 0x60;
	parm[1] = 0x0;

	if (ss7->flags & SS7_ISDN_ACCESS_INDICATOR)
		parm [1] |= 0x01;

	return 2;
}

static FUNC_RECV(forward_call_ind_receive)
{
	return 2;
}

static FUNC_DUMP(forward_call_ind_dump)
{
	char *cb_str, *hg_str, *kj_str;
	switch ((parm[0] >> 1) & 3) {
		case 0:
			cb_str = "no end-to-end";
			break;
		case 1:
			cb_str = "pass-along";
			break;
		case 2:
			cb_str = "SCCP";
			break;
		case 3:
			cb_str = "pass-along and SCCP";
			break;
	}

	switch ((parm[0] >> 6) & 3) {
		case 0:
			hg_str = "ISDN user part preferred all the way";
			break;
		case 1:
			hg_str = "ISDN user part not preferred all the way";
			break;
		case 2:
			hg_str = "ISDN user part requried all the way";
			break;
		case 3:
			hg_str = "spare";
			break;
	}

	switch ((parm[1] >> 1) & 3) {
		case 0:
			kj_str = "no indication";
			break;
		case 1:
			kj_str = "connectionless method available";
			break;
		case 2:
			kj_str = "connection oriented method available";
			break;
		case 3:
			kj_str = "connectionless and connection oriented method available";
			break;
	}

	ss7_message(ss7, "\t\t\tNat/Intl Call Ind: call to be treated as a %s call (%d)\n", (parm[0] & 1) ? "international" : "national", parm[0] & 1);
	ss7_message(ss7, "\t\t\tEnd to End Method Ind: %s method(s) available (%d)\n", cb_str, (parm[0] >> 1) & 3);
	ss7_message(ss7, "\t\t\tInterworking Ind: %sinterworking encountered (%d)\n", ((parm[0] >> 3) & 1) ? "" : "no ", (parm[0] >> 3) & 1);
	ss7_message(ss7, "\t\t\tEnd to End Info Ind: %send-to-end information available (%d)\n", ((parm[0]>>4)&1) ? "" : "no ", (parm[0] >> 4) & 1);
	ss7_message(ss7, "\t\t\tISDN User Part Ind: ISDN user part %sused all the way (%d)\n", ((parm[0]>>5)&1) ? "" : "not ", (parm[0] >> 5) & 1);
	ss7_message(ss7, "\t\t\tISDN User Part Pref Ind: %s (%d)\n", hg_str, (parm[0] >> 6) & 3);
	ss7_message(ss7, "\t\t\tISDN Access Ind: originating access %s (%d)\n", (parm[1] & 1) ? "ISDN" : "non-ISDN", parm[1] & 1);
	ss7_message(ss7, "\t\t\tSCCP Method Ind: %s (%d)\n", kj_str, (parm[1] >> 1) & 3);
	return 2;
}

static FUNC_RECV(calling_party_cat_receive)
{
	c->calling_party_cat = parm[0];
	return 1;
}

static FUNC_SEND(calling_party_cat_transmit)
{
       parm[0] = c->calling_party_cat;
	return 1;
}

static FUNC_DUMP(calling_party_cat_dump)
{
	char *cattype;

	switch (parm[0]) {
		case 1:
			cattype = "Operator, French";
			break;
		case 2:
			cattype = "Operator, English";
			break;
		case 3:
			cattype = "Operator, German";
			break;
		case 4:
			cattype = "Operator, Russian";
			break;
		case 5:
			cattype = "Operator, Spanish";
			break;
		case 9:
			cattype = "Reserved";
			break;
		case 10:
			cattype = "Ordinary calling subscriber";
			break;
		case 11:
			cattype = "Calling subscriber with priority";
			break;
		case 12:
			cattype = "Data Call (voice band data)";
			break;
		case 13:
			cattype = "Test Call";
			break;
		case 15:
			cattype = "Payphone";
			break;
		default:
			cattype = "Unknown";
			break;
	}

	ss7_message(ss7, "\t\t\tCategory: %s (%d)\n", cattype, parm[0]);
	return 1;
}

static FUNC_RECV(user_service_info_receive)
{
	/* NoOp it for now */
	return len;
}

static FUNC_SEND(user_service_info_transmit)
{
	/* Default to Coding standard CCITT / 3.1 khz Audio */
	parm[0] = 0x90;
	/* Default to Circuit mode / 64kbps */
	parm[1] = 0x90;
	/* User Layer 1 set to ulaw */
	parm[2] = 0xa2;

	return 3;
}

static FUNC_SEND(transmission_medium_reqs_transmit)
{
	if (ss7->switchtype != SS7_ITU)
		return 0;
	/* Speech */
	parm[0] = 0;
	return 1;
}

static FUNC_RECV(transmission_medium_reqs_receive)
{
	c->transcap = parm[0] & 0x7f;
	return 1;
}

static FUNC_DUMP(transmission_medium_reqs_dump)
{
	char *type;

	switch (parm[0]) {
		case 0:
			type = "Speech";
			break;
		case 1:
			type = "Spare";
			break;
		case 2:
			type = "64 kbit/s unrestricted";
			break;
		case 4:
			type = "3.1 khz audio";
			break;
		case 6:
			type = "64 kbit/s preferred";
			break;
		case 7:
			type = "2 x 64 kbit/s unrestricted";
			break;
		case 8:
			type = "384 kbit/s unrestricted";
			break;
		case 9:
			type = "1536 kbit/s unrestricted";
			break;
		case 10:
			type = "1920 kbit/s unrestricted";
			break;
		default:
			type = "N x 64kbit/s unrestricted or possibly spare";
			break;
	}
	ss7_message(ss7, "\t\t\t%s (%d)\n", type, parm[0]);
	return 1;
}

static FUNC_DUMP(called_party_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";

	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNI: %x\n", (parm[1] >> 7) & 0x1);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);

	isup_get_number(numbuf, &parm[2], len - 2, oddeven);

	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);

	return len;
}

/* For variable length parameters we pass in the length of the parameter */
static FUNC_RECV(called_party_num_receive)
{
	int odd = 0;

	if (parm[0] & 0x80)
		odd = 1;

	isup_get_number(c->called_party_num, &parm[2], len - 2, odd);

	c->called_nai = parm[0] & 0x7f; /* Nature of Address Indicator */

	return len;
}

static FUNC_SEND(called_party_num_transmit)
{
	int numlen, oddeven;

	isup_put_number(&parm[2], c->called_party_num, &numlen, &oddeven);

	parm[0] = c->called_nai & 0x7f; /* Nature of Address Indicator */

	if (oddeven)
		parm[0] |= 0x80; /* Odd number of digits */

	parm[1] = 0x1 << 4; /* Assume E.164 ISDN numbering plan, called number complete  */

	return numlen + 2;
}

static FUNC_RECV(backward_call_ind_receive)
{
	c->called_party_status_ind = (parm[0] >> 2) & 0x3;
	c->echocontrol_ind = (parm[1] >> 5) & 0x1;
	return 2;
}

static FUNC_SEND(backward_call_ind_transmit)
{
	parm[0] = 0x40;
	parm[1] = 0x04;

	if (c->local_echocontrol_ind)
		parm[1] |= 0x20;

	if (ss7->flags | SS7_ISDN_ACCESS_INDICATOR)
		parm[1] |= 0x10;

	return 2;
}

static FUNC_DUMP(backward_call_ind_dump)
{
	unsigned char ba = parm[0] & 0x3;
	unsigned char dc = (parm[0] >> 2) & 0x3;
	unsigned char fe = (parm[0] >> 4) & 0x3;
	unsigned char hg = (parm[0] >> 6) & 0x3;
	unsigned char i = parm[1] & 0x1;
	unsigned char j = (parm[1] >> 1) & 0x1;
	unsigned char k = (parm[1] >> 2) & 0x1;
	unsigned char l = (parm[1] >> 3) & 0x1;
	unsigned char m = (parm[1] >> 4) & 0x1;
	unsigned char n = (parm[1] >> 5) & 0x1;
	unsigned char pq = (parm[1] >> 7) & 0x3;

	ss7_message(ss7, "\t\t\tCharge indicator: %d\n", ba);
	ss7_message(ss7, "\t\t\tCalled party's status indicator: %d\n", dc);
	ss7_message(ss7, "\t\t\tCalled party's category indicator: %d\n", fe);
	ss7_message(ss7, "\t\t\tEnd to End method indicator: %d\n", hg);
	ss7_message(ss7, "\t\t\tInterworking indicator: %d\n", i);
	ss7_message(ss7, "\t\t\tEnd to End information indicator: %d\n", j);
	ss7_message(ss7, "\t\t\tISDN user part indicator: %d\n", k);
	ss7_message(ss7, "\t\t\tHolding indicator: %d\n", l);
	ss7_message(ss7, "\t\t\tISDN access indicator: %d\n", m);
	ss7_message(ss7, "\t\t\tEcho control device indicator: %d\n", n);
	ss7_message(ss7, "\t\t\tSCCP method indicator: %d\n", pq);

	return 2;
}

static FUNC_RECV(opt_backward_call_ind_receive)
{
	return 1;
}

static FUNC_DUMP(opt_backward_call_ind_dump)
{
	unsigned char a, b, c, d;
	a = parm[0] & 1;
	b = (parm[0] >> 1) & 1;
	c = (parm[0] >> 2) & 1;
	d = (parm[0] >> 3) & 1;

	ss7_message(ss7, "\t\t\tIn-band information indicator: %d\n", a);
	ss7_message(ss7, "\t\t\tCall diversion may occur indicator: %d\n", b);
	ss7_message(ss7, "\t\t\tSimple segmentation indicator: %d\n", c);
	ss7_message(ss7, "\t\t\tMLPP user indicator: %d\n", d);
	return 1;
}

static FUNC_DUMP(opt_forward_call_ind_dump)
{
	char *desc;
	
	switch (parm[0] & 0x03) {
	case ISUP_CUG_NON:
		desc = "non-CUG call";
		break;
	case ISUP_CUG_OUTGOING_ALLOWED:
		desc = "closed user group call, outgoing access allowed";
		break;
	case ISUP_CUG_OUTGOING_NOT_ALLOWED:
		desc = "closed user group call, outgoing access not allowed";
		break;
	default:
		desc = "spare";
	}
	
	ss7_message(ss7, "\t\t\tClosed user group call indicator: %s\n", desc);
	ss7_message(ss7, "\t\t\tSimple segmentation indicator: %s\n", (parm[0] & (1 << 2)) ? 
			"additional information will be sent in segmentation message" :
			"no additional message will be sent");
	ss7_message(ss7, "\t\t\tConnected line identify request indicator %s\n", (parm[0] & (1 << 7)) ?
			"requested": "not requested");
	
	return 1;
}

static FUNC_SEND(opt_forward_call_ind_transmit)
{
	if (c->col_req || c->cug_indicator) {
		parm[0] = (c->cug_indicator & 0x03) | (c->col_req << 7);
		return 1;
	} else
		return 0;
}

static FUNC_RECV(opt_forward_call_ind_receive)
{
	c->cug_indicator = parm[0] & 0x03;
	c->col_req = parm[0] >> 7;
	return 1;
}

static FUNC_RECV(cause_receive)
{
	c->causeloc = parm[0] & 0xf;
	c->causecode = (parm[0] & 0x60) >> 5;
	c->cause = (parm[1] & 0x7f);

	return len;
}

static FUNC_SEND(cause_transmit)
{
	parm[0] = 0x80 | (c->causecode << 5) | c->causeloc;
	parm[1] = 0x80 | c->cause;
	return 2;
}

static FUNC_DUMP(cause_dump)
{
	char *cause;
	switch (parm[1] & 0x7f) {
		case 1:
			cause = "Unallocated (unassigned) number";
			break;
		case 2:
			cause = "No route to specified transit network";
			break;
		case 3:
			cause = "No route to destination";
			break;
		case 4:
			cause = "Send special information tone";
			break;
		case 5:
			cause = "Misdialled trunk prefix";
			break;
		case 6:
			cause = "Channel unacceptable";
			break;
		case 7:
			cause = "Call awarded and being delivered in an established channel";
			break;
		case 8:
			cause = "Preemption";
			break;
		case 9:
			cause = "Preemption - circuit reserved for reuse";
			break;
		case 16:
			cause = "Normal call clearing";
			break;
		case 17:
			cause = "User busy";
			break;
		case 18:
			cause = "No user responding";
			break;
		case 19:
			cause = "No answer from user (user alerted)";
			break;
		case 20:
			cause = "Subscriber absent";
			break;
		case 21:
			cause = "Call rejected";
			break;
		case 22:
			cause = "Number changed";
			break;
		case 23:
			cause = "Redirection to new destination";
			break;
		case 25:
			cause = "Exchange routing error";
			break;
		case 26:
			cause = "Non-selected user clearing";
			break;
		case 27:
			cause = "Destination out of order";
			break;
		case 28:
			cause = "Invalid number format (address incomplete)";
			break;
		case 29:
			cause = "Facility rejected";
			break;
		case 30:
			cause = "Response to STATUS ENQUIRY";
			break;
		case 31:
			cause = "Normal, unspecified";
			break;
		case 34:
			cause = "No circuit/channel available";
			break;
		case 38:
			cause = "Network out of order";
			break;
		case 39:
			cause = "Permanent frame mode connection out of service";
			break;
		case 40:
			cause = "Permanent frame mode connection operational";
			break;
		case 41:
			cause = "Temporary failure";
			break;
		case 42:
			cause = "Switching equipment congestion";
			break;
/* TODO: Finish the rest of these */
		default:
			cause = "Unknown";
	}
	ss7_message(ss7, "\t\t\tCoding Standard: %d\n", (parm[0] >> 5) & 3);
	ss7_message(ss7, "\t\t\tLocation: %d\n", parm[0] & 0xf);
	ss7_message(ss7, "\t\t\tCause Class: %d\n", (parm[1]>>4) & 0x7);
	ss7_message(ss7, "\t\t\tCause Subclass: %d\n", parm[1] & 0xf);
	ss7_message(ss7, "\t\t\tCause: %s (%d)\n", cause, parm[1] & 0x7f);

	return len;
}


static FUNC_DUMP(range_and_status_dump)
{
	ss7_message(ss7, "\t\t\tRange: %d\n", parm[0] & 0xff);
	return len;
}

static FUNC_RECV(range_and_status_receive)
{
	int i;
	int numcics;

	c->range = parm[0];
	numcics = c->range + 1;

	/* No status for these messages */
	if ((messagetype == ISUP_CQR) || (messagetype == ISUP_CQM) || (messagetype == ISUP_GRS))
		return len;

	for (i = 0; i < numcics; i++) {
		if (parm[1 + (i/8)] & (1 << (i%8)))
			c->status[i] = 1;
		else
			c->status[i] = 0;
	}

	return len;
}

static FUNC_SEND(range_and_status_transmit)
{
	int i, statuslen = 0;
	int numcics = c->range + 1;

	parm[0] = c->range & 0xff;

	/* No status for these messages */
	if ((messagetype == ISUP_CQR) || (messagetype == ISUP_CQM) || (messagetype == ISUP_GRS))
		return 1;

	statuslen = (numcics / 8) + !!(numcics % 8);

	for (i = 0; i < numcics; i++) {
		if (c->status[i])
			parm[1 + (i/8)] |= (1 << (i % 8));
	}

	return statuslen + 1;
}

static FUNC_DUMP(calling_party_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";

	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNI: %x\n", (parm[1] >> 7) & 0x1);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);
	ss7_message(ss7, "\t\t\tPresentation: %x\n", (parm[1] >> 2) & 0x3);
	ss7_message(ss7, "\t\t\tScreening: %x\n", parm[1] & 0x3);

	isup_get_number(numbuf, &parm[2], len - 2, oddeven);

	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);

	return len;
}

static FUNC_RECV(calling_party_num_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->calling_party_num, &parm[2], len - 2, oddeven);

	c->calling_nai = parm[0] & 0x7f;                /* Nature of Address Indicator */
	c->presentation_ind = (parm[1] >> 2) & 0x3;
	c->screening_ind = parm[1] & 0x3;

	return len;

}

static FUNC_SEND(calling_party_num_transmit)
{
	int oddeven, datalen;

	if (!c->calling_party_num[0] && c->presentation_ind != SS7_PRESENTATION_ADDR_NOT_AVAILABLE)
		return 0;

	if (c->calling_party_num[0] && c->presentation_ind != SS7_PRESENTATION_ADDR_NOT_AVAILABLE)
		isup_put_number(&parm[2], c->calling_party_num, &datalen, &oddeven);
	else {
		datalen = 0;
		oddeven = 0;
		c->calling_nai = 0;
	}

	parm[0] = (oddeven << 7) | c->calling_nai;      /* Nature of Address Indicator */
	 /* Assume E.164 ISDN numbering plan, calling number complete */
	parm[1] = ((c->presentation_ind == SS7_PRESENTATION_ADDR_NOT_AVAILABLE) ? 0 : (1 << 4)) |
		((c->presentation_ind & 0x3) << 2) |
		(c->screening_ind & 0x3);
	
	return datalen + 2;
}

static FUNC_DUMP(connected_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";

	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);
	ss7_message(ss7, "\t\t\tPresentation: %x\n", (parm[1] >> 2) & 0x3);
	ss7_message(ss7, "\t\t\tScreening: %x\n", parm[1] & 0x3);

	isup_get_number(numbuf, &parm[2], len - 2, oddeven);

	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);

	return len;
}

static FUNC_RECV(connected_num_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->connected_num, &parm[2], len - 2, oddeven);

	c->connected_nai = parm[0] & 0x7f;                /* Nature of Address Indicator */
	c->connected_presentation_ind = (parm[1] >> 2) & 0x3;
	c->connected_screening_ind = parm[1] & 0x3;

	return len;

}

static FUNC_SEND(connected_num_transmit)
{
	int oddeven = 0, datalen = 0;

	if (!c->col_req)
		return 0; /* if they don't ask we won't tell */
	

	if (!c->connected_num[0])
		c->connected_presentation_ind = SS7_PRESENTATION_ADDR_NOT_AVAILABLE;
	else
		isup_put_number(&parm[2], c->connected_num, &datalen, &oddeven);

	
	parm[0] = (oddeven << 7) | c->connected_nai;      /* Nature of Address Indicator */
	 /* Assume E.164 ISDN numbering plan, calling number complete */
	parm[1] = ((c->connected_presentation_ind & 0x3) << 2) | (c->connected_screening_ind & 0x3);
	return datalen + 2;
}

static FUNC_DUMP(originating_line_information_dump)
{
	char *name;

	switch (parm[0]) {
		case 0:
			name = " Plain Old Telephone Service POTS";
			break;
		case 1:
			name = " Multiparty line";
			break;
		case 2:
			name = " ANI Failure";
			break;
		case 3:
		case 4:
		case 5:
			name = " Unassigned";
			break;
		case 6:
			name = " Station Level Rating";
			break;
		case 7:
			name = " Special Operator Handling Required";
			break;
		case 8:
		case 9:
			name = "Unassigned";
			break;
		case 10:
			name = "Not assignable";
			break;
		case 11:
			name = "Unassigned";
			break;
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
		case 19:
			name = "Not assignable";
			break;
		case 20:
			name = "Automatic Identified Outward Dialing";
			break;
		case 21:
		case 22:
			name = "Unassigned";
			break;
		case 23:
			name = "Coin or Non-Coin";
			break;
		case 24:
		case 25:
			name = "Toll Free Service translated to POTS";
			break;
		case 26:
			name = "Unassigned";
			break;
		case 27:
			name = "Pay Station using Coin Control Signalling";
			break;
		case 28:
			name = "Unassigned";
			break;
		case 29:
			name = "Prison/Inmate Service";
			break;
		case 30:
		case 31:
		case 32:
			name = "Intercept";
			break;
		case 33:
			name = "Unassigned";
			break;
		case 34:
			name = "Telco Operator Handled Call";
			break;
		case 35:
		case 36:
		case 37:
		case 38:
		case 39:
			name = "Unassigned";
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
		case 48:
		case 49:
			name = "Unrestricted Use - locally determined by carrier";
			break;
		case 50:
		case 51:
			name = "Unassigned";
			break;
		case 52:
			name = "OUTWATS";
			break;
		case 53:
		case 54:
		case 55:
		case 56:
		case 57:
		case 58:
		case 59:
			name = "Unassigned";
			break;
		case 60:
			name = "TRS Unrestricted Line";
			break;
		case 61:
			name = "Cellular Wireless PCS Type 1";
			break;
		case 62:
			name = "Cellular Wireless PCS Type 2";
			break;
		case 63:
			name = "Cellular Wireless PCS Roaming";
			break;
		case 64:
		case 65:
			name = "Unassigned";
			break;
		case 66:
			name = "TRS Hotel Motel";
			break;
		case 67:
			name = "TRS Restricted Line";
			break;
		case 68:
		case 69:
			name = "Unassigned";
			break;
		case 70:
			name = "Pay Station No network Coin Control Signalling";
			break;
		case 71:
		case 72:
		case 73:
		case 74:
		case 75:
		case 76:
		case 77:
		case 78:
		case 79:
			name = "Unassigned";
			break;
		case 80:
		case 81:
		case 82:
		case 83:
		case 84:
		case 85:
		case 86:
		case 87:
		case 88:
		case 89:
			name = "Reserved";
			break;
		case 90:
		case 91:
		case 92:
			name = "Unassigned";
			break;
		case 93:
			name = "Private Virtual Network Type of service call";
			break;
		case 94:
		case 95:
		case 96:
		case 97:
		case 98:
		case 99:
			name = "Unassigned";
			break;

		default:
			name = "Unknown to Asterisk ";
	}
	ss7_message(ss7, "\t\t\tLine info code: %s (%d)\n", name, parm[0]);

	return 1;
}

static FUNC_RECV(originating_line_information_receive) 
{
	c->oli_ani2 = parm[0];

	return 1;
}

static FUNC_SEND(originating_line_information_transmit) 
{
	if (c->oli_ani2 < 0) {  /* Allow dialplan to strip OLI parm if you don't want to resend what was received */
		return 0;
	} else if (c->oli_ani2 < 99) {
		parm[0] = c->oli_ani2;  
		return 1;
	} else {
		parm[0] = 0x00; /* This value is setting OLI equal to POTS line. */
		return 1;
	}   
}

static FUNC_DUMP(carrier_identification_dump)
{
	return len;
}

static FUNC_RECV(carrier_identification_receive)
{
	return len;
}

static FUNC_SEND(carrier_identification_transmit)
{
	parm[0] = 0x22;  /* 4 digit CIC */
	parm[1] = 0x00;  /* would send default 0000 */
	parm[2] = 0x00;

	return 3;
}
static FUNC_DUMP(jip_dump)
{
	char numbuf[64] = "";
	
	isup_get_number(numbuf, &parm[0], len , 0);
	ss7_message(ss7, "\t\t\tJIP: %s\n", numbuf);
	return len;
}

static FUNC_RECV(jip_receive)
{ 
	isup_get_number(c->jip_number, &parm[0], len, 0);
	return len;
}

static FUNC_SEND(jip_transmit)
{ 
	int oddeven, datalen;
	
	if  (c->jip_number[0]) {
		isup_put_number(&parm[0], c->jip_number, &datalen, &oddeven);
		return datalen;
	}
	return 0;
}

static FUNC_DUMP(hop_counter_dump)
{
	return 1;
}

static FUNC_RECV(hop_counter_receive)
{
	return 1;
}

static FUNC_SEND(hop_counter_transmit)
{
	parm[0] = 0x01; /* would send hop counter with value of 1 */
	return 1;
}

static FUNC_RECV(charge_number_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->charge_number, &parm[2], len - 2, oddeven);

	c->charge_nai = parm[0] & 0x7f;                /* Nature of Address Indicator */
	c->charge_num_plan = (parm[1] >> 4) & 0x7;

	return len;
}

static FUNC_DUMP(charge_number_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";

	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);

	isup_get_number(numbuf, &parm[2], len - 2, oddeven);

	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);

	return len;
}

static FUNC_SEND(charge_number_transmit)  //ANSI network
{
	int oddeven, datalen;

	if (!c->charge_number[0])
		return 0;

	isup_put_number(&parm[2], c->charge_number, &datalen, &oddeven);  /* use the value from callerid in sip.conf to fill charge number */

	parm[0] = (oddeven << 7) | c->charge_nai;        /* Nature of Address Indicator = odd/even and ANI of the Calling party, subscriber number */
	parm[1] = (1 << 4) | 0x0;       //c->charge_num_plan    /* Assume E.164 ISDN numbering plan, calling number complete and make sure reserved bits are zero */

	return datalen + 2;

}

static FUNC_SEND(continuity_ind_transmit)
{
	if (c->cot_check_passed)
		parm[0] = 0x01;
	else
		parm[0] = 0x00;

	return 1;
}

static FUNC_RECV(continuity_ind_receive)
{
	if (0x1 & parm[0])
		c->cot_check_passed = 1;
	else
		c->cot_check_passed = 0;
	return 1;
}

static FUNC_DUMP(continuity_ind_dump)
{
	ss7_message(ss7, "\t\t\tContinuity Check: %s\n", (0x01 & parm[0]) ? "successful" : "failed");

	return 1;
}

static FUNC_DUMP(circuit_group_supervision_dump)
{
	char *name;

	switch (parm[0] & 0x3) {
	case 0:
		name = "Maintenance oriented";
		break;
	case 1:
		name = "Hardware Failure oriented";
		break;
	case 2:
		name = "Reserved for national use";
		break;
	case 3:
		name = "Spare";
		break;
	default:
		name = "Huh?!";
	}
	ss7_message(ss7, "\t\t\tType indicator: %s\n", name);

	return 1;
}

static FUNC_RECV(circuit_group_supervision_receive)
{
	c->cicgroupsupervisiontype = 0x3 & parm[0];
	return 1;
}

static FUNC_SEND(circuit_group_supervision_transmit)
{
	parm[0] = c->cicgroupsupervisiontype & 0x3;
	return 1;
}

static FUNC_DUMP(event_info_dump)
{
	char *name;

	switch (parm[0]) {
		case 0:
			name = "spare";
			break;
		case 1:
			name = "ALERTING";
			break;
		case 2:
			name = "PROGRESS";
			break;
		case 3:
			name = "In-band information or an appropriate pattern is now available";
			break;
		case 4:
			name = "Call forward on busy";
			break;
		case 5:
			name = "Call forward on no reply";
			break;
		case 6:
			name = "Call forward unconditional";
			break;
		default:
			name = "Spare";
			break;
	}
	ss7_message(ss7, "\t\t\t%s\n", name);
	return 1;
}

static FUNC_RECV(event_info_receive)
{
	c->event_info = parm[0];
	return 1;
}

static FUNC_SEND(event_info_transmit)
{
	parm[0] = c->event_info;
	return 1;
}

static FUNC_DUMP(redirection_info_dump)
{
	char *redirect_ind, *orig_redir_reas, *redir_reas;

	switch (parm[0] & 0x7) {
		case 0:
			redirect_ind = "No Redirection (national use)";
			break;
		case 1:
			redirect_ind = "Call rerouted (national use)";
			break;
		case 2:
			redirect_ind = "Call rerouted, all rediection information presentation restricted (national use)";
			break;
		case 3:
			redirect_ind = "Call diverted";
			break;
		case 4:
			redirect_ind = "Call diverted, all redirection information presentation restricted";
			break;
		case 5:
			redirect_ind = "Call rerouted, redirection number presentation restricted (national use)";
			break;
		case 6:
			redirect_ind = "Call diversion, redirection number presentation restricted (national use)";
			break;
		case 7:
			redirect_ind = "spare";
			break;
		default:
			redirect_ind = "Unknown";
			break;
	}

	ss7_message(ss7, "\t\t\tRedirecting indicator: %s (%d)\n", redirect_ind, parm[0] & 0x7);

	switch ((parm[0] >> 4) & 0xf) {
		case 0:
			orig_redir_reas = "Unknown/not available";
			break;
		case 1:
			orig_redir_reas = "User busy (national use)";
			break;
		case 2:
			orig_redir_reas = "No reply (national use)";
			break;
		case 3:
			orig_redir_reas = "Unconditional (national use)";
			break;
		default:
			orig_redir_reas = "spare";
			break;
	}

	ss7_message(ss7, "\t\t\tOriginal redirection reason: %s (%d)\n", orig_redir_reas, (parm[0] >> 4) & 0xf);
	ss7_message(ss7, "\t\t\tRedirection counter: %d\n", parm[1] & 0x7);

	switch ((parm[1] >> 4) & 0xf) {
		case 0:
			redir_reas = "Unknown/not available";
			break;
		case 1:
			redir_reas = "User busy";
			break;
		case 2:
			redir_reas = "No reply";
			break;
		case 3:
			redir_reas = "Unconditional";
			break;
		case 4:
			redir_reas = "Deflection during alerting";
			break;
		case 5:
			redir_reas = "Deflection immediate response";
			break;
		case 6:
			redir_reas = "Mobile subscriber not reachable";
			break;
		default:
			redir_reas = "spare";
			break;
	}

	ss7_message(ss7, "\t\t\tRedirecting reason: %s (%d)\n", redir_reas, (parm[1] >> 4) & 0xf);

	return 2;
}

static FUNC_RECV(redirection_info_receive)
{
	c->redirect_info = 1;
	c->redirect_info_ind = parm[0] & 0x7;
	c->redirect_info_orig_reas = (parm[0] >> 4) & 0xf;
	c->redirect_info_counter = parm[1] & 0x7;
	c->redirect_info_reas = (parm[1] >> 4) & 0xf;
	return 2;
}

static FUNC_SEND(redirection_info_transmit)
{
	if (!c->redirect_info)
		return 0;

	parm[0] = (c->redirect_info_ind & 0x7) | ((c->redirect_info_orig_reas << 4) & 0xf0);
	parm[1] = (c->redirect_info_counter & 0x7) | ((c->redirect_info_reas << 4) & 0xf0); 
	return 2;
}

static FUNC_RECV(generic_name_receive)
{
	c->generic_name_typeofname = (parm[0] >> 5) & 0x7;
	c->generic_name_avail = (parm[0] >> 4) & 0x1;
	c->generic_name_presentation = parm[0] & 0x3;
	memcpy(c->generic_name, &parm[1], len - 1);
	return len;
}

static FUNC_DUMP(generic_name_dump)
{
	unsigned int typeofname = (parm[0] >> 5) & 0x7;
	unsigned int avail = (parm[0] >> 4) & 0x1;
	unsigned int presentation = parm[0] & 0x3;
	char name[ISUP_MAX_NAME + 1];

	memcpy(name, &parm[1], len - 1);

	ss7_message(ss7, "\t\t\tType of Name: %s (%d)\n", (typeofname == 1) ? "Calling Name" : "Unknown", typeofname);
	ss7_message(ss7, "\t\t\tAvail: %s (%d)\n", (avail == 1) ? "Name not available" : "Name available, or availability unknown", avail);
	ss7_message(ss7, "\t\t\tPresentation: %d\n",  presentation);
	ss7_message(ss7, "\t\t\tName: %s\n", name);

	return len;
}

static FUNC_SEND(generic_name_transmit)
{
	int namelen = strlen(c->generic_name);

	/* Check to see if generic name is set before we try to add it */
	if (!c->generic_name[0])
		return 0;

	parm[0] = (c->generic_name_typeofname << 5) | ((c->generic_name_avail & 0x1) << 4) | (c->generic_name_presentation & 0x3);
	memcpy(&parm[1], c->generic_name, namelen);

	return namelen + 1;
}

static FUNC_DUMP(generic_address_dump)
{
	int oddeven = (parm[1] >> 7) & 0x1;
	char numbuf[64] = "";
	
	ss7_message(ss7, "\t\t\tType of address: %x\n", parm[0]);
	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[1] & 0x7f);
	ss7_message(ss7, "\t\t\tOddEven: %x\n", (parm[1] >> 7) & 0x1);
	ss7_message(ss7, "\t\t\tReserved: %x\n", parm[2] & 0x3);
	ss7_message(ss7, "\t\t\tPresentation: %x\n", (parm[2] >> 2) & 0x3);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[2] >> 4) & 0x7);
	
	isup_get_number(numbuf, &parm[3], len - 3, oddeven);
	
	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);
	
	return len;
}

static FUNC_RECV(generic_address_receive)
{
	int oddeven = (parm[1] >> 7) & 0x1;
	
	c->gen_add_type = parm[0];
	c->gen_add_nai = parm[1] & 0x7f;
	c->gen_add_pres_ind = (parm[2] >> 2) & 0x3;
	c->gen_add_num_plan = (parm[2] >> 4) & 0x7;
	
	isup_get_number(c->gen_add_number, &parm[3], len - 3, oddeven);
	
	return len;
}

static FUNC_SEND(generic_address_transmit)
{
	
	int oddeven, datalen;
	
	if (!c->gen_add_number[0])
		return 0;
	
	isup_put_number(&parm[3], c->gen_add_number, &datalen, &oddeven);
	
	parm[0] = c->gen_add_type;
	parm[1] = (oddeven << 7) | c->gen_add_nai;      /* Nature of Address Indicator */
	parm[2] = (c->gen_add_num_plan << 4) |                           
		((c->gen_add_pres_ind & 0x3) << 2) |
		( 0x00 & 0x3);
	
	return datalen + 3;
}


static FUNC_DUMP(generic_digits_dump)
{
	int oddeven = (parm[0] >> 5) & 0x7;
	char numbuf[64] = "";
	
	ss7_message(ss7, "\t\t\tType of digits: %x\n", parm[0] & 0x1f);
	ss7_message(ss7, "\t\t\tEncoding Scheme: %x\n", (parm[0] >> 5) & 0x7);
	isup_get_number(numbuf, &parm[1], len - 1, oddeven);
	ss7_message(ss7, "\t\t\tAddress digits: %s\n", numbuf);

	return len;
	
}

static FUNC_RECV(generic_digits_receive)
{
	c->gen_dig_scheme = (parm[0] >> 5) & 0x7;
	c->gen_dig_type = parm[0] & 0x1f;
	
	isup_get_number(c->gen_dig_number, &parm[1], len - 1, c->gen_dig_scheme);
	return len;
}

static FUNC_SEND(generic_digits_transmit)
{
	int oddeven, datalen;
	
	if (!c->gen_dig_number[0])
		return 0;
	
	switch (c->gen_dig_type) {
		case 0:
		case 1:
		case 2: /* used for sending digit strings */
			isup_put_number(&parm[1], c->gen_dig_number, &datalen, &oddeven);
			parm[0] = (oddeven << 5 ) | c->gen_dig_type;
			break;
		case 3:	 /*used for sending BUSINESS COMM. GROUP IDENTIY type */
			isup_put_generic(&parm[1], c->gen_dig_number, &datalen);
			parm[0] = (c->gen_dig_scheme << 5 ) | c->gen_dig_type;
			break;
		default:
			isup_put_number(&parm[1], c->gen_dig_number, &datalen, &oddeven);
			parm[0] = (oddeven << 5 ) | c->gen_dig_type;
			break;
	}
	return datalen + 1;
}

static FUNC_DUMP(original_called_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";
	
	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);
	ss7_message(ss7, "\t\t\tPresentation: %x\n", (parm[1] >> 2) & 0x3);
	
	isup_get_number(numbuf, &parm[2], len - 2, oddeven);
	
	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);
	
	return len;
}

static FUNC_RECV(original_called_num_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->orig_called_num, &parm[2], len - 2, oddeven);

	c->orig_called_nai = parm[0] & 0x7f;
	c->orig_called_pres_ind = (parm[1] >> 2) & 0x3;
	c->orig_called_screening_ind = parm[1] & 0x3;

	return len;
}

static FUNC_SEND(original_called_num_transmit)
{
	int oddeven, datalen;

	if (!c->orig_called_num[0])
		return 0;

	isup_put_number(&parm[2], c->orig_called_num, &datalen, &oddeven);

	parm[0] = (oddeven << 7) | c->orig_called_nai;      /* Nature of Address Indicator */
	parm[1] = (1 << 4) |                            /* Assume E.164 ISDN numbering plan, calling number complete */
		((c->orig_called_pres_ind & 0x3) << 2) |
		(c->orig_called_screening_ind & 0x3);

	return datalen + 2;
}

static FUNC_DUMP(echo_control_info_dump)
{
	unsigned char ba = parm[0] & 0x3;
	unsigned char dc = (parm[0] >> 2) & 0x3;
	unsigned char fe = (parm[0] >> 4) & 0x3;
	unsigned char hg = (parm[0] >> 6) & 0x3;
	char *ba_str, *dc_str, *fe_str, *hg_str;

	switch (ba) {
		case 0:
			ba_str = "no information";
			break;
		case 1:
			ba_str = "outgoing echo control device not included and not available";
			break;
		case 2:
			ba_str = "outgoing echo control device included";
			break;
		case 3:
			ba_str = "outgoing echo control device not included but available";
			break;
		default:
			ba_str = "unknown";
			break;
	}

	switch (dc) {
		case 0:
			dc_str = "no information";
			break;
		case 1:
			dc_str = "incoming echo control device not included and not available";
			break;
		case 2:
			dc_str = "incoming echo control device included";
			break;
		case 3:
			dc_str = "incoming echo control device not included but available";
			break;
		default:
			dc_str = "unknown";
			break;
	}

	switch (fe) {
		case 0:
			fe_str = "no information";
			break;
		case 1:
			fe_str = "outgoing echo control device activation request";
			break;
		case 2:
			fe_str = "outgoing echo control device deactivation request";
			break;
		case 3:
			fe_str = "spare";
			break;
		default:
			fe_str = "unknown";
			break;
	}

	switch (hg) {
		case 0:
			hg_str = "no information";
			break;
		case 1:
			hg_str = "incoming echo control device activation request";
			break;
		case 2:
			hg_str = "incoming echo control device deactivation request";
			break;
		case 3:
			hg_str = "spare";
			break;
		default:
			hg_str = "unknown";
			break;
	}

	ss7_message(ss7, "\t\t\tOutgoing echo control device information: %s (%d)\n", ba_str, ba);
	ss7_message(ss7, "\t\t\tIncoming echo control device information: %s (%d)\n", dc_str, dc);
	ss7_message(ss7, "\t\t\tOutgoing echo control device request: %s (%d)\n", fe_str, fe);
	ss7_message(ss7, "\t\t\tIncoming echo control device request: %s (%d)\n", hg_str, hg);

	return len;
}

static FUNC_DUMP(parameter_compat_info_dump)
{
	return len;
}

static FUNC_DUMP(propagation_delay_cntr_dump)
{
	ss7_message(ss7, "\t\t\tDelay: %dms\n", (unsigned short)(((parm[0] & 0xff) << 8) | (parm[0] & 0xff)));
	return len;
}

static FUNC_DUMP(circuit_state_ind_dump)
{
	unsigned char dcbits, babits, febits;
	char *ba_str, *dc_str, *fe_str;
	int i;
	
	for (i = 0; i < len; i++) {
		babits = parm[i] & 0x3;
		dcbits = (parm[i] >> 2) & 0x3;
		febits = (parm[i] >> 4) & 0x3;

		if (dcbits == 0) {
			switch (babits) {
				case 0:
					ba_str = "transient";
					break;
				case 1:
				case 2:
					ba_str = "spare";
					break;
				case 3:
					ba_str = "unequipped";
					break;
			}
		} else {
			switch (babits) {
				case 0:
					ba_str = "no blocking (active)";
					break;
				case 1:
					ba_str = "locally blocked";
					break;
				case 2:
					ba_str = "remotely blocked";
					break;
				case 3:
					ba_str = "locally and remotely blocked";
					break;
			}

			switch (dcbits) {
				case 1:
					dc_str = "circuit incoming busy";
					break;
				case 2:
					dc_str = "circuit outgoing busy";
					break;
				case 3:
					dc_str = "idle";
					break;
			}

			switch (febits) {
				case 0:
					fe_str = "no blocking (active)";
					break;
				case 1:
					fe_str = "locally blocked";
					break;
				case 2:
					fe_str = "remotely blocked";
					break;
				case 3:
					fe_str = "locally and remotely blocked";
					break;
			}

		}

		ss7_message(ss7, "\t\t\tMaintenance blocking state: %s (%d)\n", ba_str, babits);
		if (!dcbits)
			continue;
		ss7_message(ss7, "\t\t\tCall processing state: %s (%d)\n", dc_str, dcbits);
		ss7_message(ss7, "\t\t\tHardware blocking state: %s (%d)\n", fe_str, febits);
	}
	return len;
}

static FUNC_SEND(circuit_state_ind_transmit)
{
	int numcics = c->range + 1, i;

	for (i = 0; i < numcics; i++)
		parm[i] = c->status[i];

	return numcics;
}

static FUNC_DUMP(tns_dump)
{
	ss7_message(ss7, "\t\t\tType of Network: %x\n", (parm[0] >> 4) & 0x7);
	ss7_message(ss7, "\t\t\tNetwork ID plan: %x\n", parm[0] & 0xf);
	ss7_message(ss7, "\t\t\tNetwork ID: %x %x\n", parm[1], parm[2]);
	ss7_message(ss7, "\t\t\tCircuit Code: %x\n", (parm[3] >> 4) & 0xf);
	
	return len;
}

static FUNC_SEND(tns_transmit)
{
	return 0;
}

static FUNC_RECV(tns_receive)
{
	return len;
}

static FUNC_DUMP(generic_notofication_ind_dump)
{
	int pos = 0;
	ss7_message(ss7, "\t\t\tNotification indicator: ");
	
	while (pos < len && (pos || !(parm[pos - 1] & 0x80))) {
		switch (parm[pos] & 0x7f) {
			case 0x00:
				ss7_message(ss7, "user suspended; ");
				break;
			case 0x01:
				ss7_message(ss7, "user resumed; ");
				break;
			case 0x02:
				ss7_message(ss7, "bearer service change; ");
				break;
			case 0x03:
				ss7_message(ss7, "discriminator for extension to ASN.1; ");
				break;
			case 0x04:
				ss7_message(ss7, "call completion delay; ");
				break;
			case 0x42:
				ss7_message(ss7, "conference established; ");
				break;
			case 0x43:
				ss7_message(ss7, "conference disconnected; ");
				break;
			case 0x44:
				ss7_message(ss7, "other party added; ");
				break;
			case 0x45:
				ss7_message(ss7, "isolated; ");
				break;
			case 0x46:
				ss7_message(ss7, "reattached; ");
				break;
			case 0x47:
				ss7_message(ss7, "other party isolated; ");
				break;
			case 0x48:
				ss7_message(ss7, "other party reattached; ");
				break;
			case 0x49:
				ss7_message(ss7, "other party split; ");
				break;
			case 0x4a:
				ss7_message(ss7, "other party disconnected; ");
				break;
			case 0x4b:
				ss7_message(ss7, "other party floating; ");
				break;
			case 0x60:
				ss7_message(ss7, "call is a waiting call; ");
				break;
			case 0x68:
				ss7_message(ss7, "diversion activated; ");
				break;
			case 0x69:
				ss7_message(ss7, "call transfer, alerting; ");
				break;
			case 0x6a:
				ss7_message(ss7, "call transfer, active; ");
				break;
			case 0x79:
				ss7_message(ss7, "remote hold; ");
				break;
			case 0x7a:
				ss7_message(ss7, "remote retrieval; ");
				break;
			case 0x7b:
				ss7_message(ss7, "remote is diverting; ");
				break;
				
			default:
				ss7_message(ss7, "reserved; ");
		}
		pos++;
	}
	ss7_message(ss7, "\n");
	return len;
}

static FUNC_SEND(generic_notofication_ind_transmit)
{
	return 0;
}

static FUNC_RECV(generic_notofication_ind_receive)
{
	return len;
}

static FUNC_SEND(lspi_transmit)
{
	/* On Nortel this needs to be set to ARM the RLT functionality. */
	/* This causes the Nortel switch to return the CALLREFERENCE Parm on the ACM of the outgoing call */
	/* This parm has more fields that can be set but Nortel DMS-250/500 needs it set as below */
	if (c->lspi_scheme) {
		parm[0] = c->lspi_scheme << 5 | c->lspi_type;  /* only setting parms for NORTEL RLT on IMT trktype */
		return 1;
	}
	return 0;
}

static FUNC_RECV(lspi_receive)
{
	c->lspi_type = parm[0] & 0x1f;
	c->lspi_scheme = parm[0] >> 5 & 0x7;
	c->lspi_context = parm[1] & 0xf;
	isup_get_number(c->lspi_ident, &parm[2], len - 2, c->lspi_scheme);
	
	return len;
}

static FUNC_DUMP(lspi_dump)
{
	ss7_message(ss7, "\t\t\tLSPI Type: %x\n", parm[0] & 0x1f);
	ss7_message(ss7, "\t\t\tEncoding Scheme: %x\n", (parm[0] >> 5) & 0x7);
	ss7_message(ss7, "\t\t\tContext ID: %x\n", parm[1] & 0xf);
	ss7_message(ss7, "\t\t\tSpare: %x\n", (parm[1] >> 4) & 0xf);
	ss7_message(ss7, "\t\t\tLSP Identity: %x\n", parm[2]);
	
	return len;
}

static FUNC_DUMP(call_ref_dump)
{
	unsigned int ptc, callr;
	
	callr = parm[0] | (parm[1] << 8) | (parm[2] << 16);
	if (ss7->switchtype == SS7_ANSI)
		ptc = parm[3] | (parm[4] << 8) | (parm[5] << 16);
	else
		ptc = parm[3] | (parm[4] << 8);
	
	ss7_message(ss7, "\t\t\tCall identity: %d\n", callr);
	if (ss7->switchtype == SS7_ANSI)
		ss7_message(ss7, "\t\t\tPC: Net-CLstr-Mbr: %d-%d-%d\n",(ptc >> 16) & 0xff, (ptc >> 8) & 0xff, ptc & 0xff);
	else
		ss7_message(ss7, "\t\t\tPC: 0x%x\n", ptc);
	
	return len;
}

static FUNC_SEND(call_ref_transmit)
{
	if (c->call_ref_ident) {
		if (ss7->switchtype == SS7_ANSI) {
			parm[0] = c->call_ref_ident & 0xff;
			parm[1] = (c->call_ref_ident >> 8) & 0xff;
			parm[2] = (c->call_ref_ident >> 16) & 0xff;
			parm[3] = c->call_ref_pc & 0xff;
			parm[4] = (c->call_ref_pc >> 8) & 0xff;
			parm[5] = (c->call_ref_pc >> 16) & 0xff;
			return 6;
		} else {
			parm[0] = c->call_ref_ident & 0xff;
			parm[1] = (c->call_ref_ident >> 8) & 0xff;
			parm[2] = (c->call_ref_ident >> 16) & 0xff;
			parm[3] = c->call_ref_pc & 0xff;
			parm[4] = (c->call_ref_pc >> 8) & 0x3f;
			return 5;
		}
	}
	return 0;
}

static FUNC_RECV(call_ref_receive)
{
	if (ss7->switchtype == SS7_ANSI) {
		c->call_ref_ident = parm[0] | (parm[1] << 8) | (parm[2] << 16);
		c->call_ref_pc = parm[3] | (parm[4] << 8) | (parm[5] << 16);
	} else {
		c->call_ref_ident = parm[0] | (parm[1] << 8) | (parm[2] << 16);
		c->call_ref_pc = parm[3] | ((parm[4] & 0x3f) << 8);
	}
	return len;
}

static FUNC_DUMP(facility_ind_dump)
{
	ss7_message(ss7, "\t\t\tFacility Indicator: %x\n", parm[0]);
	return 1;
}

static FUNC_RECV(facility_ind_receive)
{
	return 1;
}

static FUNC_SEND(facility_ind_transmit)
{
	parm[0] = 0x10; /* Setting Value to Nortel DMS-250/500 needs for RLT */
	return 1;
}

static FUNC_DUMP(redirecting_number_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";
	
	ss7_message(ss7, "\t\t\tNature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "\t\t\tNI: %x\n", (parm[1] >> 7) & 0x1);
	ss7_message(ss7, "\t\t\tNumbering plan: %x\n", (parm[1] >> 4) & 0x7);
	ss7_message(ss7, "\t\t\tPresentation: %x\n", (parm[1] >> 2) & 0x3);
	ss7_message(ss7, "\t\t\tScreening: %x\n", parm[1] & 0x3);
	
	isup_get_number(numbuf, &parm[2], len - 2, oddeven);
	
	ss7_message(ss7, "\t\t\tAddress signals: %s\n", numbuf);
	
	return len;
}

static FUNC_RECV(redirecting_number_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	
	isup_get_number(c->redirecting_num, &parm[2], len - 2, oddeven);
	
	c->redirecting_num_nai = parm[0] & 0x7f;                /* Nature of Address Indicator */
	c->redirecting_num_presentation_ind = (parm[1] >> 2) & 0x3;
	c->redirecting_num_screening_ind = parm[1] & 0x3;
	
	return len;
	
}

static FUNC_SEND(redirecting_number_transmit)
{
	int oddeven, datalen;

	if (!c->redirecting_num[0])
		return 0;

	isup_put_number(&parm[2], c->redirecting_num, &datalen, &oddeven);
	parm[0] = (oddeven << 7) | c->redirecting_num_nai;      /* Nature of Address Indicator */
	parm[1] = (1 << 4) |                            /* Assume E.164 ISDN numbering plan, calling number complete */
		((c->redirecting_num_presentation_ind & 0x3) << 2) |
		(c->redirecting_num_screening_ind & 0x3);

	return datalen + 2;
}	

static FUNC_DUMP(redirect_counter_dump)
{
	ss7_message(ss7, "\t\t\tRedirect count: %i\n", parm[0] & 0x1f);
	return 1;
}

static FUNC_RECV(redirect_counter_receive)
{
	c->redirect_counter = parm[0] & 0x1f;
	return 1;
}

static FUNC_SEND(redirect_counter_transmit)
{
	if (!c->redirect_counter)
		return 0;

	parm[0] = c->redirect_counter & 0x1f;
	return 1;
}

static FUNC_DUMP(susres_ind_dump)
{
	ss7_message(ss7, "\t\t\tSUS/RES indicator: %s (%i)\n", (parm[0] & 1) ? "network initiated" : "ISDN subscriber initiated", parm[0] & 1);
	return 1;
}

static FUNC_RECV(susres_ind_receive)
{
	c->susres_ind = parm[0] & 1;
	return 1;
}

static FUNC_SEND(susres_ind_transmit)
{
	parm[0] = c->susres_ind & 1;
	return 1;
}

static FUNC_DUMP(inr_ind_dump)
{
	ss7_message(ss7, "\t\t\tCalling party address %srequested\n", (parm[0] & 0x1) ? "" : "not ");
	ss7_message(ss7, "\t\t\tHolding %srequested\n", (parm[0] & 0x2) ? "" : "not ");
	ss7_message(ss7, "\t\t\tCalling party category %srequested\n", (parm[0] & 0x8) ? "" : "not ");
	ss7_message(ss7, "\t\t\tCharge information %srequested\n", (parm[0] & 0x10) ? "" : "not ");
	ss7_message(ss7, "\t\t\tMalicous call identification %srequested\n", (parm[0] & 0x80) ? "" : "not ");
	return 2;
}

static FUNC_RECV(inr_ind_receive)
{
	c->inr_ind[0] = parm[0];
	c->inr_ind[1] = parm[1];
	return 2;
}

static FUNC_SEND(inr_ind_transmit)
{
	parm[0] = c->inr_ind[0];
	parm[1] = c->inr_ind[1];
	return 2;
}

static FUNC_DUMP(inf_ind_dump)
{
	ss7_message(ss7, "\t\t\tCalling party address: %s\n",
		(parm[0] & 0x2) ? ((parm[0] & 0x1) ? "included" : "spare" ) : ((parm[0] & 0x1) ? "not available" : "not included") );
	ss7_message(ss7, "\t\t\tHold: %sprovided\n", (parm[0] & 0x4) ? "" : "not ");
	ss7_message(ss7, "\t\t\tCalling party's category %sincluded\n", (parm[0] & 0x20) ? "" : "not ");
	ss7_message(ss7, "\t\t\tCharge information %sincluded\n", (parm[0] & 0x40) ? "" : "not ");
	ss7_message(ss7, "\t\t\t%s\n", (parm[0] & 0x80) ? "Unsolicated" : "Solicated");

	return 2;
}

static FUNC_RECV(inf_ind_receive)
{
	c->inf_ind[0] = parm[0];
	c->inf_ind[1] = parm[1];
	if ((parm[0] & 0x3) == 0x1)
		c->presentation_ind = SS7_PRESENTATION_ADDR_NOT_AVAILABLE;
	return 2;	
}

static FUNC_SEND(inf_ind_transmit)
{
	parm[0] = c->inf_ind[0];
	parm[1] = c->inf_ind[1];
	return 2;
}

static FUNC_DUMP(access_transport_dump)
{
	return len;	
}	
static FUNC_RECV(access_transport_receive)
{
	return len;	
}	

static FUNC_SEND(access_transport_transmit)
{
	return len;	
}	

static FUNC_DUMP(subs_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64];

	isup_get_number(numbuf, &parm[1], len - 1, oddeven);
	ss7_message(ss7, "\t\t\tSubsequent signals: %s\n", numbuf);
	
	return len;
}

static FUNC_RECV(subs_num_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->called_party_num, &parm[1], len - 1, oddeven);

	return len;
}

static FUNC_SEND(subs_num_transmit)
{
	int oddeven, datalen;

	isup_put_number(&parm[1], c->called_party_num, &datalen, &oddeven);
	parm[0] = (oddeven << 7);

	return datalen + 1;
}

static FUNC_DUMP(cug_interlock_code_dump)
{
	char ni[5];
	unsigned short code;
	
	ni[0] = digit2char(parm[0] >> 4);
	ni[1] = digit2char(parm[0] & 0x0f);
	ni[2] = digit2char(parm[1] >> 4);
	ni[3] = digit2char(parm[1] & 0x0f);
	ni[4] = '\0';
	
	code = (parm[2] << 8) | parm [3];

	ss7_message(ss7, "\t\t\tNetwork Identify: %s\n", ni);
	ss7_message(ss7, "\t\t\tBinary Code: %d\n", code);
	return 4;
}

static FUNC_RECV(cug_interlock_code_receive)
{
	c->cug_interlock_ni[0] = digit2char(parm[0] >> 4);
	c->cug_interlock_ni[1] = digit2char(parm[0] & 0x0f);
	c->cug_interlock_ni[2] = digit2char(parm[1] >> 4);
	c->cug_interlock_ni[3] = digit2char(parm[1] & 0x0f);
	c->cug_interlock_ni[4] = '\0';
	
	c->cug_interlock_code = (parm[2] << 8) | parm [3];
	return 4;
}

static FUNC_SEND(cug_interlock_code_transmit)
{
	if (!(c->cug_indicator == ISUP_CUG_OUTGOING_ALLOWED || c->cug_indicator == ISUP_CUG_OUTGOING_NOT_ALLOWED))
		return 0;
		
	parm[0] = (char2digit(c->cug_interlock_ni[0]) << 4) | char2digit(c->cug_interlock_ni[1]);
	parm[1] = (char2digit(c->cug_interlock_ni[2]) << 4) | char2digit(c->cug_interlock_ni[3]);
	
	parm[2] = c->cug_interlock_code >> 8;
	parm[3] = c->cug_interlock_code & 0xff;
		
	return 4;
}

static struct parm_func parms[] = {
	{ISUP_PARM_NATURE_OF_CONNECTION_IND, "Nature of Connection Indicator", nature_of_connection_ind_dump, nature_of_connection_ind_receive, nature_of_connection_ind_transmit },
	{ISUP_PARM_FORWARD_CALL_IND, "Forward Call Indicators", forward_call_ind_dump, forward_call_ind_receive, forward_call_ind_transmit },
	{ISUP_PARM_CALLING_PARTY_CAT, "Calling Party's Category", calling_party_cat_dump, calling_party_cat_receive, calling_party_cat_transmit},
	{ISUP_PARM_TRANSMISSION_MEDIUM_REQS, "Transmission Medium Requirements", transmission_medium_reqs_dump, transmission_medium_reqs_receive, transmission_medium_reqs_transmit},
	{ISUP_PARM_USER_SERVICE_INFO, "User Service Information", NULL, user_service_info_receive, user_service_info_transmit},
	{ISUP_PARM_CALLED_PARTY_NUM, "Called Party Number", called_party_num_dump, called_party_num_receive, called_party_num_transmit},
	{ISUP_PARM_CAUSE, "Cause Indicator", cause_dump, cause_receive, cause_transmit},
	{ISUP_PARM_CONTINUITY_IND, "Continuity Indicator", continuity_ind_dump, continuity_ind_receive, continuity_ind_transmit},
	{ISUP_PARM_ACCESS_TRANS, "Access Transport", access_transport_dump, access_transport_receive, access_transport_transmit},
	{ISUP_PARM_BUSINESS_GRP, "Business Group"},
	{ISUP_PARM_CALL_REF, "Call Reference", call_ref_dump, call_ref_receive, call_ref_transmit},
	{ISUP_PARM_CALLING_PARTY_NUM, "Calling Party Number", calling_party_num_dump, calling_party_num_receive, calling_party_num_transmit},
	{ISUP_PARM_CARRIER_ID, "Carrier Identification", carrier_identification_dump, carrier_identification_receive, carrier_identification_transmit},
	{ISUP_PARM_SELECTION_INFO, "Selection Information"},
	{ISUP_PARM_CHARGE_NUMBER, "Charge Number", charge_number_dump, charge_number_receive, charge_number_transmit},
	{ISUP_PARM_CIRCUIT_ASSIGNMENT_MAP, "Circuit Assignment Map"},
	{ISUP_PARM_CONNECTION_REQ, "Connection Request"},
	{ISUP_PARM_CUG_INTERLOCK_CODE, "Interlock Code", cug_interlock_code_dump, cug_interlock_code_receive, cug_interlock_code_transmit},
	{ISUP_PARM_EGRESS_SERV, "Egress Service"},
	{ISUP_PARM_GENERIC_ADDR, "Generic Address", generic_address_dump, generic_address_receive, generic_address_transmit},
	{ISUP_PARM_GENERIC_DIGITS, "Generic Digits", generic_digits_dump, generic_digits_receive, generic_digits_transmit},
	{ISUP_PARM_GENERIC_NAME, "Generic Name", generic_name_dump, generic_name_receive, generic_name_transmit},
	{ISUP_PARM_TRANSIT_NETWORK_SELECTION, "Transit Network Selection", tns_dump, tns_receive, tns_transmit},
	{ISUP_PARM_GENERIC_NOTIFICATION_IND, "Generic Notification Indication", generic_notofication_ind_dump, generic_notofication_ind_receive, generic_notofication_ind_transmit},
	{ISUP_PARM_PROPAGATION_DELAY, "Propagation Delay Counter", propagation_delay_cntr_dump},
	{ISUP_PARM_HOP_COUNTER, "Hop Counter", hop_counter_dump, hop_counter_receive, hop_counter_transmit},
	{ISUP_PARM_BACKWARD_CALL_IND, "Backward Call Indicator", backward_call_ind_dump, backward_call_ind_receive, backward_call_ind_transmit},
	{ISUP_PARM_OPT_BACKWARD_CALL_IND, "Optional Backward Call Indicator", opt_backward_call_ind_dump, opt_backward_call_ind_receive, NULL},
	{ISUP_PARM_CIRCUIT_GROUP_SUPERVISION_IND, "Circuit Group Supervision Indicator", circuit_group_supervision_dump, circuit_group_supervision_receive, circuit_group_supervision_transmit},
	{ISUP_PARM_RANGE_AND_STATUS, "Range and status", range_and_status_dump, range_and_status_receive, range_and_status_transmit},
	{ISUP_PARM_EVENT_INFO, "Event Information", event_info_dump, event_info_receive, event_info_transmit},
	{ISUP_PARM_OPT_FORWARD_CALL_INDICATOR, "Optional forward call indicator", opt_forward_call_ind_dump, opt_forward_call_ind_receive, opt_forward_call_ind_transmit},
	{ISUP_PARM_LOCATION_NUMBER, "Location Number"},
	{ISUP_PARM_ORIG_LINE_INFO, "Originating line information", originating_line_information_dump, originating_line_information_receive, originating_line_information_transmit},
	{ISUP_PARM_REDIRECTION_INFO, "Redirection Information", redirection_info_dump, redirection_info_receive, redirection_info_transmit},
	{ISUP_PARM_ORIGINAL_CALLED_NUM, "Original called number", original_called_num_dump, original_called_num_receive, original_called_num_transmit},
	{ISUP_PARM_JIP, "Jurisdiction Information Parameter", jip_dump, jip_receive, jip_transmit},
	{ISUP_PARM_ECHO_CONTROL_INFO, "Echo Control Information", echo_control_info_dump, NULL, NULL},
	{ISUP_PARM_PARAMETER_COMPAT_INFO, "Parameter Compatibility Information", parameter_compat_info_dump, NULL, NULL},
	{ISUP_PARM_CIRCUIT_STATE_IND, "Circuit State Indicator", circuit_state_ind_dump, NULL, circuit_state_ind_transmit},
	{ISUP_PARM_LOCAL_SERVICE_PROVIDER_IDENTIFICATION, "Local Service Provider ID", lspi_dump, lspi_receive, lspi_transmit},
	{ISUP_PARM_FACILITY_IND, "Facility Indicator", facility_ind_dump, facility_ind_receive, facility_ind_transmit},
	{ISUP_PARM_REDIRECTING_NUMBER, "Redirecting Number", redirecting_number_dump, redirecting_number_receive, redirecting_number_transmit},
	{ISUP_PARM_ACCESS_DELIVERY_INFO, "Access Delivery Information", },
	{ISUP_PARM_REDIRECT_COUNTER, "Redirect Counter", redirect_counter_dump, redirect_counter_receive, redirect_counter_transmit},
	{ISUP_PARM_SUSRES_IND, "SUS/RES Indicator", susres_ind_dump, susres_ind_receive, susres_ind_transmit},
	{ISUP_PARM_INR_IND, "Information Request Indicators", inr_ind_dump, inr_ind_receive, inr_ind_transmit},
	{ISUP_PARM_INF_IND, "Information Indicators", inf_ind_dump, inf_ind_receive, inf_ind_transmit},
	{ISUP_PARM_SUBSEQUENT_NUMBER, "Subsequent Number", subs_num_dump, subs_num_receive, subs_num_transmit},
	{ISUP_CONNECTED_NUMBER, "Connected Number", connected_num_dump, connected_num_receive, connected_num_transmit}
};

static char * param2str(int parm)
{
	int x;
	int totalparms = sizeof(parms)/sizeof(struct parm_func);
	for (x = 0; x < totalparms; x++)
		if (parms[x].parm == parm)
			return parms[x].name;

	return "Unknown";
}

static void init_isup_call(struct isup_call *c)
{
	int x;
	for (x = 0; x < ISUP_MAX_TIMERS; x++)
		c->timer[x] = -1;
	c->oli_ani2 = -1;
	c->range = 0;
	c->got_sent_msg = 0;
	c->calling_party_cat = 0x0a; /* Default to Ordinary calling subscriber */
}

static struct isup_call * __isup_new_call(struct ss7 *ss7, int nolink)
{
	struct isup_call *c, *cur;
	c = calloc(1, sizeof(struct isup_call));
	if (!c)
		return NULL;

	init_isup_call(c);

	if (nolink)
		return c;
	else {
		cur = ss7->calls;
		if (cur) {
			while (cur->next)
				cur = cur->next;
			cur->next = c;
		} else
			ss7->calls = c;

		return c;
	}
}

struct isup_call * isup_new_call(struct ss7 *ss7)
{
	return __isup_new_call(ss7, 0);
}

void isup_set_call_dpc(struct isup_call *c, unsigned int dpc)
{
	c->dpc = dpc;
}

void isup_set_called(struct isup_call *c, const char *called, unsigned char called_nai, const struct ss7 *ss7)
{
	if (called && called[0]) {
		if (ss7->switchtype == SS7_ITU)
			snprintf(c->called_party_num, sizeof(c->called_party_num), "%s#", called);
		else
			snprintf(c->called_party_num, sizeof(c->called_party_num), "%s", called);
		c->called_nai = called_nai;
	}
}

void isup_set_oli(struct isup_call *c, int oli_ani2)
{
	c->oli_ani2 = oli_ani2;
}

void isup_set_calling(struct isup_call *c, const char *calling, unsigned char calling_nai, unsigned char presentation_ind, unsigned char screening_ind)
{
	if ((calling && calling[0]) || presentation_ind == SS7_PRESENTATION_ADDR_NOT_AVAILABLE) {
		if (calling)
			strncpy(c->calling_party_num, calling, sizeof(c->calling_party_num));
		c->calling_nai = calling_nai;
		c->presentation_ind = presentation_ind;
		c->screening_ind = screening_ind;
	}
}

void isup_set_connected(struct isup_call *c, const char *connected, unsigned char connected_nai, unsigned char connected_presentation_ind, unsigned char connected_screening_ind) {
	if ((connected && connected[0]) || connected_presentation_ind == SS7_PRESENTATION_ADDR_NOT_AVAILABLE) {
		if (connected)
			strncpy(c->connected_num, connected, sizeof(c->connected_num));
		c->connected_nai = connected_nai;
		c->connected_presentation_ind = connected_presentation_ind;
		c->connected_screening_ind = connected_screening_ind;
	}
}

void isup_set_redirecting_number(struct isup_call *c, const char *redirecting_number, unsigned char redirecting_num_nai, unsigned char redirecting_num_presentation_ind, unsigned char redirecting_num_screening_ind)
{
	if (redirecting_number && redirecting_number[0]) {
		strncpy(c->redirecting_num, redirecting_number, sizeof(c->redirecting_num));
		c->redirecting_num_nai = redirecting_num_nai;
		c->redirecting_num_presentation_ind = redirecting_num_presentation_ind;
		c->redirecting_num_screening_ind = redirecting_num_screening_ind;
	}
}

void isup_set_redirectiong_info(struct isup_call *c, unsigned char redirect_info_ind, unsigned char redirect_info_orig_reas,
	unsigned char redirect_info_counter, unsigned char redirect_info_reas)
{
	c->redirect_info = 1;
	c->redirect_info_ind = redirect_info_ind;
	c->redirect_info_orig_reas = redirect_info_orig_reas;
	c->redirect_info_counter = redirect_info_counter;
	c->redirect_info_reas = redirect_info_reas;
}

void isup_set_redirect_counter(struct isup_call *c, unsigned char redirect_counter)
{
	c->redirect_counter = redirect_counter;
}

void isup_set_orig_called_num(struct isup_call *c, const char *orig_called_num, unsigned char orig_called_nai, unsigned char orig_called_pres_ind, unsigned char orig_called_screening_ind)
{
	if (orig_called_num && orig_called_num[0]) {
		strncpy(c->orig_called_num, orig_called_num, sizeof(c->orig_called_num));
		c->orig_called_nai = orig_called_nai;
		c->orig_called_pres_ind = orig_called_pres_ind;
		c->orig_called_screening_ind = orig_called_screening_ind;
	}
}

void isup_set_col_req(struct isup_call *c)
{
	c->col_req = 1;
}

void isup_set_cug(struct isup_call *c, unsigned char cug_indicator, const char *cug_interlock_ni, unsigned short cug_interlock_code)
{
	c->cug_indicator = cug_indicator;
	strncpy(c->cug_interlock_ni, cug_interlock_ni, sizeof(c->cug_interlock_ni));
	c->cug_interlock_code = cug_interlock_code;
}

void isup_set_echocontrol(struct isup_call *c, unsigned char ec)
{
	c->local_echocontrol_ind = ec;
}

void isup_set_charge(struct isup_call *c, const char *charge, unsigned char charge_nai, unsigned char charge_num_plan)
{
	if (charge && charge[0]) {
		strncpy(c->charge_number, charge, sizeof(c->charge_number));
		c->charge_nai = charge_nai;
		c->charge_num_plan = charge_num_plan;
	}
}

void isup_set_gen_address(struct isup_call *c, const char *gen_number, unsigned char gen_add_nai, unsigned char gen_pres_ind, unsigned char gen_num_plan, unsigned char gen_add_type)
{
	if (gen_number && gen_number[0]) {
		strncpy(c->gen_add_number, gen_number, sizeof(c->gen_add_number));
		c->gen_add_nai = gen_add_nai;
		c->gen_add_pres_ind = gen_pres_ind;
		c->gen_add_num_plan = gen_num_plan;
		c->gen_add_type = gen_add_type;
	}
}

void isup_set_gen_digits(struct isup_call *c, const char *gen_number, unsigned char gen_dig_type, unsigned char gen_dig_scheme)
{
	if (gen_number && gen_number[0]) {
		strncpy(c->gen_dig_number, gen_number, sizeof(c->gen_dig_number));
		c->gen_dig_type = gen_dig_type;
		c->gen_dig_scheme = gen_dig_scheme;
	}
}

void isup_set_generic_name(struct isup_call *c, const char *generic_name, unsigned int typeofname, unsigned int availability, unsigned int presentation)
{
        if (generic_name && generic_name[0]) {
		strncpy(c->generic_name, generic_name, sizeof(c->generic_name));
		/* Terminate this just in case */
		c->generic_name[ISUP_MAX_NAME - 1] = '\0';
		c->generic_name_typeofname = typeofname;
		c->generic_name_avail = availability;
		c->generic_name_presentation = presentation;
	}
}

void isup_set_jip_digits(struct isup_call *c, const char *jip_number)
{
	if (jip_number && jip_number[0]) {
		strncpy(c->jip_number, jip_number, sizeof(c->jip_number));
	}
}

void isup_set_lspi(struct isup_call *c, const char *lspi_ident, unsigned char lspi_type, unsigned char lspi_scheme, unsigned char lspi_context)
{
	if (lspi_ident && lspi_ident[0]) {
		strncpy(c->lspi_ident, lspi_ident, sizeof(c->lspi_ident));
		c->lspi_context = lspi_context;
		c->lspi_scheme = lspi_scheme;
		c->lspi_type = lspi_type;
	}
}

void isup_set_callref(struct isup_call *c, unsigned int call_ref_ident, unsigned int call_ref_pc)
{
	c->call_ref_ident = call_ref_ident;
	c->call_ref_pc = call_ref_pc;
}

void isup_set_calling_party_category(struct isup_call *c, unsigned int category)
{
        c->calling_party_cat = category;
}

void isup_init_call(struct ss7 *ss7, struct isup_call *c, int cic, unsigned int dpc)
{
	c->cic = cic;
	c->dpc = dpc;
	if (ss7->switchtype == SS7_ANSI)
		c->sls = ansi_sls_next(ss7);
	else
		c->sls = cic & 0xf;
}

static struct isup_call * isup_find_call(struct ss7 *ss7, struct routing_label *rl, int cic)
{
	struct isup_call *cur, *winner = NULL;

	cur = ss7->calls;
	while (cur) {
		if ((cur->cic == cic) && (cur->dpc == rl->opc)) {
			winner = cur;
			break;
		}
		cur = cur->next;
	}

	if (!winner) {
		winner = __isup_new_call(ss7, 0);
		winner->cic = cic;
		winner->dpc = rl->opc;
		winner->sls = rl->sls;
	}

	return winner;
}

void isup_free_call(struct ss7 *ss7, struct isup_call *c)
{
	struct isup_call *cur, *prev = NULL, *winner = NULL;
	
	if (!ss7 || !c)
		return;

	cur = ss7->calls;
	while (cur) {
		if (cur == c) {
			winner = cur;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if (winner) {
		if (!prev)
			ss7->calls = winner->next;
		else
			prev->next = winner->next;

		isup_stop_all_timers(ss7, c);
		free(c);
	} else
		ss7_error(ss7, "Requested free an unlinked call!!!\n");


	return;
}

static int do_parm(struct ss7 *ss7, struct isup_call *c, int message, int parm, unsigned char *parmbuf, int maxlen, int parmtype, int tx)
{
	struct isup_parm_opt *optparm = NULL;
	int x;
	int res = 0;
	int totalparms = sizeof(parms)/sizeof(struct parm_func);

	for (x = 0; x < totalparms; x++) {
		if (parms[x].parm == parm) {
			if ((tx && parms[x].transmit) || (!tx && parms[x].receive)) {
				switch (parmtype) {
					case PARM_TYPE_FIXED:
						if (tx)
							return parms[x].transmit(ss7, c, message, parmbuf, maxlen);
						else
							return parms[x].receive(ss7, c, message, parmbuf, maxlen);
					case PARM_TYPE_VARIABLE:
						if (tx) {
							res = parms[x].transmit(ss7, c, message, parmbuf + 1, maxlen);
							if (res > 0) {
								parmbuf[0] = res;
								return res + 1;
							}
							return res;
						} else {
							parms[x].receive(ss7, c, message, parmbuf + 1, parmbuf[0]);
							return 1 + parmbuf[0];
						}

					case PARM_TYPE_OPTIONAL:
						optparm = (struct isup_parm_opt *)parmbuf;
						if (tx) {
							optparm->type = parms[x].parm;
							res = parms[x].transmit(ss7, c, message, optparm->data, maxlen);
							if (res > 0) {
								optparm->len = res;
							} else
								return res;
						} else
							res = parms[x].receive(ss7, c, message, optparm->data, optparm->len);
						return res + 2;
				}
			}
		}
	}
	return -1;
}

static int dump_parm(struct ss7 *ss7, int message, int parm, unsigned char *parmbuf, int maxlen, int parmtype)
{
	struct isup_parm_opt *optparm = NULL;
	int x;
	int len = 0;
	int totalparms = sizeof(parms)/sizeof(struct parm_func);
	char *parmname = "Unknown Parm";

	for (x = 0; x < totalparms; x++) {
		if (parms[x].parm == parm) {
			if (parms[x].name)
				parmname = parms[x].name;

			ss7_message(ss7, "\t\t%s:\n", parms[x].name ? parms[x].name : "Unknown");

			if (parms[x].dump) {
				switch (parmtype) {
					case PARM_TYPE_FIXED:
						len = parms[x].dump(ss7, message, parmbuf, maxlen);
						break;
					case PARM_TYPE_VARIABLE:
						parms[x].dump(ss7, message, parmbuf + 1, parmbuf[0]);
						len = 1 + parmbuf[0];
						break;
					case PARM_TYPE_OPTIONAL:
						optparm = (struct isup_parm_opt *)parmbuf;
						parms[x].dump(ss7, message, optparm->data, optparm->len);
						len = 2 + optparm->len;
						break;
				}

			} else {
				switch (parmtype) {
					case PARM_TYPE_VARIABLE:
						len = parmbuf[0] + 1;
						break;
					case PARM_TYPE_OPTIONAL:
						optparm = (struct isup_parm_opt *)parmbuf;
						len = optparm->len + 2;
						break;
				}
			}

			ss7_dump_buf(ss7, 3, parmbuf, len);
			return len;
		}
	}

	/* This is if we don't find it....  */
	optparm = (struct isup_parm_opt *)parmbuf;
	ss7_message(ss7, "\t\tUnknown Parameter (0x%x):\n", optparm->type);
	ss7_dump_buf(ss7, 3, optparm->data, optparm->len);
	return optparm->len + 2;
}

static int isup_send_message(struct ss7 *ss7, struct isup_call *c, int messagetype, int parms[])
{
	struct ss7_msg *msg;
	struct isup_h *mh = NULL;
	unsigned char *rlptr;
	int ourmessage = -1;
	int rlsize;
	unsigned char *varoffsets = NULL, *opt_ptr;
	int fixedparams = 0, varparams = 0, optparams = 0;
	int len = sizeof(struct ss7_msg);
	struct routing_label rl;
	int res = 0;
	int offset = 0;
	int x = 0;
	int i = 0;
	int priority = -1;

	/* Do init stuff */
	msg = ss7_msg_new();

	if (!msg) {
		ss7_error(ss7, "Allocation failed!\n");
		return -1;
	}

	rlptr = ss7_msg_userpart(msg);


	rl.opc = ss7->pc;
	/* use CIC's DPC instead of linkset's DPC */
	rl.dpc = c->dpc;
	rl.sls = c->sls;
	rl.type = ss7->switchtype;
	rlsize = set_routinglabel(rlptr, &rl);
	mh = (struct isup_h *)(rlptr + rlsize); /* Note to self, do NOT put a typecasted pointer next to an addition operation */

	/* Set the CIC - ITU style */
	if (ss7->switchtype == SS7_ITU) {
		mh->cic[0] = c->cic & 0xff;
		mh->cic[1] = (c->cic >> 8) & 0x0f;
	} else {
		mh->cic[0] = c->cic & 0xff;
		mh->cic[1] = (c->cic >> 8) & 0x03f;
	}

	mh->type = messagetype;
	/* Find the metadata for our message */
	for (x = 0; x < sizeof(messages)/sizeof(struct message_data); x++)
		if (messages[x].messagetype == messagetype)
			ourmessage = x;

	if (ourmessage < 0) {
		ss7_error(ss7, "Unable to find message %d in message list!\n", mh->type);
		return -1;
	}

	fixedparams = messages[ourmessage].mand_fixed_params;
	varparams = messages[ourmessage].mand_var_params;
	optparams = messages[ourmessage].opt_params;
	priority = messages[ourmessage].ansi_priority;

	/* Again, the ANSI exception */
	if (ss7->switchtype == SS7_ANSI) {
		if (messages[ourmessage].messagetype == ISUP_IAM) {
			fixedparams = 3;
			varparams = 2;
		} else if (messages[ourmessage].messagetype == ISUP_RLC) {
			optparams = 0;
		} else if (messages[ourmessage].messagetype == ISUP_GRS) {
			optparams = 1;
		} else if (messages[ourmessage].messagetype == ISUP_GRA) {
			optparams = 1;
		}
	}

	/* Add fixed params */
	for (x = 0; x < fixedparams; x++) {
		res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_FIXED, 1);

		if (res < 0) {
			ss7_error(ss7, "!! Unable to add mandatory fixed parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}

	varoffsets = &mh->data[offset];
	/* Make sure we grab our opional parameters */
	if (optparams) {
		opt_ptr = &mh->data[offset + varparams];
		offset += varparams + 1; /* add one for the optionals */
		len -= varparams + 1;
	} else {
		offset += varparams;
		len -= varparams;
	}

	/* Whew, some complicated math for all of these offsets and different sections */
	for (; (x - fixedparams) < varparams; x++) {
		varoffsets[i] = &mh->data[offset] - &varoffsets[i];
		i++;
		res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_VARIABLE, 1);

		if (res < 0) {
			ss7_error(ss7, "!! Unable to add mandatory variable parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}
       	/* Optional parameters */
	if (optparams) {
		int addedparms = 0;
		int offsetbegins = offset;
		while (parms[x] > -1) {
			res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_OPTIONAL, 1);
			x++;

			if (res < 0) {
				ss7_error(ss7, "!! Unable to add optional parameter '%s'\n", param2str(parms[x]));
				return -1;
			}

			if (res > 0)
				addedparms++;

			len -= res;
			offset += res;
		}

		if (addedparms) {
			*opt_ptr = &mh->data[offsetbegins] - opt_ptr;
			/* Add end of optional parameters */
			mh->data[offset++] = 0;
		} else
			*opt_ptr = 0;

	}

	ss7_msg_userpart_len(msg, offset + rlsize + CIC_SIZE + 1);   /* Message type length is 1 */

	return mtp3_transmit(ss7, SIG_ISUP, rl, priority, msg);
}

int isup_dump(struct ss7 *ss7, struct mtp2 *link, unsigned char *buf, int len)
{
	struct isup_h *mh;
	unsigned short cic;
	int ourmessage = -1;
	int *parms = NULL;
	int offset = 0;
	int fixedparams = 0, varparams = 0, optparams = 0;
	int res, x;
	unsigned char *param_pointer = NULL;

	mh = (struct isup_h*) buf;

	len -= 3; /* ISUP msg header size !*/
	
	if (ss7->switchtype == SS7_ITU) {
		cic = mh->cic[0] | ((mh->cic[1] & 0x0f) << 8);
	} else {
		cic = mh->cic[0] | ((mh->cic[1] & 0x3f) << 8);
	}

	ss7_message(ss7, "\t\tCIC: %d\n", cic);
	ss7_dump_buf(ss7, 2, buf, 2);
	ss7_message(ss7, "\t\tMessage Type: %s\n", message2str(mh->type), mh->type & 0xff);
	ss7_dump_buf(ss7, 2, &buf[2], 1);

	/* Find us in the message list */
	for (x = 0; x < sizeof(messages)/sizeof(struct message_data); x++)
		if (messages[x].messagetype == mh->type)
			ourmessage = x;

	if (ourmessage < 0) {
		ss7_error(ss7, "!! Unable to handle message of type 0x%x\n", mh->type);
		return -1;
	}

	fixedparams = messages[ourmessage].mand_fixed_params;
	varparams = messages[ourmessage].mand_var_params;
	parms = messages[ourmessage].param_list;
	optparams = messages[ourmessage].opt_params;

	if (ss7->switchtype == SS7_ANSI) {
		/* Check for the ANSI IAM exception */
		if (messages[ourmessage].messagetype == ISUP_IAM) {
			/* Stupid ANSI SS7, they just had to be different, didn't they? */
			fixedparams = 3;
			varparams = 2;
			parms = ansi_iam_params;
		} else if (messages[ourmessage].messagetype == ISUP_RLC) {
			optparams = 0;
		}
	}

	if (fixedparams)
		ss7_message(ss7, "\t\t--FIXED LENGTH PARMS[%d]--\n", fixedparams);

	/* Parse fixed parms */
	for (x = 0; x < fixedparams; x++) {
		res = dump_parm(ss7, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_FIXED);

		if (res < 0) {
			ss7_error(ss7, "!! Unable to parse mandatory fixed parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}

	if (len < 1)
		return 0;

	if (varparams || optparams) {
		param_pointer = &mh->data[offset];
	} else
		return 0;

	x = 0;
	if (varparams) {
		ss7_message(ss7, "\t\t--VARIABLE LENGTH PARMS[%d]--\n", varparams); 
		
		for (; x < varparams && len > 0; x++) {
			if (!param_pointer[0])
				return 0;
			res = dump_parm(ss7, mh->type, parms[fixedparams + x], 
					(void *)(param_pointer + param_pointer[0]), len, PARM_TYPE_VARIABLE);
		
			if (res < 0) {
				ss7_error(ss7, "!! Unable to parse mandatory variable parameter '%s'\n", param2str(parms[fixedparams + x]));
				return -1;
			}
			len -= (res + 1); /* 1byte for pointer */
			param_pointer++;
			offset++;
		}
	}
	
	if (len < 1 || !param_pointer[0])
		return 0;

	/* Optional paramter parsing code */
	if (optparams) {
			
		ss7_message(ss7, "\t\t--OPTIONAL PARMS--\n");
		offset += param_pointer[0];
		len-- ; /* optional parameter pointer */
		
		while ((len > 0) && (mh->data[offset] != 0)) {
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(mh->data + offset);

			res = dump_parm(ss7, mh->type, optparm->type, (void *)(mh->data + offset), optparm->len, PARM_TYPE_OPTIONAL);

			if (res < 0) {
#if 0
				ss7_message(ss7, "Unhandled optional parameter 0x%x '%s'\n", optparm->type, param2str(optparm->type));
				isup_dump_buffer(ss7, optparm->data, optparm->len);
#endif
				res = optparm->len + 2;
			}

			len -= res;
			offset += res;
		}
	}

	return 0;
}

/* Checking we whether got more 1 bits back in the status */
static int isup_check_status(unsigned char *sent_status, unsigned char *got_status, int range)
{
	int i;
	for (i = 0; i <= range; i++)
		if (got_status[i] > sent_status[i])
			return 1;

	return 0;
}

int isup_needs_masquerade(struct ss7 *ss7, struct routing_label *rl, unsigned int cic, unsigned char *buf, int len);

static int isup_handle_unexpected(struct ss7 *ss7, struct isup_call *c, unsigned int opc)
{
	int res;

	if (c->got_sent_msg & (ISUP_CALL_CONNECTED)) {
		ss7_message(ss7, "ignoring... \n");
	} else {
		ss7_message(ss7, "reseting the cic\n");
		res = ss7_hangup(ss7, c->cic, opc, 16, SS7_HANGUP_SEND_RSC);
		if (res == SS7_CIC_IDLE)
			isup_rsc(ss7, c);
		else if (res == SS7_CIC_NOT_EXISTS)
			isup_free_call(ss7, c);
	}
		return 0;
}

int isup_receive(struct ss7 *ss7, struct mtp2 *link, struct routing_label *rl, unsigned char *buf, int len)
{
	unsigned short cic;
	struct isup_h *mh;
	struct isup_call *c;
	int i;
	int *parms = NULL;
	int offset = 0;
	int ourmessage = -1;
	int fixedparams = 0, varparams = 0, optparams = 0;
	int res, x;
	unsigned char *param_pointer = NULL;
	unsigned int opc = rl->opc;
	ss7_event *e;


	mh = (struct isup_h*) buf;
	if (ss7->switchtype == SS7_ITU) {
		cic = mh->cic[0] | ((mh->cic[1] & 0x0f) << 8);
	} else {
		cic = mh->cic[0] | ((mh->cic[1] & 0x3f) << 8);
	}

	if (isup_needs_masquerade(ss7, rl, cic, buf, len))
		return 0;

	len -= 3; /* ISUP msg header size !*/
	 
	/* Find us in the message list */
	for (x = 0; x < sizeof(messages)/sizeof(struct message_data); x++)
		if (messages[x].messagetype == mh->type)
			ourmessage = x;


	if (ourmessage < 0) {
		ss7_error(ss7, "!! Unable to handle message of type 0x%x on CIC %d\n", mh->type, cic);
		return -1;
	}

	fixedparams = messages[ourmessage].mand_fixed_params;
	varparams = messages[ourmessage].mand_var_params;
	parms = messages[ourmessage].param_list;
	optparams = messages[ourmessage].opt_params;

	if (ss7->switchtype == SS7_ANSI) {
		/* Check for the ANSI IAM exception */
		if (messages[ourmessage].messagetype == ISUP_IAM) {
			/* Stupid ANSI SS7, they just had to be different, didn't they? */
			fixedparams = 3;
			varparams = 2;
			parms = ansi_iam_params;
		} else if (messages[ourmessage].messagetype == ISUP_RLC) {
			optparams = 0;
		} else if (messages[ourmessage].messagetype == ISUP_GRS) {
			optparams = 1;
		} else if (messages[ourmessage].messagetype == ISUP_GRA) {
			optparams = 1;
		}
	}

	c = isup_find_call(ss7, rl, cic);

	if (!c) {
		ss7_error(ss7, "Huh? No call!!!???\n");
		return -1;
	}

	/* Parse fixed parms */
	for (x = 0; x < fixedparams; x++) {
		res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_FIXED, 0);

		if (res < 0) {
			ss7_error(ss7, "!! Unable to parse mandatory fixed parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}

	if (varparams || optparams)
		param_pointer = &mh->data[offset];

	if (varparams && len > 0) {
		for (x = 0; x < varparams && len && param_pointer[0]; x++) {
			res = do_parm(ss7, c, mh->type, parms[fixedparams + x], 
					(void *)(param_pointer + param_pointer[0]), len, PARM_TYPE_VARIABLE, 0);

			if (res < 0) {
				ss7_error(ss7, "!! Unable to parse mandatory variable parameter '%s'\n", param2str(parms[fixedparams + x]));
				return -1;
			}

			len -= (res + 1); /* 1byte for pointer */
			param_pointer++;
			offset++;
		}
	}

	/* Optional paramter parsing code */
	if (optparams && len > 0 && param_pointer[0]) {
		offset += param_pointer[0];
		len-- ; /* optional parameter pointer */

		while ((len > 0) && (mh->data[offset] != 0)) {
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(mh->data + offset);

			res = do_parm(ss7, c, mh->type, optparm->type, mh->data + offset, len, PARM_TYPE_OPTIONAL, 0);

			if (res < 0) {
				ss7_message(ss7, "Unhandled optional parameter 0x%x '%s'\n", optparm->type, param2str(optparm->type));
				isup_dump_buffer(ss7, optparm->data, optparm->len);
				res = optparm->len + 2;
			}

			len -= res;
			offset += res;
		}
	}

	switch (mh->type) {
		case ISUP_IAM:
			return isup_event_iam(ss7, c, opc);
		case ISUP_SAM:
			if (c->got_sent_msg != ISUP_GOT_IAM) {
				ss7_message(ss7, "Got unexpected SAM  on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_SAM;
			e->sam.cic = c->cic;
			e->sam.call = c;
			e->sam.opc = opc; /* keep OPC information */
			strncpy(e->sam.called_party_num, c->called_party_num, sizeof(e->sam.called_party_num));
			e->sam.called_nai = c->called_nai;
			e->sam.got_sent_msg = c->got_sent_msg;
			e->sam.cot_check_passed = c->cot_check_passed;
			e->sam.cot_check_required = c->cot_check_required;
			e->sam.cot_performed_on_previous_cic = c->cot_performed_on_previous_cic;
			isup_start_timer(ss7, c, ISUP_TIMER_T35);
			return 0;
		case ISUP_INF:
			if (!(c->got_sent_msg & ISUP_SENT_INR) && !(c->inf_ind[0] & 0x80)) {
				ss7_message(ss7, "Got solicated INF but we didn't send INR on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			if ((ss7->flags & SS7_INR_IF_NO_CALLING) && 
				!c->calling_party_num[0] && 
				c->presentation_ind != SS7_PRESENTATION_ADDR_NOT_AVAILABLE) {
				isup_rel(ss7, c, 16);
				return 0;
			}

			if (c->inf_ind[0] & 0x80) {
				/* unsolicated */
				if (!isup_free_call_if_clear(ss7, c))
					ss7_call_null(ss7, c, 1);
				return 0;
			} else {
				isup_stop_timer(ss7, c, ISUP_TIMER_T33);
				c->got_sent_msg &= ~ISUP_SENT_INR;
				return isup_event_iam(ss7, c, opc);
			}
		case ISUP_INR:
			if (!(c->got_sent_msg & ISUP_SENT_IAM)) {
				ss7_message(ss7, "Got INF but no outgoing call on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			int inf_parm0 = 0x20; /* Calling party category included, solicated */
			if (!c->calling_party_num[0])
				inf_parm0 |= 0x1; /* Calling party address not available */
			else
				inf_parm0 |= 0x3; /* Calling party address included */
			isup_inf(ss7, c, inf_parm0, 0);
			if (!isup_free_call_if_clear(ss7, c))
				ss7_call_null(ss7, c, 1);
			return 0;
		case ISUP_CQM:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_CQM;
			e->cqm.startcic = cic;
			e->cqm.endcic = cic + c->range;
			e->cqm.opc = opc; /* keep OPC information */
			e->cqm.call = c;
			return 0;
		case ISUP_GRS:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_GRS;
			e->grs.startcic = cic;
			e->grs.endcic = cic + c->range;
			e->grs.call = c;
			e->grs.opc = opc; /* keep OPC information */
			return 0;
		case ISUP_GRA:
			if (!(c->got_sent_msg & ISUP_SENT_GRS)) {
				ss7_message(ss7, "Got GRA but we didn't send GRS on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			/* checking the answer */
			if (c->range != c->sent_grs_endcic - c->cic) {
				ss7_message(ss7, "Got GRA doesn't match with the sent GRS on CIC %d DPC %d\n", c->cic, opc);
				return 0;
			}
	
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_GRA;
			e->gra.startcic = cic;
			e->gra.endcic = cic + c->range;
			for (i = 0; i < (c->range + 1); i++)
				e->gra.status[i] = c->status[i];
			e->gra.opc = opc; /* keep OPC information */
			e->gra.call = c;
			e->gra.sent_endcic = c->sent_grs_endcic;
			e->gra.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg &= ~ISUP_SENT_GRS;
			isup_stop_timer(ss7, c, ISUP_TIMER_T22);
			isup_stop_timer(ss7, c, ISUP_TIMER_T23);
			return 0;
		case ISUP_RSC:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}
			isup_stop_all_timers(ss7, c);
			e->e = ISUP_EVENT_RSC;
			e->rsc.cic = cic;
			e->rsc.call = c;
			e->rsc.opc = opc; /* keep OPC information */
			e->rsc.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg = 0;
			return 0;
		case ISUP_REL:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			isup_stop_timer(ss7, c, ISUP_TIMER_T7);
			isup_stop_timer(ss7, c, ISUP_TIMER_T2);
			isup_stop_timer(ss7, c, ISUP_TIMER_T6);
			isup_stop_timer(ss7, c, ISUP_TIMER_T35);
			c->got_sent_msg &= ~(ISUP_CALL_CONNECTED | ISUP_SENT_IAM | ISUP_GOT_IAM | ISUP_GOT_CCR | ISUP_SENT_INR);
			e->e = ISUP_EVENT_REL;
			e->rel.cic = c->cic;
			e->rel.call = c;
			e->rel.cause = c->cause;
			e->rel.opc = opc; /* keep OPC information */
			e->rel.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_ACM:
			if (!(c->got_sent_msg & ISUP_SENT_IAM)) {
				ss7_message(ss7, "Got ACM but we didn't send IAM on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			isup_stop_timer(ss7, c, ISUP_TIMER_T7);
			c->got_sent_msg |= ISUP_GOT_ACM;
			e->e = ISUP_EVENT_ACM;
			e->acm.cic = c->cic;
			e->acm.call_ref_ident = c->call_ref_ident;
			e->acm.call_ref_pc = c->call_ref_pc;
			e->acm.call = c;
			e->acm.opc = opc; /* keep OPC information */
			e->acm.called_party_status_ind = c->called_party_status_ind;
			e->acm.echocontrol_ind = c->echocontrol_ind;
			e->acm.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_CON:
			if (!(c->got_sent_msg & ISUP_SENT_IAM)) {
				ss7_message(ss7, "Got CON but we didn't send IAM on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			isup_stop_timer(ss7, c, ISUP_TIMER_T7);
			c->got_sent_msg |= ISUP_GOT_CON;
			e->e = ISUP_EVENT_CON;
			e->con.cic = c->cic;
			e->con.call = c;
			e->con.opc = opc; /* keep OPC information */
			e->con.connected_nai = c->connected_nai;
			e->con.connected_presentation_ind = c->connected_presentation_ind;
			e->con.connected_screening_ind = c->connected_screening_ind;
			strncpy(e->con.connected_num, c->connected_num, sizeof(e->con.connected_num));
			e->con.echocontrol_ind = c->echocontrol_ind;
			e->con.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_ANM:
			if (!(c->got_sent_msg & ISUP_SENT_IAM)) {
				ss7_message(ss7, "Got ANM but we didn't send IAM on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			c->got_sent_msg |= ISUP_GOT_ANM;
			e->e = ISUP_EVENT_ANM;
			e->anm.cic = c->cic;
			e->anm.call = c;
			e->anm.opc = opc; /* keep OPC information */
			e->anm.connected_nai = c->connected_nai;
			e->anm.connected_presentation_ind = c->connected_presentation_ind;
			e->anm.connected_screening_ind = c->connected_screening_ind;
			strncpy(e->anm.connected_num, c->connected_num, sizeof(e->anm.connected_num));
			e->anm.got_sent_msg = c->got_sent_msg;
			e->anm.echocontrol_ind = c->echocontrol_ind;
			return 0;
		case ISUP_RLC:
			if (!(c->got_sent_msg & (ISUP_SENT_REL | ISUP_SENT_RSC))) {
				ss7_message(ss7, "Got RLC but we didn't send REL/RSC on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_RLC;
			e->rlc.cic = c->cic;
			e->rlc.opc = opc; /* keep OPC information */
			e->rlc.call = c;
			e->rlc.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg &= ~(ISUP_SENT_REL | ISUP_SENT_RSC);
			isup_stop_timer(ss7, c, ISUP_TIMER_T1);
			isup_stop_timer(ss7, c, ISUP_TIMER_T5);
			isup_stop_timer(ss7, c, ISUP_TIMER_T16);
			isup_stop_timer(ss7, c, ISUP_TIMER_T17);
			return 0;
		case ISUP_COT:
			/* Got we CCR or CCR in IAM ?*/
			if (!(c->got_sent_msg & ISUP_GOT_CCR) & !c->cot_performed_on_previous_cic) {
				ss7_message(ss7, "Got COT but we didn't got CCR previously on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_COT;
			e->cot.cic = c->cic;
			e->cot.passed = c->cot_check_passed;
			e->cot.cot_performed_on_previous_cic = c->cot_performed_on_previous_cic;
			if (!c->cot_check_passed)
				c->got_sent_msg &= ~ISUP_GOT_IAM; /* we will get a new IAM but we are kepping the call */
			e->cot.call = c;
			e->cot.opc = opc; /* keep OPC information */
			e->cot.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg &= ~ISUP_GOT_CCR;
			return 0;
		case ISUP_CCR:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}
			c->got_sent_msg |= ISUP_GOT_CCR;
			e->e = ISUP_EVENT_CCR;
			e->ccr.cic = c->cic;
			e->ccr.opc = opc; /* keep OPC information */
			e->ccr.call = c;
			e->ccr.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_CVT:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_CVT;
			e->cvt.cic = c->cic;
			e->cvt.call = c;
			return 0;
		case ISUP_BLO:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_BLO;
			e->blo.cic = c->cic;
			e->blo.opc = opc; /* keep OPC information */
			e->blo.call = c;
			e->blo.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_UBL:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_UBL;
			e->ubl.cic = c->cic;
			e->ubl.opc = opc; /* keep OPC information */
			e->ubl.call = c;
			return 0;
		case ISUP_BLA:
			if (!(c->got_sent_msg & ISUP_SENT_BLO)) {
				ss7_message(ss7, "Got BLA but we didn't send BLO on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			isup_stop_timer(ss7, c, ISUP_TIMER_T12);
			isup_stop_timer(ss7, c, ISUP_TIMER_T13);

			e->e = ISUP_EVENT_BLA;
			e->bla.cic = c->cic;
			e->bla.opc = opc; /* keep OPC information */
			e->bla.call = c;
			e->bla.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg &= ~ISUP_SENT_BLO;			
			return 0;
		case ISUP_LPA:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_LPA;
			e->lpa.cic = c->cic;
			e->lpa.opc = opc; /* keep OPC information */
			e->lpa.call = c;
			return 0;
		case ISUP_UBA:
			if (!(c->got_sent_msg & ISUP_SENT_UBL)) {
				ss7_message(ss7, "Got UBA but we didn't send UBL on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			isup_stop_timer(ss7, c, ISUP_TIMER_T14);
			isup_stop_timer(ss7, c, ISUP_TIMER_T15);


			e->e = ISUP_EVENT_UBA;
			e->uba.cic = c->cic;
			e->uba.opc = opc; /* keep OPC information */
			e->uba.call = c;
			e->uba.got_sent_msg = c->got_sent_msg;
			c->got_sent_msg &= ~ISUP_SENT_UBL;
			return 0;
		case ISUP_CGB:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_CGB;
			e->cgb.startcic = cic;
			e->cgb.endcic = cic + c->range;
			e->cgb.type = c->cicgroupsupervisiontype;

			for (i = 0; i < (c->range + 1); i++)
				e->cgb.status[i] = c->status[i];
			e->cgb.opc = opc; /* keep OPC information */
			e->cgb.call = c;
			return 0;
		case ISUP_CGU:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_CGU;
			e->cgu.startcic = cic;
			e->cgu.endcic = cic + c->range;
			e->cgu.type = c->cicgroupsupervisiontype;

			for (i = 0; i < (c->range + 1); i++)
				e->cgu.status[i] = c->status[i];
			e->cgu.opc = opc; /* keep OPC information */
			e->cgu.call = c;
			return 0;
		case ISUP_CPG:
			if (!(c->got_sent_msg & ISUP_SENT_IAM)) {
				ss7_message(ss7, "Got CPG but we didn't send IAM on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_CPG;
			e->cpg.cic = c->cic;
			e->cpg.opc = opc; /* keep OPC information */
			e->cpg.event = c->event_info;
			e->cpg.call = c;
			e->cpg.got_sent_msg = c->got_sent_msg;
			e->cpg.echocontrol_ind = c->echocontrol_ind;
			return 0;
		case ISUP_UCIC:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_UCIC;
			e->ucic.cic = c->cic;
			e->ucic.opc = opc; /* keep OPC information */
			e->ucic.call = c;
			return 0;
		case ISUP_FAA:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_FAA;
			e->faa.cic = c->cic;
			e->faa.call_ref_ident = c->call_ref_ident;
			e->faa.call_ref_pc = c->call_ref_pc;
			e->faa.opc = opc; /* keep OPC information */
			e->faa.call = c;
			return 0;
		case ISUP_FAR:
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			e->e = ISUP_EVENT_FAR;
			e->far.cic = c->cic;
			e->far.call_ref_ident = c->call_ref_ident;
			e->far.call_ref_pc = c->call_ref_pc;
			e->ucic.opc = opc; /* keep OPC information */
			e->far.call = c;
			return 0;
		case ISUP_CGBA:
			if (!(c->got_sent_msg & ISUP_SENT_CGB)) {
				ss7_message(ss7, "Got CGBA but we didn't send CGB on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			/* checking the answer */
			if (c->range != c->sent_cgb_endcic - c->cic || c->cicgroupsupervisiontype != c->sent_cgb_type ||
				isup_check_status(c->sent_cgb_status, c->status, c->range)) {
				ss7_message(ss7, "Got CGBA doesn't match with the sent CGB on CIC %d DPC %d\n", c->cic, opc);
				return 0;
			}
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}
			
			e->e = ISUP_EVENT_CGBA;
			e->cgba.startcic = c->cic;
			e->cgba.endcic = c->cic + c->range;
			e->cgba.sent_endcic = c->sent_cgb_endcic;
			e->cgba.type = c->cicgroupsupervisiontype;
			e->cgba.sent_type = c->sent_cgb_type;
			for (i = 0; i < (c->range + 1); i++) {
				e->cgba.status[i] = c->status[i];
				e->cgba.sent_status[i] = c->sent_cgb_status[i];
			}
			e->cgba.got_sent_msg = c->got_sent_msg;
			e->cgba.opc = opc;
			e->cgba.call = c;
			isup_clear_callflags(ss7, c, ISUP_SENT_CGB);
			return 0;
		case ISUP_CGUA:
			if (!(c->got_sent_msg & ISUP_SENT_CGU)) {
				ss7_message(ss7, "Got CGUA but we didn't send CGU on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			/* checking the answer */
			if (c->range != c->sent_cgu_endcic - c->cic || c->cicgroupsupervisiontype != c->sent_cgu_type ||
				isup_check_status(c->sent_cgu_status, c->status, c->range)) {
				ss7_message(ss7, "Got CGUA doesn't match with the sent CGU on CIC %d DPC %d\n", c->cic, opc);
				return 0;
			}

			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}
			
			e->e = ISUP_EVENT_CGUA;
			e->cgua.startcic = c->cic;
			e->cgua.endcic = c->cic + c->range;
			e->cgua.sent_endcic = c->sent_cgu_endcic;
			e->cgua.type = c->cicgroupsupervisiontype;
			e->cgua.sent_type = c->sent_cgu_type;
			for (i = 0; i < (c->range + 1); i++) {
				e->cgua.status[i] = c->status[i];
				e->cgua.sent_status[i] = c->sent_cgu_status[i];
			}
			e->cgua.got_sent_msg = c->got_sent_msg;
			e->cgua.opc = opc;
			e->cgua.call = c;
			isup_clear_callflags(ss7, c, ISUP_SENT_CGU);
			return 0;
		case ISUP_SUS:
			if (!(c->got_sent_msg & (ISUP_GOT_IAM | ISUP_SENT_IAM))) {
				ss7_message(ss7, "Got SUS but no call on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			if (c->susres_ind)
				isup_start_timer(ss7, c, ISUP_TIMER_T6); /* network */
			else
				isup_start_timer(ss7, c, ISUP_TIMER_T2);

			e->e = ISUP_EVENT_SUS;
			e->susres.cic = c->cic;
			e->susres.opc = opc; /* keep OPC information */
			e->susres.call = c;
			e->susres.susres_ind = c->susres_ind;
			e->susres.got_sent_msg = c->got_sent_msg;
			return 0;
		case ISUP_RES:
			if (!(c->got_sent_msg & (ISUP_GOT_IAM | ISUP_SENT_IAM))) {
				ss7_message(ss7, "Got RES but no call on CIC %d PC %d ", c->cic, opc);
				return isup_handle_unexpected(ss7, c, opc);
			}
			e = ss7_next_empty_event(ss7);
			if (!e) {
				ss7_call_null(ss7, c, 1);
				isup_free_call(ss7, c);
				return -1;
			}

			if (c->susres_ind)
				isup_stop_timer(ss7, c, ISUP_TIMER_T6); /* network */
			else
				isup_stop_timer(ss7, c, ISUP_TIMER_T2);

			e->e = ISUP_EVENT_RES;
			e->susres.cic = c->cic;
			e->susres.opc = opc; /* keep OPC information */
			e->susres.call = c;
			e->susres.susres_ind = c->susres_ind;
			e->susres.got_sent_msg = c->got_sent_msg;
			return 0;
		default:
			if (!isup_free_call_if_clear(ss7, c))
				ss7_call_null(ss7, c, 1);
			return 0;
	}
}

int isup_event_iam(struct ss7 *ss7, struct isup_call *c, int opc)
{
	ss7_event *e;

	/* Checking dual seizure */
	if (c->got_sent_msg == ISUP_SENT_IAM) {
		if ((ss7->pc > opc) ? (~c->cic & 1) : (c->cic & 1)) {
			ss7_message(ss7, "Dual seizure on CIC %d DPC %d we are the controlling, ignore IAM\n", c->cic, opc);
			return 0;
		} else {
			ss7_message(ss7, "Dual seizure on CIC %d DPC %d they are the controlling, hangup our call\n", c->cic, opc);
			c->got_sent_msg = ISUP_GOT_IAM;
			ss7_hangup(ss7, c->cic, opc, SS7_CAUSE_TRY_AGAIN, SS7_HANGUP_REEVENT_IAM);
			return 0;
		}
	}

	c->got_sent_msg |= ISUP_GOT_IAM;

	if ((ss7->flags & SS7_INR_IF_NO_CALLING) && 
		!c->calling_party_num[0] && c->presentation_ind != SS7_PRESENTATION_ADDR_NOT_AVAILABLE) {
		c->dpc = opc;
		isup_inr(ss7, c, 0x1, 0); /* Calling party address requested */
		return 0;
	}

	e = ss7_next_empty_event(ss7);
	if (!e) {
		ss7_call_null(ss7, c, 1);
		isup_free_call(ss7, c);
		return -1;
	}

	if (c->cot_check_required)
		c->got_sent_msg |= ISUP_GOT_CCR;

	e->e = ISUP_EVENT_IAM;
	e->iam.got_sent_msg = c->got_sent_msg;
	e->iam.cic = c->cic;
	e->iam.transcap = c->transcap;
	e->iam.cot_check_required = c->cot_check_required;
	e->iam.cot_performed_on_previous_cic = c->cot_performed_on_previous_cic;
	c->cot_check_passed = 0;
	strncpy(e->iam.called_party_num, c->called_party_num, sizeof(e->iam.called_party_num));
	e->iam.called_nai = c->called_nai;
	strncpy(e->iam.calling_party_num, c->calling_party_num, sizeof(e->iam.calling_party_num));
	e->iam.calling_nai = c->calling_nai;
	e->iam.presentation_ind = c->presentation_ind;
	e->iam.screening_ind = c->screening_ind;
	strncpy(e->iam.charge_number, c->charge_number, sizeof(e->iam.charge_number));
	e->iam.charge_nai = c->charge_nai;
	e->iam.charge_num_plan = c->charge_num_plan;
	e->iam.oli_ani2 = c->oli_ani2;
	e->iam.gen_add_nai = c->gen_add_nai;
	e->iam.gen_add_num_plan = c->gen_add_num_plan;
	strncpy(e->iam.gen_add_number, c->gen_add_number, sizeof(e->iam.gen_add_number));
	e->iam.gen_add_pres_ind = c->gen_add_pres_ind;
	e->iam.gen_add_type = c->gen_add_type;
	strncpy(e->iam.gen_dig_number, c->gen_dig_number, sizeof(e->iam.gen_dig_number));
	e->iam.gen_dig_type = c->gen_dig_type;
	e->iam.gen_dig_scheme = c->gen_dig_scheme;
	strncpy(e->iam.jip_number, c->jip_number, sizeof(e->iam.jip_number));
	strncpy(e->iam.generic_name, c->generic_name, sizeof(e->iam.generic_name));
	e->iam.generic_name_typeofname = c->generic_name_typeofname;
	e->iam.generic_name_avail = c->generic_name_avail;
	e->iam.generic_name_presentation = c->generic_name_presentation;
	e->iam.lspi_type = c->lspi_type;
	e->iam.lspi_scheme = c->lspi_scheme;
	e->iam.lspi_context = c->lspi_context;
	strncpy(e->iam.lspi_ident, c->lspi_ident, sizeof(e->iam.lspi_ident));
	strncpy(e->iam.orig_called_num, c->orig_called_num, sizeof(e->iam.orig_called_num));
	e->iam.orig_called_nai = c->orig_called_nai;
	e->iam.orig_called_pres_ind = c->orig_called_pres_ind;
	e->iam.orig_called_screening_ind = c->orig_called_screening_ind;
	strncpy(e->iam.redirecting_num, c->redirecting_num, sizeof(e->iam.redirecting_num));
	e->iam.redirecting_num_nai = c->redirecting_num_nai;
	e->iam.redirecting_num_presentation_ind = c->redirecting_num_presentation_ind;
	e->iam.redirecting_num_screening_ind = c->redirecting_num_screening_ind;
	e->iam.calling_party_cat = c->calling_party_cat;
	e->iam.redirect_counter = c->redirect_counter;
	e->iam.redirect_info = c->redirect_info;
	e->iam.redirect_info_ind = c->redirect_info_ind;
	e->iam.redirect_info_orig_reas = c->redirect_info_orig_reas;
	e->iam.redirect_info_counter = c->redirect_info_counter;
	e->iam.redirect_info_reas = c->redirect_info_reas;
	e->iam.cug_indicator = c->cug_indicator;
	e->iam.cug_interlock_code = c->cug_interlock_code;
	strncpy(e->iam.cug_interlock_ni, c->cug_interlock_ni, sizeof(e->iam.cug_interlock_ni));
	e->iam.call = c;
	e->iam.opc = opc; /* keep OPC information */
	e->iam.echocontrol_ind = c->echocontrol_ind;
	if (!strchr(c->called_party_num, '#'))
		isup_start_timer(ss7, c, ISUP_TIMER_T35);
	return 0;
}

int isup_cqr(struct ss7 *ss7, int begincic, int endcic, unsigned int dpc, unsigned char status[])
{
	struct isup_call call;
	int i, res;

	for (i = 0; (i + begincic) <= endcic; i++)
		call.status[i] = status[i];

	call.cic = begincic;
	call.range = endcic - begincic;
	call.dpc = dpc;

	if (call.range > 31)
		return -1;

	res = isup_send_message(ss7, &call, ISUP_CQR, cqr_params);

	if (res == -1) {
		ss7_error(ss7, "Unable to send CQR to DPC: %d\n", dpc);
	}

	return res;
	
}

int isup_grs(struct ss7 *ss7, struct isup_call *c, int endcic)
{
	int res;

	if (!ss7 || !c)
		return -1;
	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;

	res = isup_send_message(ss7, c, ISUP_GRS, greset_params);

	if (ss7->switchtype == SS7_ANSI) {
		/* ANSI require that we send it twice.  Don't think I understand completely why.
		 * T1.113 in 2.9.3.2 */
		res = isup_send_message(ss7, c, ISUP_GRS, greset_params);
	}

	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_GRS;
		c->sent_grs_endcic = endcic;
		isup_stop_all_timers(ss7, c);
		isup_start_timer(ss7, c, ISUP_TIMER_T22);
		isup_start_timer(ss7, c, ISUP_TIMER_T23);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send GRS to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_gra(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[])
{
	int i, res;

	if (!ss7 || !c)
		return -1;

	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;

	for (i = 0; (i + c->cic) <= endcic; i++)
		c->status[i] = state[i];

	res = isup_send_message(ss7, c, ISUP_GRA, greset_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send GRA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_cgb(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[], int type)
{
	int i, res;
	if (!ss7 || !c)
		return -1;
	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;
	c->sent_cgb_endcic = endcic;
	c->cicgroupsupervisiontype = type;
	c->sent_cgb_type = type;

	for (i = 0; (i + c->cic) <= endcic; i++) {
		c->status[i] = state[i];
		c->sent_cgb_status[i] = state[i];
	}

	res = isup_send_message(ss7, c, ISUP_CGB, cicgroup_params);
	
	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_CGB;
		c->sent_cgb_type = type;
		isup_start_timer(ss7, c, ISUP_TIMER_T18);
		isup_start_timer(ss7, c, ISUP_TIMER_T19);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CGB to DPC: %d\n", c->dpc);
	}
	return res;
}

int isup_cgu(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[], int type)
{
	int i, res;
	if (!ss7 || !c)
		return -1;
	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;
	c->sent_cgu_endcic = endcic;
	c->cicgroupsupervisiontype = type;
	c->sent_cgu_type = type;

	for (i = 0; (i + c->cic) <= endcic; i++) {
		c->status[i] = state[i];
		c->sent_cgu_status[i] = state[i];
	}

	isup_start_timer(ss7, c, ISUP_TIMER_T20);
	isup_start_timer(ss7, c, ISUP_TIMER_T21);
	res = isup_send_message(ss7, c, ISUP_CGU, cicgroup_params);

	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_CGU;
		isup_start_timer(ss7, c, ISUP_TIMER_T20);
		isup_start_timer(ss7, c, ISUP_TIMER_T21);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CGU to DPC: %d\n", c->dpc);
	}
	return res;
}

int isup_cgba(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[])
{
	int i, res;

	if (!ss7 || !c)
		return -1;
	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;

	for (i = 0; (i + c->cic) <= endcic; i++) {
		c->status[i] = state[i];
	}
	
	res = isup_send_message(ss7, c, ISUP_CGBA, cicgroup_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CGBA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_cgua(struct ss7 *ss7, struct isup_call *c, int endcic, unsigned char state[])
{
	int i, res;

	if (!ss7 || !c)
		return -1;
	if (endcic - c->cic > 31)
		return -1;

	c->range = endcic - c->cic;

	for (i = 0; (i + c->cic) <= endcic; i++) {
		c->status[i] = state[i];
	}
	
	res = isup_send_message(ss7, c, ISUP_CGUA, cicgroup_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CGUA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_iam(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	if (ss7->switchtype == SS7_ITU)
		res = isup_send_message(ss7, c, ISUP_IAM, iam_params);
	else
		res = isup_send_message(ss7, c, ISUP_IAM, ansi_iam_params);

	if (res > -1) {
		isup_start_timer(ss7, c, ISUP_TIMER_T7);
		c->got_sent_msg |= ISUP_SENT_IAM;
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send IAM to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_acm(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_ACM, acm_params);

	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_ACM;
		isup_stop_timer(ss7, c, ISUP_TIMER_T35);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send ACM to DPC: %d\n", c->dpc);
	}
	
	return res;
}

int isup_faa(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;
	
	res = isup_send_message(ss7, c, ISUP_FAA, faa_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send FAA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_far(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;
	
	if (c->next && c->next->call_ref_ident) {
		c->call_ref_ident = c->next->call_ref_ident;
		c->call_ref_pc = c->next->call_ref_pc;
		c->got_sent_msg |= ISUP_SENT_FAR;
		res = isup_send_message(ss7, c, ISUP_FAR, far_params);
	}
	return -1;

	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_FAR;
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send FAR to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_anm(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_ANM, anm_params);

	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_ANM;
		isup_stop_timer(ss7, c, ISUP_TIMER_T35);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send ANM to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_con(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	c->got_sent_msg |= ISUP_SENT_CON;
	isup_stop_timer(ss7, c, ISUP_TIMER_T35);
	res = isup_send_message(ss7, c, ISUP_CON, con_params);

	if ( res > -1) {
		c->got_sent_msg |= ISUP_SENT_CON;
		isup_stop_timer(ss7, c, ISUP_TIMER_T35);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CON to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_rel(struct ss7 *ss7, struct isup_call *c, int cause)
{
	int res;
	if (!ss7 || !c)
		return -1;

	if (cause < 0)
		cause = 16;

	c->cause = cause;
	c->causecode = CODE_CCITT;
	c->causeloc = LOC_PRIV_NET_LOCAL_USER;

	res = isup_send_message(ss7, c, ISUP_REL, rel_params);

	if (res > -1) {
		isup_stop_timer(ss7, c, ISUP_TIMER_T7);
		isup_stop_timer(ss7, c, ISUP_TIMER_T2);
		isup_stop_timer(ss7, c, ISUP_TIMER_T6);
		isup_stop_timer(ss7, c, ISUP_TIMER_T35);
		isup_start_timer(ss7, c, ISUP_TIMER_T1);
		isup_start_timer(ss7, c, ISUP_TIMER_T5);

		c->got_sent_msg |= ISUP_SENT_REL;
		c->got_sent_msg &= ~(ISUP_SENT_IAM | ISUP_CALL_CONNECTED | ISUP_GOT_IAM | ISUP_GOT_CCR | ISUP_SENT_INR);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send REL to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_rlc(struct ss7 *ss7, struct isup_call *c)
{
	int res;

	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_RLC, empty_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send RLC to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_inr(struct ss7 *ss7, struct isup_call *c, unsigned char ind0, unsigned char ind1)
{
	int res;
	if (!ss7 || !c)
		return -1;

	c->inr_ind[0] = ind0;
	c->inr_ind[1] = ind1;

	res = isup_send_message(ss7, c, ISUP_INR, inr_params);
	
	if (res > -1) {
		c->got_sent_msg |= ISUP_SENT_INR;
		isup_start_timer(ss7, c, ISUP_TIMER_T33);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send INR to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_inf(struct ss7 *ss7, struct isup_call *c, unsigned char ind0, unsigned char ind1)
{
	int res;
	if (!ss7 || !c)
		return -1;

	c->inf_ind[0] = ind0;
	c->inf_ind[1] = ind1;

	res = isup_send_message(ss7, c, ISUP_INF, inf_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send INF to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_sus(struct ss7 *ss7, struct isup_call *c, unsigned char indicator)
{
	int res;

	if (!ss7 || !c)
		return -1;

	c->susres_ind = indicator;
	res = isup_send_message(ss7, c, ISUP_SUS, susres_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send SUS to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_res(struct ss7 *ss7, struct isup_call *c, unsigned char indicator)
{
	int res;

	if (!ss7 || !c)
		return -1;

	c->susres_ind = indicator;
	res = isup_send_message(ss7, c, ISUP_RES, susres_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send RES to DPC: %d\n", c->dpc);
	}

	return res;
}

static int isup_send_message_ciconly(struct ss7 *ss7, int messagetype, int cic, unsigned int dpc)
{
	int res;
	struct isup_call c;
	c.cic = cic;
	c.dpc = dpc;
	res = isup_send_message(ss7, &c, messagetype, empty_params);
	return res;
}

int isup_cpg(struct ss7 *ss7, struct isup_call *c, int event)
{
	int res;
	if (!ss7 || !c)
		return -1;

	c->event_info = event;

	res = isup_send_message(ss7, c, ISUP_CPG, cpg_params);

	if (res > -1) {
		isup_stop_timer(ss7, c, ISUP_TIMER_T35);
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send CPG to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_rsc(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_RSC, empty_params);	
	
	if (res > -1) {
		isup_stop_all_timers(ss7, c);
		isup_start_timer(ss7, c, ISUP_TIMER_T17);
		c->got_sent_msg = ISUP_SENT_RSC;
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send RSC to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_blo(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_BLO, empty_params);

	if (res > -1) {
		isup_start_timer(ss7, c, ISUP_TIMER_T12);
		isup_start_timer(ss7, c, ISUP_TIMER_T13);
		c->got_sent_msg |= ISUP_SENT_BLO;
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send BLO to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_ubl(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_UBL, empty_params);

	if (res > -1) {
		isup_start_timer(ss7, c, ISUP_TIMER_T14);
		isup_start_timer(ss7, c, ISUP_TIMER_T15);
		c->got_sent_msg |= ISUP_SENT_UBL;
	} else {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send UBL to DPC: %d\n", c->dpc);
	}

	return res;

}

int isup_bla(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 || !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_BLA, empty_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send BLA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_lpa(struct ss7 *ss7, int cic, unsigned int dpc)
{
	int res;
	if (!ss7)
		return -1;

	res = isup_send_message_ciconly(ss7, ISUP_LPA, cic, dpc);

	if (res == -1) {
		ss7_error(ss7, "Unable to send LPA to DPC: %d\n", dpc);
	}

	return res;
}


int isup_ucic(struct ss7 *ss7, int cic, unsigned int dpc)
{
	int res;
	if (!ss7)
		return -1;

	res = isup_send_message_ciconly(ss7, ISUP_UCIC, cic, dpc);

	if (res == -1) {
		ss7_error(ss7, "Unable to send UCIC to DPC: %d\n", dpc);
	}

	return res;
}

int isup_uba(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	if (!ss7 && !c)
		return -1;

	res = isup_send_message(ss7, c, ISUP_UBA, empty_params);

	if (res == -1) {
		ss7_call_null(ss7, c, 0);
		isup_free_call(ss7, c);
		ss7_error(ss7, "Unable to send UBA to DPC: %d\n", c->dpc);
	}

	return res;
}

int isup_cvr(struct ss7 *ss7, int cic, unsigned int dpc)
{
	int res;
	if (!ss7)
		return -1;
	
	res = isup_send_message_ciconly(ss7, ISUP_CVR, cic, dpc);
	if (res == -1) {
		ss7_error(ss7, "Unable to send CVR to DPC: %d\n", dpc);
	}

	return res;
}

int isup_show_calls(struct ss7 *ss7, void (* cust_printf)(int fd, const char *format, ...), int fd)
{
	struct isup_call *c = ss7->calls;
	char buff[256];
	char *p;
	char sent[80];
	char got[80];
	int x;
	cust_printf(fd, "%5s %5s %3s  %-24s  %-16s  %s\n", "  CIC", "  DPC", "SLS", "Sent", "Got", "TIMERS (time left s)"); 	

	while (c) {
		p = sent;
		*p = '\0';
		if (c->got_sent_msg & ISUP_SENT_RSC)
			p += sprintf(p, "RSC ");
		if (c->got_sent_msg & ISUP_SENT_IAM)
			p += sprintf(p, "IAM ");
		if (c->got_sent_msg & ISUP_SENT_ACM)
			p += sprintf(p, "ACM ");
		if (c->got_sent_msg & ISUP_SENT_REL)
			p += sprintf(p, "REL ");
		if (c->got_sent_msg & ISUP_SENT_BLO)
			p += sprintf(p, "BLO ");
		if (c->got_sent_msg & ISUP_SENT_UBL)
			p += sprintf(p, "UBL ");
		if (c->got_sent_msg & ISUP_SENT_GRS)
			p += sprintf(p, "GRS ");
		if (c->got_sent_msg & ISUP_SENT_CGB)
			p += sprintf(p, "CGB ");
		if (c->got_sent_msg & ISUP_SENT_CGU)
			p += sprintf(p, "CGU ");
		if (c->got_sent_msg & ISUP_SENT_CON)
			p += sprintf(p, "CON ");
		if (c->got_sent_msg & ISUP_SENT_ANM)
			p += sprintf(p, "ANM ");
		if (c->got_sent_msg & ISUP_SENT_INR)
			p += sprintf(p, "INR ");

		p = got;
		*p = '\0';
		if (c->got_sent_msg & ISUP_GOT_CCR)
			p += sprintf(p, "CCR ");
		if (c->got_sent_msg & ISUP_GOT_IAM)
			p += sprintf(p, "IAM ");
		if (c->got_sent_msg & ISUP_GOT_ACM)
			p += sprintf(p, "ACM ");
		if (c->got_sent_msg & ISUP_GOT_ANM)
			p += sprintf(p, "ANM ");
		if (c->got_sent_msg & ISUP_GOT_CON)
			p += sprintf(p, "CON ");

		p = buff;
		p += sprintf(p, "%5i %5i %3i  %-24s  %-16s  ", c->cic, c->dpc, c->sls, sent, got);

		for (x = 0; x < ISUP_MAX_TIMERS; x++) {
			if (c->timer[x] > -1) {
				p += isup_timer2str(x, p);
				p--;
				p += sprintf(p, "(%li) ",  ss7->ss7_sched[c->timer[x]].when.tv_sec - time(NULL));
			}
		}
		*p = '\n';
		p++;
		*p = '\0';
		cust_printf(fd, buff);
		p = buff;
		c = c->next;
	}
	return 0;
}

static int isup_timer2str(int timer, char *res)
{
	switch (timer) {
		case ISUP_TIMER_T1:
			strcpy (res, "t1");
			return 3;
		case ISUP_TIMER_T2:
			strcpy (res, "t2");
			return 3;
		case ISUP_TIMER_T5:
			strcpy (res, "t5");
			return 3;
		case ISUP_TIMER_T6:
			strcpy (res, "t6");
			return 3;
		case ISUP_TIMER_T7:
			strcpy (res, "t7");
			return 3;
		case ISUP_TIMER_T12:
			strcpy (res, "t12");
			return 4;
		case ISUP_TIMER_T13:
			strcpy (res, "t13");
			return 4;
		case ISUP_TIMER_T14:
			strcpy (res, "t14");
			return 4;
		case ISUP_TIMER_T15:
			strcpy (res, "t15");
			return 4;
		case ISUP_TIMER_T16:
			strcpy (res, "t16");
			return 4;
		case ISUP_TIMER_T17:
			strcpy (res, "t17");
			return 4;
		case ISUP_TIMER_T18:
			strcpy (res, "t18");
			return 4;
		case ISUP_TIMER_T19:
			strcpy (res, "t19");
			return 4;
		case ISUP_TIMER_T20:
			strcpy (res, "t20");
			return 4;
		case ISUP_TIMER_T21:
			strcpy (res, "t21");
			return 4;
		case ISUP_TIMER_T22:
			strcpy (res, "t22");
			return 4;
		case ISUP_TIMER_T23:
			strcpy (res, "t23");
			return 4;
		case ISUP_TIMER_T33:
			strcpy (res, "t33");
			return 4;
		case ISUP_TIMER_T35:
			strcpy (res, "t35");
			return 4;
		default:
			strcpy (res, "unknown");
		return 8;
	}
}

int ss7_set_isup_timer(struct ss7 *ss7, char *name, int ms)
{
	if (!strcasecmp(name, "t1"))
		ss7->isup_timers[ISUP_TIMER_T1] = ms;
	else if (!strcasecmp(name, "t2"))
		ss7->isup_timers[ISUP_TIMER_T2] = ms;
	else if (!strcasecmp(name, "t5"))
		ss7->isup_timers[ISUP_TIMER_T5] = ms;
	else if (!strcasecmp(name, "t6"))
		ss7->isup_timers[ISUP_TIMER_T6] = ms;
	else if (!strcasecmp(name, "t7"))
		ss7->isup_timers[ISUP_TIMER_T7] = ms;
	else if (!strcasecmp(name, "t12"))
		ss7->isup_timers[ISUP_TIMER_T12] = ms;
	else if (!strcasecmp(name, "t13"))
		ss7->isup_timers[ISUP_TIMER_T13] = ms;
	else if (!strcasecmp(name, "t14"))
		ss7->isup_timers[ISUP_TIMER_T14] = ms;
	else if (!strcasecmp(name, "t15"))
		ss7->isup_timers[ISUP_TIMER_T15] = ms;
	else if (!strcasecmp(name, "t16"))
		ss7->isup_timers[ISUP_TIMER_T16] = ms;
	else if (!strcasecmp(name, "t17"))
		ss7->isup_timers[ISUP_TIMER_T17] = ms;
	else if (!strcasecmp(name, "t18"))
		ss7->isup_timers[ISUP_TIMER_T18] = ms;
	else if (!strcasecmp(name, "t19"))
		ss7->isup_timers[ISUP_TIMER_T19] = ms;
	else if (!strcasecmp(name, "t20"))
		ss7->isup_timers[ISUP_TIMER_T20] = ms;
	else if (!strcasecmp(name, "t21"))
		ss7->isup_timers[ISUP_TIMER_T21] = ms;
	else if (!strcasecmp(name, "t22"))
		ss7->isup_timers[ISUP_TIMER_T22] = ms;
	else if (!strcasecmp(name, "t23"))
		ss7->isup_timers[ISUP_TIMER_T23] = ms;
	else if (!strcasecmp(name, "t33"))
		ss7->isup_timers[ISUP_TIMER_T33] = ms;
	else if (!strcasecmp(name, "t35"))
		ss7->isup_timers[ISUP_TIMER_T35] = ms;
	else {
		ss7_message(ss7, "Unknown ISUP timer: %s\n", name);
		return 0;
	}
	ss7_message(ss7, "ISUP timer %s = %ims\n", name, ms);
	return 1;
}

static void isup_timer_expiry(void *data)
{
	struct isup_timer_param *param = data;
	char buf[16];
	int x;

	isup_timer2str(param->timer, buf);

	if (param->timer == ISUP_TIMER_T5 || param->timer == ISUP_TIMER_T13 || param->timer == ISUP_TIMER_T15 ||
			param->timer == ISUP_TIMER_T17 || param->timer == ISUP_TIMER_T19 || 
			param->timer == ISUP_TIMER_T21 || param->timer == ISUP_TIMER_T21)
		ss7_error(param->ss7, "ISUP timer %s expired on CIC %i DPC %i\n", buf, param->c->cic, param->c->dpc);
	else
		ss7_message(param->ss7, "ISUP timer %s expired on CIC %i DPC %i\n", buf, param->c->cic, param->c->dpc);

	param->c->timer[param->timer] = -1;

	switch (param->timer) {
		case ISUP_TIMER_T1:
			isup_send_message(param->ss7, param->c, ISUP_REL, rel_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T1);
			break;
		case ISUP_TIMER_T16:
			param->c->got_sent_msg = ISUP_SENT_RSC;
			isup_send_message(param->ss7, param->c, ISUP_RSC, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T16);
			break;
		case ISUP_TIMER_T2:
		case ISUP_TIMER_T6:
			ss7_hangup(param->ss7, param->c->cic, param->c->dpc, 16, SS7_HANGUP_SEND_REL);
			break;
		case ISUP_TIMER_T7:
			ss7_hangup(param->ss7, param->c->cic, param->c->dpc, 31, SS7_HANGUP_SEND_REL);
			break;
		case ISUP_TIMER_T5:
			ss7_notinservice(param->ss7, param->c->cic, param->c->dpc);
			/* no break here */
		case ISUP_TIMER_T17:
			isup_stop_all_timers(param->ss7, param->c);
			param->c->got_sent_msg = ISUP_SENT_RSC;
			isup_send_message(param->ss7, param->c, ISUP_RSC, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T17);
			break;
		case ISUP_TIMER_T12:
			isup_send_message(param->ss7, param->c, ISUP_BLO, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T12);
			break;
		case ISUP_TIMER_T13:
			isup_stop_timer(param->ss7, param->c, ISUP_TIMER_T12);
			isup_send_message(param->ss7, param->c, ISUP_BLO, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T13);
			break;
		case ISUP_TIMER_T14:
			isup_send_message(param->ss7, param->c, ISUP_UBL, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T14);
			break;
		case ISUP_TIMER_T15:
			isup_stop_timer(param->ss7, param->c, ISUP_TIMER_T14);
			isup_send_message(param->ss7, param->c, ISUP_UBL, empty_params);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T15);
			break;
		case ISUP_TIMER_T19:
			isup_stop_timer(param->ss7, param->c, ISUP_TIMER_T18);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T19);
			/* no break here */
		case ISUP_TIMER_T18:
			if (param->timer != ISUP_TIMER_T19)
				isup_start_timer(param->ss7, param->c, ISUP_TIMER_T18);
			param->c->range = param->c->sent_cgb_endcic - param->c->cic;
			param->c->cicgroupsupervisiontype = param->c->sent_cgb_type;
			for (x = 0; (x + param->c->cic) <= param->c->sent_cgb_endcic; x++)
				param->c->status[x] = param->c->sent_cgb_status[x];
			isup_send_message(param->ss7, param->c, ISUP_CGB, cicgroup_params);
			break;
		case ISUP_TIMER_T21:
			isup_stop_timer(param->ss7, param->c, ISUP_TIMER_T20);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T21);
			/* no break here */
		case ISUP_TIMER_T20:
			if (param->timer != ISUP_TIMER_T21)
				isup_start_timer(param->ss7, param->c, ISUP_TIMER_T20);
			param->c->range = param->c->sent_cgu_endcic - param->c->cic;
			param->c->cicgroupsupervisiontype = param->c->sent_cgu_type;
			for (x = 0; (x + param->c->cic) <= param->c->sent_cgu_endcic; x++)
				param->c->status[x] = param->c->sent_cgu_status[x];
			isup_send_message(param->ss7, param->c, ISUP_CGU, cicgroup_params);
		case ISUP_TIMER_T23:
			isup_stop_timer(param->ss7, param->c, ISUP_TIMER_T22);
			isup_start_timer(param->ss7, param->c, ISUP_TIMER_T23);
			/* no break here */
		case ISUP_TIMER_T22:
			if (param->timer != ISUP_TIMER_T23)
				isup_start_timer(param->ss7, param->c, ISUP_TIMER_T22);
			param->c->range = param->c->sent_grs_endcic - param->c->cic;
			isup_send_message(param->ss7, param->c, ISUP_GRS, greset_params);
			break;
		case ISUP_TIMER_T33:
			param->c->got_sent_msg &= ~ISUP_SENT_INR;
			isup_rel(param->ss7, param->c, 16);
			break;
		case ISUP_TIMER_T35:
			isup_rel(param->ss7, param->c, 28);
			break;
		default:
			ss7_message (param->ss7, "timer expired, doing nothing\n");
	}
	free(param);
}

static void isup_stop_timer(struct ss7 *ss7, struct isup_call *c, int timer)
{
	char buf[16];
	struct isup_timer_param *param;

	if (!ss7 || !c)
		return;

	if (c->timer[timer] > -1) {
		param = ss7->ss7_sched[c->timer[timer]].data;
		ss7_schedule_del(ss7, &c->timer[timer]);
		free(param);
		c->timer[timer] = -1;
		isup_timer2str(timer, buf);
		ss7_message(ss7, "ISUP timer %s stopped on CIC %i DPC: %i\n", buf, c->cic, c->dpc);
	}
}

static void isup_stop_all_timers(struct ss7 *ss7, struct isup_call *c)
{
	int x;

	if (!ss7 || !c)
		return;

	for (x = 0; x < ISUP_MAX_TIMERS; x++)
		if (c->timer[x] > -1)
			isup_stop_timer(ss7, c, x);
}

static int isup_start_timer(struct ss7 *ss7, struct isup_call *c, int timer)
{
	char buf[16];
	struct isup_timer_param *data;

	if (!ss7 || !c)
		return -1;

	if (!ss7->isup_timers[timer])
		return -1;

	data = calloc(1, sizeof(struct isup_timer_param));

	data->ss7 = ss7;
	data->c = c;
	data->timer = timer;
	
	isup_timer2str(timer, buf);
	if (c->timer[timer] > -1)
		isup_stop_timer(ss7, c, timer);
	c->timer[timer] = ss7_schedule_event(ss7, ss7->isup_timers[timer], &isup_timer_expiry, data);
	if (c->timer[timer] > -1) {
		ss7_message(ss7, "ISUP timer %s (%ims) started on CIC %i DPC %i\n", buf, ss7->isup_timers[timer], c->cic, c->dpc);
		return 0;
	} else
		ss7_error(ss7, "Unable to start ISUP timer %s on CIC %i DPC %i\n", buf, ss7->isup_timers[timer], c->cic, c->dpc);

	return -1;
}

struct isup_call * isup_free_call_if_clear(struct ss7 *ss7, struct isup_call *c)
{
	int x;

	if (!ss7 || !c)
		return NULL;
	
	if (c->got_sent_msg)	
		return c;

	for (x = 0; x < ISUP_MAX_TIMERS; x++)
		if (c->timer[x] > -1)
			return c;

	isup_free_call(ss7, c);
	return NULL;
}

void isup_free_all_calls(struct ss7 *ss7)
{
	while (ss7->calls) {
		ss7_call_null(ss7, ss7->calls, 1);
		isup_free_call(ss7, ss7->calls);
	}
}

void isup_clear_callflags(struct ss7 *ss7, struct isup_call *c, unsigned long flags)
{
	if (!ss7 || !c)
		return;

	c->got_sent_msg &= ~flags;

	if (flags & ISUP_SENT_IAM)
		isup_stop_timer(ss7, c, ISUP_TIMER_T7);

	if (flags & ISUP_SENT_CGB) {
		isup_stop_timer(ss7, c, ISUP_TIMER_T18);
		isup_stop_timer(ss7, c, ISUP_TIMER_T19);
	}

	if (flags & ISUP_SENT_CGU) {
		isup_stop_timer(ss7, c, ISUP_TIMER_T20);
		isup_stop_timer(ss7, c, ISUP_TIMER_T21);
	}
}

/* Janelle is the bomb (Again) */
