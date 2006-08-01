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

typedef unsigned int point_code;

struct routing_label {
	unsigned int type;
	point_code dpc;
	point_code opc;
	unsigned char sls;
};

/* Process any MTP2 events that occur */
ss7_event* mtp3_process_event(struct ss7 *ss7, ss7_event *e);

/* The main receive function for MTP3 */
int mtp3_receive(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

int mtp3_dump(struct ss7 *ss7, struct mtp2 *link, void *msg, int len);

/* Transmit */
int mtp3_transmit(struct ss7 *ss7, unsigned char userpart, unsigned char sls, struct ss7_msg *m);

unsigned char sls_next(struct ss7 *ss7);

int set_routinglabel(unsigned char *sif, struct routing_label *rl);

unsigned char sls_next(struct ss7 *ss7);

#endif /* _MTP3_H */
