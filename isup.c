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

#include <string.h>
#include <stdlib.h>
#include "libss7.h"
#include "isup.h"
#include "ss7_internal.h"
#include "mtp3.h"

#define FUNC_DUMP(name) int ((name))(struct ss7 *ss7, struct isup_call *c, int messagetype, unsigned char *parm, int len)
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

static int iam_params[] = {ISUP_PARM_NATURE_OF_CONNECTION_IND, ISUP_PARM_FORWARD_CALL_IND, ISUP_PARM_CALLING_PARTY_CAT,
	ISUP_PARM_TRANSMISSION_MEDIUM_REQS, ISUP_PARM_CALLED_PARTY_NUM, ISUP_PARM_CALLING_PARTY_NUM, -1}; /* Don't have optional IEs */

static int ansi_iam_params[] = {ISUP_PARM_NATURE_OF_CONNECTION_IND, ISUP_PARM_FORWARD_CALL_IND, ISUP_PARM_CALLING_PARTY_CAT,
	ISUP_PARM_USER_SERVICE_INFO, ISUP_PARM_CALLED_PARTY_NUM, ISUP_PARM_CALLING_PARTY_NUM, -1}; /* Don't have optional IEs */

static int acm_params[] = {ISUP_PARM_BACKWARD_CALL_IND, -1};

static int anm_params[] = { -1};

static int con_params[] = { ISUP_PARM_BACKWARD_CALL_IND, -1};

static int rel_params[] = { ISUP_PARM_CAUSE, -1};

static int greset_params[] = { ISUP_PARM_RANGE_AND_STATUS, -1};

static int cot_params[] = { ISUP_PARM_CONTINUITY_IND, -1};

static int cpg_params[] = { ISUP_PARM_EVENT_INFO, -1};

static int cicgroup_params[] = { ISUP_PARM_CIRCUIT_GROUP_SUPERVISION_IND, ISUP_PARM_RANGE_AND_STATUS, -1};

static int empty_params[] = { -1};

static struct message_data {
	int messagetype;
	int mand_fixed_params;
	int mand_var_params;
	int opt_params;
	int *param_list;
} messages[] = {
	{ISUP_IAM, 4, 1, 1, iam_params},
	{ISUP_ACM, 1, 0, 1, acm_params},
	{ISUP_ANM, 0, 0, 1, anm_params},
	{ISUP_CON, 1, 0, 1, con_params},
	{ISUP_REL, 0, 1, 1, rel_params},
	{ISUP_RLC, 0, 0, 1, empty_params},
	{ISUP_GRS, 0, 1, 0, greset_params},
	{ISUP_GRA, 0, 1, 0, greset_params},
	{ISUP_CGB, 1, 1, 0, cicgroup_params},
	{ISUP_CGU, 1, 1, 0, cicgroup_params},
	{ISUP_CGBA, 1, 1, 0, cicgroup_params},
	{ISUP_CGUA, 1, 1, 0, cicgroup_params},
	{ISUP_COT, 1, 0, 0, cot_params},
	{ISUP_CCR, 0, 0, 0, empty_params},
	{ISUP_BLO, 0, 0, 0, empty_params},
	{ISUP_UBL, 0, 0, 0, empty_params},
	{ISUP_BLA, 0, 0, 0, empty_params},
	{ISUP_UBA, 0, 0, 0, empty_params},
	{ISUP_RSC, 0, 0, 0, empty_params},
	{ISUP_CPG, 1, 0, 1, cpg_params},
};

static int isup_send_message(struct ss7 *ss7, struct isup_call *c, int messagetype, int parms[]);

static char * message2str(unsigned char message)
{
	switch (message) {
		case ISUP_IAM:
			return "IAM";
		case ISUP_ACM:
			return "ACM";
		case ISUP_ANM:
			return "ANM";
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
	for (i = 0; i < ((srclen * 2) - oddeven); i++)
		dest[i] = digit2char(src[i/2] >> ((i % 2) ? 4 : 0));

	dest[i] = '\0'; 
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
	
	return 1; /* Length plus size of type header */
}

static FUNC_RECV(nature_of_connection_ind_receive)
{
	unsigned char cci = (parm[0] >> 2) & 0x3;

	if (cci == 0x1)
		c->cot_check_required = 1;
	else
		c->cot_check_required = 0;

	return 1;
}

static FUNC_DUMP(nature_of_connection_ind_dump)
{
	unsigned char con = parm[0];
	char *continuity;
	
	ss7_message(ss7, "	Satellites in connection: %d\n", con&0x03);
	con>>=2; 
	switch (con & 0x03) {
		case 0:
			continuity = "Check not required";
			break;
		case 1:
			continuity = "Check required on this circuit";
			break;
		case 2:
			continuity = "Check required on previous circuit";
			break;
		case 3:
			continuity = "spare";
			break;
	}
	ss7_message(ss7, "	Continuity Check: %s\n", continuity);
	con>>=2;
	con &= 0x01;

	ss7_message(ss7, "	Outgoing half echo control device %s\n", con ? "included" : "not included");

	return 2;
}


static FUNC_SEND(forward_call_ind_transmit)
{
	parm[0] = 0x60;
	parm[1] = 0x01;
	return 2;
}

static FUNC_RECV(forward_call_ind_receive)
{
	return 2;
}

static FUNC_DUMP(forward_call_ind_dump)
{
	return 2;
}

static FUNC_RECV(calling_party_cat_receive)
{
	return 1;
}
	
static FUNC_SEND(calling_party_cat_transmit)
{
	/* Default to unknown */
	parm[0] = 0x0a;
	return 1;
}

static FUNC_DUMP(calling_party_cat_dump)
{
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
	return 1;
}

/* For variable length parameters we pass in the length of the parameter */
static FUNC_RECV(called_party_num_receive)
{
	int odd = 0;

	if (parm[0] & 0x80)
		odd = 1;
	
	isup_get_number(c->called_party_num, &parm[2], len - 2, odd);

	return len;
}

static FUNC_SEND(called_party_num_transmit)
{
	int numlen, oddeven;

	isup_put_number(&parm[2], c->called_party_num, &numlen, &oddeven);

	parm[0] = 0x03; /* Assume unknown */
	
	if (oddeven)
		parm[0] |= 0x80; /* Odd number of digits */

	parm[1] = (0x1 << 4) | 0x00; /* Assume E.164 numbering plan */

	return numlen + 2;
}

static FUNC_RECV(backward_call_ind_receive)
{
	return 2;
}

static FUNC_SEND(backward_call_ind_transmit)
{
	parm[0] = 0x40;
	parm[1] = 0x14;
	return 2;
}

static FUNC_DUMP(backward_call_ind_dump)
{
	return 2;
}

static FUNC_RECV(cause_receive)
{
	c->causeloc = parm[0] & 0xf;
	c->causecode = (parm[0] & 0x60) >> 5;
	c->cause = (parm[1] & 0x7f);

	return 2;
}

static FUNC_SEND(cause_transmit)
{
	parm[0] = 0x80 | (c->causecode << 5) | c->causeloc;
	parm[1] = 0x80 | c->cause;
	return 2;
}


static FUNC_DUMP(range_and_status_dump)
{
	ss7_message(ss7, "\tPARM: Range and Status\n");
	ss7_message(ss7, "\t	Range: %d\n", parm[0] & 0xff);
	return len;
}

static FUNC_RECV(range_and_status_receive)
{
	int i;
	int numcics;

	c->range = parm[0];
	numcics = c->range + 1;

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

	if (messagetype == ISUP_GRS)
		return 1;
	
	statuslen = (numcics / 8) + !!(numcics % 8);

	if (messagetype == ISUP_GRA) {
		for (i = 0; i < statuslen; i++) {
			parm[1 + i] = 0;
		}
	} else {
		for (i = 0; i < numcics; i++) {
			if (c->status[i])
				parm[1 + (i/8)] |= (1 << (i % 8));
		}
	}

	return statuslen + 1;
}

static FUNC_DUMP(calling_party_num_dump)
{
	int oddeven = (parm[0] >> 7) & 0x1;
	char numbuf[64] = "";

	ss7_message(ss7, "PARM: Calling Party Number\n");
	ss7_message(ss7, "Odd/even: %x\n", (parm[0] >> 7) & 0x1);
	ss7_message(ss7, "Nature of address: %x\n", parm[0] & 0x7f);
	ss7_message(ss7, "NI: %x\n", (parm[1] >> 7) & 0x1);
	ss7_message(ss7, "Numbering plan: %x\n", (parm[1] >> 4) & 0x7);
	ss7_message(ss7, "Presentation: %x\n", (parm[1] >> 2) & 0x3);
	ss7_message(ss7, "Screening: %x\n", parm[1] & 0x3);

	isup_get_number(numbuf, &parm[2], len - 2, oddeven);

	ss7_message(ss7, "Address signals: %s\n", numbuf);

	return len;
}

static FUNC_RECV(calling_party_num_receive)
{
	int oddeven = (parm[0] >> 7) & 0x1;

	isup_get_number(c->calling_party_num, &parm[2], len - 2, oddeven);

	return len;
}

static FUNC_SEND(calling_party_num_transmit)
{
	int oddeven, datalen;

	if (!c->calling_party_num[0])
		return 0;

	isup_put_number(&parm[2], c->calling_party_num, &datalen, &oddeven);

	parm[0] = (oddeven << 7) | 0x3;
	parm[1] = 0x11;

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
	ss7_message(ss7, "PARM: Continuity Indicator\n");
	ss7_message(ss7, "Continuity Check: %s\n", (0x01 & parm[0]) ? "successful" : "failed");

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
	ss7_message(ss7, "PARM: Circuit Group Supervision Indicator\n");
	ss7_message(ss7, "Type indicator: %s\n", name);

	return 1;
}

static FUNC_RECV(circuit_group_supervision_receive)
{
	return 1;
}

static FUNC_SEND(circuit_group_supervision_transmit)
{
	parm[0] = 0x0;
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
	ss7_message(ss7, "PARM: Event Information:\n");
	ss7_message(ss7, "%s\n", name);
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

static struct parm_func parms[] = {
	{ISUP_PARM_NATURE_OF_CONNECTION_IND, "Nature of Connection Indicator", nature_of_connection_ind_dump, nature_of_connection_ind_receive, nature_of_connection_ind_transmit },
	{ISUP_PARM_FORWARD_CALL_IND, "Forward Call Indicator", forward_call_ind_dump, forward_call_ind_receive, forward_call_ind_transmit },
	{ISUP_PARM_CALLING_PARTY_CAT, "Calling Party Category", calling_party_cat_dump, calling_party_cat_receive, calling_party_cat_transmit},
	{ISUP_PARM_TRANSMISSION_MEDIUM_REQS, "Transmission Medium Requirements", transmission_medium_reqs_dump, transmission_medium_reqs_receive, transmission_medium_reqs_transmit},
	{ISUP_PARM_USER_SERVICE_INFO, "User Service Information", NULL, user_service_info_receive, user_service_info_transmit},
	{ISUP_PARM_CALLED_PARTY_NUM, "Called Party Number", NULL, called_party_num_receive, called_party_num_transmit},
	{ISUP_PARM_CAUSE, "Cause Indicator", NULL, cause_receive, cause_transmit},
	{ISUP_PARM_CONTINUITY_IND, "Continuity Indicator", continuity_ind_dump, continuity_ind_receive, continuity_ind_transmit},
	{ISUP_PARM_ACCESS_TRANS, "Access Transport"},
	{ISUP_PARM_BUSINESS_GRP, "Business Group"},
	{ISUP_PARM_CALL_REF, "Call Reference"},
	{ISUP_PARM_CALLING_PARTY_NUM, "Calling Party Number", calling_party_num_dump, calling_party_num_receive, calling_party_num_transmit},
	{ISUP_PARM_CARRIER_ID, "Carrier Identification"},
	{ISUP_PARM_SELECTION_INFO, "Selection Information"},
	{ISUP_PARM_CHARGE_NUMBER, "Charge Number"},
	{ISUP_PARM_CIRCUIT_ASSIGNMENT_MAP, "Circuit Assignment Map"},
	{ISUP_PARM_CONNECTION_REQ, "Connection Request"},
	{ISUP_PARM_CUG_INTERLOCK_CODE, "Interlock Code"},
	{ISUP_PARM_EGRESS_SERV, "Egress Service"},
	{ISUP_PARM_GENERIC_ADDR, "Generic Address"},
	{ISUP_PARM_GENERIC_DIGITS, "Generic Digits"},
	{ISUP_PARM_GENERIC_NAME, "Generic Name"},
	{ISUP_PARM_GENERIC_NOTIFICATION_IND, "Generic Notification Indication"},
	{ISUP_PARM_PROPAGATION_DELAY, "Propagation Delay"},
	{ISUP_PARM_HOP_COUNTER, "Hop Counter"},
	{ISUP_PARM_BACKWARD_CALL_IND, "Backward Call Indicator", backward_call_ind_dump, backward_call_ind_receive, backward_call_ind_transmit},
	{ISUP_PARM_CIRCUIT_GROUP_SUPERVISION_IND, "Circuit Group Supervision Indicator", circuit_group_supervision_dump, circuit_group_supervision_receive, circuit_group_supervision_transmit},
	{ISUP_PARM_RANGE_AND_STATUS, "Range and status", range_and_status_dump, range_and_status_receive, range_and_status_transmit},
	{ISUP_PARM_EVENT_INFO, "Event Information", event_info_dump, event_info_receive, event_info_transmit},
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

struct isup_call * isup_new_call(struct ss7 *ss7)
{
	struct isup_call *c, *cur;
	c = calloc(1, sizeof(struct isup_call));
	if (!c)
		return NULL;
	cur = ss7->calls;
	if (cur) {
		while (cur->next)
			cur = cur->next;
		cur->next = c;
	} else
		ss7->calls = c;

	return c;
}

void isup_init_call(struct ss7 *ss7, struct isup_call *c, int cic, char *calledpartynum, char *callingpartynum)
{
	c->cic = cic;
	if (calledpartynum && calledpartynum[0]) {
		if (ss7->switchtype == SS7_ITU)
			snprintf(c->called_party_num, sizeof(c->called_party_num), "%s#", calledpartynum);
		else
			snprintf(c->called_party_num, sizeof(c->called_party_num), "%s", calledpartynum);
	}

	if (callingpartynum && callingpartynum[0])
		strncpy(c->calling_party_num, callingpartynum, sizeof(c->calling_party_num));

}

static struct isup_call * isup_find_call(struct ss7 *ss7, int cic)
{
	struct isup_call *cur, *winner = NULL;

	cur = ss7->calls;
	while (cur) {
		if (cur->cic == cic) {
			winner = cur;
			break;
		}
		cur = cur->next;
	}

	if (!winner) {
		winner = isup_new_call(ss7);
		winner->cic = cic;
	}

	return winner;
}

static void isup_free_call(struct ss7 *ss7, struct isup_call *c)
{
	struct isup_call *cur, *prev = NULL, *winner = NULL;

	cur = ss7->calls;
	while (cur) {
		if (cur == c) {
			winner = cur;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if (!prev)
		ss7->calls = winner->next;
	else
		prev->next = winner->next;

	if (winner)
		free(winner);

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
						} else 
							return 1 + parms[x].receive(ss7, c, message, parmbuf + 1, parmbuf[0]);
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

#if 0
static int dump_parm(struct ss7 *ss7, struct isup_call *c, int message, int parm, unsigned char *parmbuf, int maxlen, int parmtype)
{
	struct isup_parm_opt *optparm = NULL;
	int x;
	int res = 0;
	int totalparms = sizeof(parms)/sizeof(struct parm_func);

	for (x = 0; x < totalparms; x++) {
		if (parms[x].parm == parm) {
			ss7_message(ss7, "PARM: %s\n", parms[x].name ? parms[x].name : "Unknown");
			if (parms[x].dump) {
				switch (parmtype) {
					case PARM_TYPE_FIXED:
						return parms[x].dump(ss7, c, message, parmbuf, maxlen);
					case PARM_TYPE_VARIABLE:
						return 1 + parms[x].dump(ss7, c, message, parmbuf + 1, parmbuf[0]);
					case PARM_TYPE_OPTIONAL:
						optparm = (struct isup_parm_opt *)parmbuf;
						res = parms[x].dump(ss7, c, message, optparm->data, optparm->len);
						return res + 2;
				}
			} else {
				optparm = (struct isup_parm_opt *)parmbuf;
				isup_dump_buffer(ss7, optparm->data, optparm->len);
				return optparm->len + 2;
			}
		}
	}

	/* This is if we don't find it.... It's going to be either an unknown message or an unknown optional parameter */
	ss7_message(ss7, "Parm: Unknown");
	optparm = (struct isup_parm_opt *)parmbuf;
	isup_dump_buffer(ss7, optparm->data, optparm->len);
	return optparm->len + 2;
}
#endif

static int isup_send_message(struct ss7 *ss7, struct isup_call *c, int messagetype, int parms[])
{
	struct ss7_msg *msg;
	struct isup_h *mh = NULL;
	unsigned char *rlptr;
	int ourmessage = -1;
	int rlsize;
	unsigned char *varoffsets = NULL, *opt_ptr;
	int fixedparams = 0, varparams = 0;
	int len = sizeof(struct ss7_msg);
	struct routing_label rl;
	int res = 0;
	int offset = 0;
	int x = 0;
	int i = 0;

	/* Do init stuff */
	msg = ss7_msg_new();

	if (!msg) {
		ss7_error(ss7, "Allocation failed!\n");
		return -1;
	}

	rlptr = ss7_msg_userpart(msg);
	rl.opc = ss7->pc;
	rl.sls = sls_next(ss7);
	rl.dpc = ss7->def_dpc;
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

	/* Again, the ANSI exception */
	if (messages[ourmessage].messagetype == ISUP_IAM) {
		if (ss7->switchtype == SS7_ITU) {
			fixedparams = messages[ourmessage].mand_fixed_params;
			varparams = messages[ourmessage].mand_var_params;
		} else {
			/* Stupid ANSI SS7, they just had to be different, didn't they? */
			fixedparams = 3;
			varparams = 2;
		}
	} else {
		fixedparams = messages[ourmessage].mand_fixed_params;
		varparams = messages[ourmessage].mand_var_params;
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
	if (messages[ourmessage].opt_params) {
		opt_ptr = &mh->data[offset + varparams];
		offset += varparams + 1; /* add one for the optionals */
		len -= varparams + 1;
	} else {
		offset += varparams;
		len -= varparams;
	}
	i = 0;

	/* Whew, some complicated math for all of these offsets and different sections */
	for (; (x - fixedparams) < varparams; x++) {
		varoffsets[i] = &mh->data[offset] - &varoffsets[i];
		i++;
		res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_VARIABLE, 1); /* Find out what else we need to add */

		if (res < 0) {
			ss7_error(ss7, "!! Unable to add mandatory variable parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}
       	/* Optional parameters */
	if (messages[ourmessage].opt_params) {
		int addedparms = 0;
		int offsetbegins = offset;
		while (parms[x] > -1) {
			res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_OPTIONAL, 1); /* Find out what else we need to add */
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

	return mtp3_transmit(ss7, SIG_ISUP, rl.sls, msg);
}

int isup_dump(struct ss7 *ss7, struct mtp2 *link, unsigned char *buf, int len)
{
	struct isup_h *mh;
	unsigned short cic;
	int ourmessage = -1;
	int x;

	mh = (struct isup_h*) buf;

	if (ss7->switchtype == SS7_ITU) {
		cic = mh->cic[0] | ((mh->cic[1] << 8) & 0x0f);
	} else {
		cic = mh->cic[0] | ((mh->cic[1] << 8) & 0x3f);
	}
	/* Find us in the message list */
	for (x = 0; x < sizeof(messages)/sizeof(struct message_data); x++)
		if (messages[x].messagetype == mh->type)
			ourmessage = x;

	if (ourmessage < 0) {
		ss7_error(ss7, "!! Unable to handle message of type 0x%x\n", mh->type);
		return -1;
	}

	ss7_message(ss7, "\t\tMessage Type: %s (%x)\n", message2str(mh->type), mh->type & 0xff);
	ss7_message(ss7, "\t\tCIC: %d\n", cic);

	return 0;
}

int isup_receive(struct ss7 *ss7, struct mtp2 *link, unsigned char *buf, int len)
{
	unsigned short cic;
	struct isup_h *mh;
	struct isup_call *c;
	int i;
	int *parms = NULL;
	int offset = 0;
	int ourmessage = -1;
	int fixedparams = 0, varparams = 0;
	int res, x;
	unsigned char *opt_ptr = NULL;
	ss7_event *e;

	mh = (struct isup_h*) buf;
	if (ss7->switchtype == SS7_ITU) {
		cic = mh->cic[0] | ((mh->cic[1] << 8) & 0x0f);
	} else {
		cic = mh->cic[0] | ((mh->cic[1] << 8) & 0x3f);
	}

	/* Find us in the message list */
	for (x = 0; x < sizeof(messages)/sizeof(struct message_data); x++)
		if (messages[x].messagetype == mh->type)
			ourmessage = x;


	if (ourmessage < 0) {
		ss7_error(ss7, "!! Unable to handle message of type 0x%x\n", mh->type);
		return -1;
	}

	c = isup_find_call(ss7, cic);

	/* Check for the ANSI IAM exception */
	if (messages[ourmessage].messagetype == ISUP_IAM) {
		if (ss7->switchtype == SS7_ITU) {
			fixedparams = messages[ourmessage].mand_fixed_params;
			varparams = messages[ourmessage].mand_var_params;
			parms = messages[ourmessage].param_list;
		} else {
			/* Stupid ANSI SS7, they just had to be different, didn't they? */
			fixedparams = 3;
			varparams = 2;
			parms = ansi_iam_params;
		}
	} else {
		fixedparams = messages[ourmessage].mand_fixed_params;
		varparams = messages[ourmessage].mand_var_params;
		parms = messages[ourmessage].param_list;
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

	if (varparams) {
		offset += varparams; /* add one for the optionals */
		res -= varparams;
	}
	if (messages[ourmessage].opt_params) {
		opt_ptr = &mh->data[offset++];
	}

	i = 0;

	for (; (x - fixedparams) < varparams; x++) {
		res = do_parm(ss7, c, mh->type, parms[x], (void *)(mh->data + offset), len, PARM_TYPE_VARIABLE, 0); /* Find out what else we need to add */

		if (res < 0) {
			ss7_error(ss7, "!! Unable to parse mandatory variable parameter '%s'\n", param2str(parms[x]));
			return -1;
		}

		len -= res;
		offset += res;
	}

	/* Optional paramter parsing code */
	if (messages[ourmessage].opt_params && *opt_ptr) {
		while ((len > 0) && (mh->data[offset] != 0)) {
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(mh->data + offset);

			res = do_parm(ss7, c, mh->type, optparm->type, mh->data + offset, len, PARM_TYPE_OPTIONAL, 0); /* Find out what else we need to add */

			if (res < 0) {
				ss7_message(ss7, "Unhandled optional parameter 0x%x '%s'\n", optparm->type, param2str(optparm->type));
				isup_dump_buffer(ss7, optparm->data, optparm->len);
				res = optparm->len + 2;
			}

			len -= res;
			offset += res;
		}
	}

	e = ss7_next_empty_event(ss7);
	if (!e) {
		ss7_error(ss7, "Event queue full, unable to get next empty event\n");
		return -1;
	}

	switch (mh->type) {
		case ISUP_IAM:
			e->e = ISUP_EVENT_IAM;
			e->iam.cic = c->cic;
			e->iam.transcap = c->transcap;
			e->iam.cot_check_required = c->cot_check_required;
			strncpy(e->iam.called_party_num, c->called_party_num, sizeof(e->iam.called_party_num));
			strncpy(e->iam.calling_party_num, c->calling_party_num, sizeof(e->iam.calling_party_num));
			e->iam.call = c;
			return 0;
		case ISUP_GRS:
			e->e = ISUP_EVENT_GRS;
			e->grs.startcic = cic;
			e->grs.endcic = cic + c->range;
			isup_free_call(ss7, c); /* Won't need this again */
			return 0;
		case ISUP_GRA:
			e->e = ISUP_EVENT_GRA;
			e->gra.startcic = cic;
			e->gra.endcic = cic + c->range;
			for (i = 0; i < (c->range + 1); i++)
				e->gra.status[i] = c->status[i];

			isup_free_call(ss7, c); /* Won't need this again */
			return 0;
		case ISUP_RSC:
			e->e = ISUP_EVENT_RSC;
			e->rsc.cic = cic;
			e->rsc.call = c;
			return 0;
		case ISUP_REL:
			e->e = ISUP_EVENT_REL;
			e->rel.cic = c->cic;
			e->rel.call = c;
			return 0;
		case ISUP_ACM:
			e->e = ISUP_EVENT_ACM;
			e->acm.cic = c->cic;
			e->acm.call = c;
			return 0;
		case ISUP_CON:
			e->e = ISUP_EVENT_CON;
			e->con.cic = c->cic;
			e->con.call = c;
			return 0;
		case ISUP_ANM:
			e->e = ISUP_EVENT_ANM;
			e->anm.cic = c->cic;
			e->anm.call = c;
			return 0;
		case ISUP_RLC:
			e->e = ISUP_EVENT_RLC;
			e->rlc.cic = c->cic;
			isup_free_call(ss7, c);
			return 0;
		case ISUP_COT:
			e->e = ISUP_EVENT_COT;
			e->cot.cic = c->cic;
			e->cot.passed = c->cot_check_passed;
			e->cot.call = c;
			return 0;
		case ISUP_CCR:
			e->e = ISUP_EVENT_CCR;
			e->ccr.cic = c->cic;
			return 0;
		case ISUP_BLO:
			e->e = ISUP_EVENT_BLO;
			e->blo.cic = c->cic;
			isup_free_call(ss7, c);
			return 0;
		case ISUP_UBL:
			e->e = ISUP_EVENT_UBL;
			e->ubl.cic = c->cic;
			isup_free_call(ss7, c);
			return 0;
		case ISUP_BLA:
			e->e = ISUP_EVENT_BLA;
			e->bla.cic = c->cic;
			isup_free_call(ss7, c);
			return 0;
		case ISUP_UBA:
			e->e = ISUP_EVENT_UBA;
			e->uba.cic = c->cic;
			isup_free_call(ss7, c);
			return 0;
		case ISUP_CGB:
			e->e = ISUP_EVENT_CGB;
			e->cgb.startcic = cic;
			e->cgb.endcic = cic + c->range;

			for (i = 0; i < (c->range + 1); i++)
				e->cgb.status[i] = c->status[i];

			isup_free_call(ss7, c);
			return 0;
		case ISUP_CGU:
			e->e = ISUP_EVENT_CGU;
			e->cgu.startcic = cic;
			e->cgu.endcic = cic + c->range;

			for (i = 0; i < (c->range + 1); i++)
				e->cgu.status[i] = c->status[i];

			isup_free_call(ss7, c);
			return 0;
		case ISUP_CPG:
			e->e = ISUP_EVENT_CPG;
			e->cpg.cic = c->cic;
			e->cpg.event = c->event_info;
			return 0;
		default:
			ss7_error(ss7, "!! Unable to handle message type %s\n", message2str(mh->type));
			return -1;
	}
}

static int isup_send_cicgroupmessage(struct ss7 *ss7, int messagetype, int begincic, int endcic, unsigned char status[])
{
	struct isup_call call;
	int i;

	for (i = 0; (i + begincic) <= endcic; i++)
		call.status[i] = status[i];

	call.cic = begincic;
	call.range = endcic - begincic;
	return isup_send_message(ss7, &call, messagetype, cicgroup_params);
}

int isup_grs(struct ss7 *ss7, int begincic, int endcic)
{
	struct isup_call call;
	call.cic = begincic;
	call.range = endcic - begincic;
	return isup_send_message(ss7, &call, ISUP_GRS, greset_params);
}

int isup_gra(struct ss7 *ss7, int begincic, int endcic)
{
	struct isup_call call;
	call.cic = begincic;
	call.range = endcic - begincic;
	return isup_send_message(ss7, &call, ISUP_GRA, greset_params);
}

int isup_cgb(struct ss7 *ss7, int begincic, int endcic, unsigned char state[])
{
	return isup_send_cicgroupmessage(ss7, ISUP_CGB, begincic, endcic, state);
}

int isup_cgu(struct ss7 *ss7, int begincic, int endcic, unsigned char state[])
{
	return isup_send_cicgroupmessage(ss7, ISUP_CGU, begincic, endcic, state);
}

int isup_cgba(struct ss7 *ss7, int begincic, int endcic, unsigned char state[])
{
	return isup_send_cicgroupmessage(ss7, ISUP_CGBA, begincic, endcic, state);
}

int isup_cgua(struct ss7 *ss7, int begincic, int endcic, unsigned char state[])
{
	return isup_send_cicgroupmessage(ss7, ISUP_CGUA, begincic, endcic, state);
}

int isup_iam(struct ss7 *ss7, struct isup_call *c)
{
	if (ss7->switchtype == SS7_ITU)
		return isup_send_message(ss7, c, ISUP_IAM, iam_params);
	else
		return isup_send_message(ss7, c, ISUP_IAM, ansi_iam_params);
}

int isup_acm(struct ss7 *ss7, struct isup_call *c)
{
	return isup_send_message(ss7, c, ISUP_ACM, acm_params);
}

int isup_anm(struct ss7 *ss7, struct isup_call *c)
{
	return isup_send_message(ss7, c, ISUP_ANM, anm_params);
}

int isup_con(struct ss7 *ss7, struct isup_call *c)
{
	return isup_send_message(ss7, c, ISUP_CON, con_params);
}

int isup_rel(struct ss7 *ss7, struct isup_call *c, int cause)
{
	if (cause < 0)
		cause = 16;

	c->cause = cause;
	c->causecode = CODE_CCITT;
	c->causeloc = LOC_PRIV_NET_LOCAL_USER;
	return isup_send_message(ss7, c, ISUP_REL, rel_params);
}

int isup_rlc(struct ss7 *ss7, struct isup_call *c)
{
	int res;
	res = isup_send_message(ss7, c, ISUP_RLC, empty_params);
	isup_free_call(ss7, c);
	return res;
}

static int isup_send_message_ciconly(struct ss7 *ss7, int messagetype, int cic)
{
	int res;
	struct isup_call c;
	c.cic = cic;
	res = isup_send_message(ss7, &c, messagetype, empty_params);
	return res;
}

int isup_cpg(struct ss7 *ss7, struct isup_call *c, int event)
{
	c->event_info = event;
	return isup_send_message(ss7, c, ISUP_CPG, cpg_params);
}

int isup_rsc(struct ss7 *ss7, int cic)
{
	return isup_send_message_ciconly(ss7, ISUP_RSC, cic);
}

int isup_blo(struct ss7 *ss7, int cic)
{
	return isup_send_message_ciconly(ss7, ISUP_BLO, cic);
}

int isup_ubl(struct ss7 *ss7, int cic)
{
	return isup_send_message_ciconly(ss7, ISUP_UBL, cic);
}

int isup_bla(struct ss7 *ss7, int cic)
{
	return isup_send_message_ciconly(ss7, ISUP_BLA, cic);
}

int isup_uba(struct ss7 *ss7, int cic)
{
	return isup_send_message_ciconly(ss7, ISUP_UBA, cic);
}
/* Janelle is the bomb (Again) */
