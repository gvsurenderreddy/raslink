/*
 * Network Broadcast Sound Daemon (Client Listener)
 *
 * Mark Spencer <markster@digium.com>
 *
 * Distributed under the terms of the GNU General Public License
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <time.h>
#include "nbs.h"

int main(int argc, char *argv[])
{
	static int debug =0;
	NBS *nbs;
	struct nbsl_header *h;
	unsigned char buf[NBS_BLOCKSIZE * 8];
	if (argc > 1) {
		if (!strcmp(argv[1], "-d"))
			debug = 1;
		else {
			fprintf(stderr, "Usage: nbscat [ -d ]\n");
			exit(1);
		}
	}
	nbs = nbs_newstream("nbscat", "listener", 0);
	if (!nbs) {
		fprintf(stderr, "Unable to create listener: %s\n", strerror(errno));
		exit(1);
	}
	nbs_setdebug(nbs, debug);
	if (nbs_listen(nbs)) {
		fprintf(stderr, "Unable to listen: %s\n", strerror(errno));
		exit(1);
	}
	for (;;) {
		h = nbs_read(nbs, buf, sizeof(buf));
		if (!h) {
			fprintf(stderr, "Stream terminated\n");
			break;
		}
		if (h->cmd == NBSL_FRAME_LISTENAUDIO) {
			if (write(STDOUT_FILENO, h->data, h->length)) {
			}
		} else {
			fprintf(stderr, "Unexpected frame command: %d!\n", h->cmd);
		}
	}
	exit(0);
}
