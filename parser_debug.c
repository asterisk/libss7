#include <stdio.h>
#include "libss7.h"
#include "ss7_internal.h"
#include "mtp2.h"
#include "isup.h"
#include "mtp3.h"

int main(int argc, char **argv)
{
	FILE *fp;
	struct ss7 *ss7 = ss7_new(SS7_ANSI);
	unsigned char mybuf[512];
	int res = 0, i = 0, size;
	ss7_event *e;

	if (argc != 2)
		return -1;

	fp = fopen(argv[1], "r");

	while (res != EOF) {
		res = fscanf(fp, "%x ", &mybuf[i++]);
	}

	size = i + 1;

	for (i = 0; i < size; i++) {
		printf("%.2x ", mybuf[i]);
	}

	printf("\n");

	ss7_add_link(ss7, 10);

	ss7->debug = SS7_DEBUG_MTP2 | SS7_DEBUG_MTP3 | SS7_DEBUG_ISUP;
	ss7->links[0]->state = MTP_INSERVICE;

	mtp2_receive(ss7->links[0], mybuf, size);

	e = ss7_check_event(ss7);

	return 0;
}
