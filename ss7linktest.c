#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <zaptel/zaptel.h>
#include "libss7.h"

struct linkset {
	struct ss7 *ss7;
	int linkno;
	int fd;
} linkset[2];

int linknum = 1;
int callcount = 0;
unsigned int opc;
unsigned int dpc;

#define NUM_BUFS 32

void ss7_call(struct ss7 *ss7)
{
	int i;
	struct isup_call *c;

	c = isup_new_call(ss7);

	if (c) {
		isup_set_called(c, "12345", SS7_NAI_NATIONAL, ss7);
		isup_set_calling(c, "7654321", SS7_NAI_NATIONAL, SS7_PRESENTATION_ALLOWED, SS7_SCREENING_USER_PROVIDED);
		isup_init_call(ss7, c, (callcount % 12) + 1, dpc);
		isup_iam(ss7, c);
		printf("Callcount = %d\n ", ++callcount);
	}
}

void *ss7_run(void *data)
{
	int res = 0;
	unsigned char readbuf[512] = "";
	struct timeval *next = NULL, tv;
	struct linkset *linkset = (struct linkset *) data;
	struct ss7 *ss7 = linkset->ss7;
	int ourlink = linknum;
	ss7_event *e = NULL;
	struct pollfd poller;
	int nextms;
	int x;

	printf("Starting link %d\n", linknum++);
	ss7_start(ss7);

	while(1) {
		if ((next = ss7_schedule_next(ss7))) {
			gettimeofday(&tv, NULL);
			tv.tv_sec = next->tv_sec - tv.tv_sec;
			tv.tv_usec = next->tv_usec - tv.tv_usec;
			if (tv.tv_usec < 0) {
				tv.tv_usec += 1000000;
				tv.tv_sec -= 1;
			}
			if (tv.tv_sec < 0) {
				tv.tv_sec = 0;
				tv.tv_usec = 0;
			}
			nextms = tv.tv_sec * 1000;
			nextms += tv.tv_usec / 1000;
		}
		poller.fd = linkset->fd;
		poller.events = POLLIN | POLLOUT | POLLPRI;
		poller.revents = 0;

		res = poll(&poller, 1, nextms);
		if (res < 0) {
			printf("next->tv_sec = %d\n", next->tv_sec);
			printf("next->tv_usec = %d\n", next->tv_usec);
			printf("tv->tv_sec = %d\n", tv.tv_sec);
			printf("tv->tv_usec = %d\n", tv.tv_usec);
			perror("select");
		}
		else if (!res) {
			ss7_schedule_run(ss7);
			continue;
		}

		if (poller.revents & POLLPRI) {
			if (ioctl(linkset->fd, ZT_GETEVENT, &x)) {
				perror("Error in exception retrieval!\n");
			}
			switch (x) {
				case ZT_EVENT_OVERRUN:
					printf("Overrun detected!\n");
					break;
				case ZT_EVENT_BADFCS:
					printf("Bad FCS!\n");
					break;
				case ZT_EVENT_ABORT:
					printf("HDLC Abort!\n");
					break;
				default:
					printf("Got exception %d!\n", x);
					break;
			}
		}

		if (poller.revents & POLLIN)
			res = ss7_read(ss7, linkset->fd);
		if (poller.revents & POLLOUT) {
			res = ss7_write(ss7, linkset->fd);
			if (res < 0) {
				printf("Error in write %s", strerror(errno));
			}
		}

#if 0
		if (res < 0)
			exit(-1);
#endif

		while ((e = ss7_check_event(ss7))) {
			if (e) {
				switch (e->e) {
					case SS7_EVENT_UP:
						printf("[%d] --- SS7 Up ---\n", linkset->linkno);
						isup_grs(ss7, 1, 24, dpc);
						break;
					case MTP2_LINK_UP:
						printf("[%d] MTP2 link up\n", linkset->linkno);
						break;
					case ISUP_EVENT_GRS:
						printf("Got GRS from cic %d to %d: Acknowledging\n", e->grs.startcic, e->grs.endcic);
						isup_gra(ss7, e->grs.startcic, e->grs.endcic, dpc);
						break;
					case ISUP_EVENT_RSC:
						isup_rlc(ss7, e->rsc.call);
						break;
					case ISUP_EVENT_GRA:
						printf("Got GRA from cic %d to %d.\n", e->gra.startcic, e->gra.endcic);
						ss7_call(ss7);
						break;
					case ISUP_EVENT_BLO:
						isup_bla(ss7, e->blo.cic, dpc);
						break;
					case ISUP_EVENT_CGB:
						isup_cgba(ss7, e->cgb.startcic, e->cgb.endcic, dpc, e->cgb.status, e->cgb.type);
						break;
					case ISUP_EVENT_CGU:
						isup_cgua(ss7, e->cgu.startcic, e->cgu.endcic, dpc, e->cgu.status, e->cgu.type);
						break;
					case ISUP_EVENT_IAM:
						printf("Got IAM for cic %d and number %s\n", e->iam.cic, e->iam.called_party_num);
						printf("CallerID is %s\n", e->iam.calling_party_num);
						printf("Sending ACM\n");
						isup_acm(ss7, e->iam.call);
						printf("Sending ANM\n");
						isup_anm(ss7, e->iam.call);
						break;
					case ISUP_EVENT_REL:
						printf("Got REL for cic %d\n", e->rel.cic);
						isup_rlc(ss7, e->rel.call);
						//ss7_call(ss7);
						break;
					case ISUP_EVENT_ACM:
						printf("Got ACM for cic %d\n", e->acm.cic);
						break;
					case ISUP_EVENT_ANM:
						printf("Got ANM for cic %d\n", e->anm.cic);
						isup_rel(ss7, e->anm.call, 16);
						break;
					case ISUP_EVENT_RLC:
						printf("Got RLC for cic %d\n", e->rlc.cic);
						ss7_call(ss7);
						break;
					default:
						printf("Unknown event %d\n", e->e);
						break;
				}
			}
		}
	}
}

void myprintf(struct ss7 *ss7, char *fmt)
{
	int i = 0;
	printf("%s", fmt);
}

int zap_open(int devnum)
{
	int fd;
	ZT_BUFFERINFO bi;
	fd = open("/dev/zap/channel", O_RDWR|O_NONBLOCK, 0600);
	if ((fd < 0) || (ioctl(fd, ZT_SPECIFY, &devnum) == -1)) {
		printf("Could not open device %d: %s\n", devnum, strerror(errno));
		return -1;
	}
	bi.txbufpolicy = ZT_POLICY_IMMEDIATE;
	bi.rxbufpolicy = ZT_POLICY_IMMEDIATE;
	bi.numbufs = NUM_BUFS;
	bi.bufsize = 512;
	if (ioctl(fd, ZT_SET_BUFINFO, &bi)) {
		close(fd);
		return -1;
	}
	return fd;
}

void print_args(void)
{
	printf("Incorrect arguments.  Should be:\n");
	printf("ss7linktest [sigchan number] [ss7 style - itu or ansi] [OPC - in decimal] [DPC - in decimal]\n");
	printf("Example:\n");
	printf("ss7linktest 16 itu 1 2\n");
	printf("This would run the linktest program on zap/16 with ITU style signalling, with an OPC of 1 and a DPC of 2\n");
}

int main(int argc, char *argv[])
{
	int fd;
	struct ss7 *ss7;
	pthread_t tmp;
	int channum;
	unsigned int type;

	if (argc < 5) {
		print_args();
		return -1;
	}
	channum = atoi(argv[1]);

	if (!strcasecmp(argv[2], "ansi")) {
		type = SS7_ANSI;
	} else if (!strcasecmp(argv[2], "itu")) {
		type = SS7_ITU;
	} else {
		print_args();
		return -1;
	}

	opc = atoi(argv[3]);
	dpc = atoi(argv[4]);

	fd = zap_open(channum);

	if (fd == -1)
		return -1;

	if (!(ss7 = ss7_new(type))) {
		perror("ss7_new");
		exit(1);
	}
	linkset[0].ss7 = ss7;
	linkset[0].fd = fd;
	linkset[0].linkno = 0;

	ss7_set_message(myprintf);
	ss7_set_error(myprintf);
	ss7_set_network_ind(ss7, SS7_NI_NAT);

	ss7_set_debug(ss7, 0xfffffff);
	if ((ss7_add_link(ss7, fd))) {
		perror("ss7_add_link");
		exit(1);
	}

	ss7_set_pc(ss7, opc);
	ss7_set_adjpc(ss7, fd, dpc);

	if (pthread_create(&tmp, NULL, ss7_run, &linkset[0])) {
		perror("thread(0)");
		exit(1);
	}

	pthread_join(tmp, NULL);

	return 0;
}
