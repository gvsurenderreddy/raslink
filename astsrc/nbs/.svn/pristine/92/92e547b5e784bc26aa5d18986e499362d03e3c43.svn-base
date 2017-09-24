/*
 * Network Broadcast Sound Daemon (Client Library)
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

struct nbs {
	char app[16];
	char name[80];
	unsigned int flags;
	unsigned int id;
	int s;
	int blocksize;
	int debug;
	int channels;
	int format;
	int bitrate;
	int blocking;
};

static void nbsl_header_init(struct nbsl_header *h, int cmd, int length)
{
	h->magic = NBSL_MAGIC;
	h->length = length;
	h->cmd = cmd;
}

int nbs_setblocksize(NBS *n, const int blocksize)
{
	int res;
	int bs;
	n->blocksize = blocksize;
	bs = blocksize * 2 * n->channels * 2;
	res = setsockopt(n->s, SOL_SOCKET, SO_SNDBUF, &bs, sizeof(bs));
	if (!res)
		res = setsockopt(n->s, SOL_SOCKET, SO_SNDBUF, &bs, sizeof(bs));
	return res;
}

struct nbsl_header *nbs_read(NBS *n, void *buf, int len)
{
	int res;
	struct nbsl_header *h = buf;
	res = read(n->s, buf, sizeof(struct nbsl_header));
	if (res != sizeof(struct nbsl_header)) {
		if (n->debug)
			fprintf(stderr, "Short read on NBSL header\n");
		return NULL;
	}
	/* Verify header */
	if (h->magic  != NBSL_MAGIC) {
		if (n->debug)
			fprintf(stderr, "Invalid NBSL magic received\n");
		return NULL;
	}
	len -= sizeof(struct nbsl_header);
	if (len < h->length) {
		if (n->debug)
			fprintf(stderr, "Insufficient space for read\n");
		return NULL;
	}
	res = read(n->s, h->data, h->length);
	if (res < h->length) {
		if (n->debug)
			fprintf(stderr, "Short read on NBSL data\n");
		return NULL;
	}
	return h;
}

static int nbs_setup(NBS *n)
{
	struct nbsl_message setup;
	int res;
	struct nbsl_nak *nak;
	struct nbsl_header *h;
	char data[2048];
	
	
	memset(&setup, 0, sizeof(setup));
	strncpy(setup.u.s.streamname, n->name, sizeof(setup.u.s.streamname) - 1);
	strncpy(setup.u.s.appname, n->app, sizeof(setup.u.s.appname) - 1);
	setup.u.s.flags = n->flags;
	setup.u.s.bitrate = n->bitrate;
	setup.u.s.channels = n->channels;
	setup.u.s.format = n->format;
	setup.u.s.audiolen = n->blocksize;
	setup.u.s.id = n->id;
	nbsl_header_init(&setup.h, NBSL_FRAME_SETUP, sizeof(setup.u.s));
	res = write(n->s, &setup, sizeof(setup.h) + sizeof(setup.u.s));
	if (res != sizeof(setup.h) + sizeof(setup.u.s)) {
		if (n->debug) 
			fprintf(stderr, "Unable to write setup frame: %s\n", strerror(errno));
		return -1;
	}
	for(;;) {
		h = nbs_read(n, data, sizeof(data));
		if (!h)
			return -1;
		switch(h->cmd) {
		case NBSL_FRAME_SETUPACK:
			if (n->debug)
				fprintf(stderr,"NBS: Connected and acked with server\n");
			nbsl_header_init(&setup.h, NBSL_FRAME_AUDIOMODE, 0);
			res = write(n->s, &setup, sizeof(setup.h));
			if (res != sizeof(setup.h)) {
				if (n->debug) 
					fprintf(stderr, "Unable to write setup frame: %s\n", strerror(errno));
				return -1;
			}
			break;
		case NBSL_FRAME_SETUPNAK:
			if (h->length >= sizeof(struct nbsl_nak)) {
				nak = (struct nbsl_nak *)(h->data);
				if (n->debug)
					fprintf(stderr, "NBS: NAK'd for reason %d\n", nak->reason);
			} else if (n->debug)
				fprintf(stderr, "NBS: NAK'd for for no reason\n");
			return -1;
		case NBSL_FRAME_AUDIOREADY:
			if (n->debug)
				fprintf(stderr, "NBS: Server ready for audio\n");
			return 0;
		default:
			if (n->debug)
				fprintf(stderr, "NBS: Unknown command %d\n", h->cmd);
			return -1;
		}
	}	 
	/* Never reached */
	return 0;
}

static int nbs_sendlisten(NBS *n)
{
	struct nbsl_message listener;
	int res;
	
	
	memset(&listener, 0, sizeof(listener));
	nbsl_header_init(&listener.h, NBSL_FRAME_LISTENER, 0);
	res = write(n->s, &listener, sizeof(listener.h));
	if (res != sizeof(listener.h)) {
		if (n->debug) 
			fprintf(stderr, "Unable to write listener frame: %s\n", strerror(errno));
		return -1;
	}
	if (n->debug)
		fprintf(stderr, "NBS: Server ready for listening\n");
	return 0;
}

NBS *nbs_newstream(const char *app, const char *name, const int flags)
{
	int s;
	struct sockaddr_un sun;
	NBS *n;
	srand(time(NULL));
	s = socket(PF_UNIX, SOCK_STREAM, 0);
	if (s < 0) 
		return NULL;
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, NBSL_SOCKET);
	if (connect(s, (struct sockaddr *)&sun, sizeof(sun))) {
		/* Try launching nbsd */
		if (errno == ECONNREFUSED) {
			if (system("/usr/sbin/nbsd")) {
			}
			if (connect(s, (struct sockaddr *)&sun, sizeof(sun))) {
				close(s);
				return NULL;
			}
		} else {
			close(s);
			return NULL;
		}
	}
	n = malloc(sizeof(NBS));
	if (n) {
		memset(n, 0, sizeof(NBS));
		strncpy(n->app, app, sizeof(n->app) - 1);
		strncpy(n->name, name, sizeof(n->name) - 1);
		n->flags = flags;
		n->s = s;
		n->debug = 0;
		n->bitrate = NBS_BITRATE;
		n->channels = NBS_CHANNELS;
		n->format = NBS_FORMAT;
		n->id = rand();
		n->blocking = 1;
		if (nbs_setblocksize(n, NBS_BLOCKSIZE))  {
			close(n->s);
			free(n);
			n = NULL;
		}
	}
	return n;
}

int nbs_connect(NBS *n)
{
	int res;
	int blocking = n->blocking;
	/* Force back to blocking mode */
	if (!blocking)
		nbs_setblocking(n, 1);
	res = nbs_setup(n);
	if (!blocking)
		nbs_setblocking(n, 0);
	return res;
}

int nbs_listen(NBS *n)
{
	int res;
	int blocking = n->blocking;
	/* Force back to blocking mode */
	if (!blocking)
		nbs_setblocking(n, 1);
	res = nbs_sendlisten(n);
	if (!blocking)
		nbs_setblocking(n, 0);
	return res;
}

void nbs_setdebug(NBS *n, int debug)
{
	n->debug = debug;
}

void nbs_delstream(NBS *n)
{
	close(n->s);
	free(n);
}

int nbs_fd(NBS *n)
{
	return n->s;
}

int nbs_write(NBS *n, const void *data, const int samples)
{
	int res;
	res = write(n->s, data, samples * n->channels * 2);
	if (res < 0)
		return res;
	return res / (n->channels * 2);
}

int nbs_freespace(NBS *n)
{
	/* Linux (nor UNIX) gives us a way to read the amount of
	   space remaining in a transmit buffer, so instead we use
	   select() combined with our knowledge of Linux's internal
	   select logic on sockets and the block size to determine
	   a minimum of space available */
	fd_set fds;	
	int res;
	struct timeval tv = { 0, 0};
	FD_ZERO(&fds);
	FD_SET(n->s, &fds);
	res = select(n->s + 1, NULL, &fds, NULL, &tv);
	if (res > 0)
		return n->blocksize;
	return 0;
}

int nbs_setblocking(NBS *n, const int blocking)
{
	int flags;
	flags = fcntl(n->s, F_GETFL);
	if (flags < 0)
		return -1;
	if (blocking)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;
	n->blocking = blocking;
	return fcntl(n->s, F_SETFL);
}

void nbs_setchannels(NBS *n, const int channels)
{
	n->channels = channels;
	nbs_setblocksize(n, n->blocksize);
}

void nbs_setbitrate(NBS *n, const int bitrate)
{
	n->bitrate = bitrate;
}
