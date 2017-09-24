
/*
 * Network Broadcast Sound Daemon
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
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>
#include <linux/soundcard.h>
#include <getopt.h>
#include <sched.h>
#include "nbs.h"

/* Define to listen to our own broadcast, in order to try to 
   be slightly more in-sync with our other network listeners */
#define LISTEN_SELF

static int sfd = -1;		/* Audio descriptor */
static int sock = -1;		/* UDP socket for remote hosts */
static int usock = -1;		/* Local domain socket for local clients */
static int duplex = 0;
static int debug = 0;
static int ultradebug = 0;
static int streamlocal = 0;
static int nofork = 0;
static int count = 0;
static int nopriority = 0;
static struct sockaddr_in bcast;
static unsigned int muteid = 0;
static unsigned int overspeakid = 0;
static unsigned int emergencyid = 0;
static NBS *localstream;

#define QLEN (NBS_SAMPLES * 2)	/* Up to 2 frames of buffer */

static struct nbs_stream {
	/* Local and remote streams */
	int s;						/* Socket */
	int num;					/* Numerical order */
	unsigned int id;			/* Unique identifier */
	int audiomode;				/* Whether we are in audio mode */
	char app[16];				/* Application name */
	char name[80];				/* Stream name */
	int format;
	int flags;
	int bitrate;
	int channels;
	int audiolen;
	int listener;
	int lastinfo;
	struct timeval lastrx;
	unsigned short seqno;
	/* Keep a couple of frames of samples */
	short q[QLEN * NBS_CHANNELS];
	int qlen;
	struct nbs_stream *next;
} *streams;

static char *ifaces[] =
{
	"eth0",
	"eth1",
};

static int setblocksize(struct nbs_stream *n, const int blocksize)
{
	int res;
	int bs;
	bs = blocksize * 2 * n->channels * 2;
	res = setsockopt(n->s, SOL_SOCKET, SO_RCVBUF, &bs, sizeof(bs));
	return res;
}

static int broadcast(struct nbs_header *h, int datalen)
{
	int res;
	res = sendto(sock, h, datalen + sizeof(struct nbs_header), 0, (struct sockaddr *)&bcast, sizeof(bcast));
	if (res < 0) {
		fprintf(stderr, "NBSD: Warning: transmit failed: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static void nsdrop(struct nbs_stream *ns)
{
	close(ns->s);
	if (muteid == ns->id)
		muteid = 0;
	if (emergencyid == ns->id)
		emergencyid = 0;
	if (overspeakid == ns->id)
		overspeakid = 0;
	free(ns);
}

static void destroy_ns(struct nbs_stream *ns)
{
	struct nbs_stream *prev = NULL, *cur;
	cur = streams;
	while(cur) {
		if (cur == ns) {
			if (prev)
				prev->next = ns->next;
			else
				streams = ns->next;
			nsdrop(ns);
			break;
		}
		prev = cur;
		cur = cur->next;
	}
}

static void check_flags(struct nbs_stream *ns)
{
	if ((ns->flags & NBS_FLAG_MUTE) && !muteid) {
		if (debug)
			printf("NBSD: Client %d is muted\n", ns->num);
		muteid = ns->id;
	}
	if ((ns->flags & NBS_FLAG_OVERSPEAK) && !overspeakid) {
		if (debug)
			printf("NBSD: Client %d is overspeaking\n", ns->num);
		overspeakid = ns->id;
	}
	if ((ns->flags & NBS_FLAG_EMERGENCY) && !emergencyid) {
		if (debug)
			printf("NBSD: Client %d is EMERGENCY\n", ns->num);
		emergencyid = ns->id;
	}
}

static int dc_broadcast(struct nbs_stream *ns)
{
	struct nbs_header h;
	if (!bcast.sin_addr.s_addr)
		return 0;
	memset(&h, 0, sizeof(h));
	h.ver = NBS_VERSION;
	h.ftype = NBS_FRAME_DESTROY;
	h.seqno = htons(ns->seqno++);
	h.streamid = htonl(ns->id);
	broadcast(&h, 0);
	return 0;
}

static int audio_acquire(struct nbs_stream *ns, struct nbs_header *h, int samples)
{
	int x;
	int res;
	int needed;
	short tmp[NBS_SAMPLES * NBS_CHANNELS];
	short *s = (short *)(h->data);
	struct {
		struct nbs_header h;
		struct nbs_streaminfo i;
	} PACKED ih;

	/* Actually read samples */
	if (ns->bitrate == NBS_BITRATE) {
		res = read(ns->s, s, samples * 2 *  ns->channels);
		if (res < 1) {
			return res;
		}
	} else {
		/* XXX Need to resample XXX */
		needed = ((float)(samples)) * ((float)ns->bitrate) / ((float)NBS_BITRATE) + 1;
		res = read(ns->s, tmp, needed * 2 *  ns->channels);
		if (res < 1) {
			return res;
		}
		res = (int)(((float)res) * ((float)NBS_BITRATE) / ((float)ns->bitrate));
		if (res > samples * 2 * ns->channels)
			res = samples * 2 * ns->channels;
		if (ns->channels == 1) {
			for (x=0;x<res/2;x++)
				s[x] = tmp[(int)(x * ((float)ns->bitrate) / ((float)NBS_BITRATE))];
		} else {
			printf("XXX implement me XXX\n");
		}
	}
	/* Get the number of samples */
	res /= (ns->channels * 2);
	if (ns->channels == 1) {
		/* Double the number of samples */
		for (x=res - 1; x>=0; x--) {
			s[(x<<1) + 1] = s[x];
			s[x<<1] = s[x];
		}
	}
	/* Broadcast local aquisition */
	if (bcast.sin_addr.s_addr && !(ns->flags & NBS_FLAG_PRIVATE)) {
		ns->lastinfo -= res;
		if (ns->lastinfo < 0) {
			memset(&ih, 0, sizeof(ih));
			ih.h.ver = NBS_VERSION;
			ih.h.ftype = NBS_FRAME_INFO;
			ih.h.seqno = htons(ns->seqno++);
			ih.h.streamid = htonl(ns->id);
			
			strncpy(ih.i.streamname, ns->name, sizeof(ih.i.streamname) - 1);
			strncpy(ih.i.appname, ns->app, sizeof(ih.i.appname) - 1);
			ih.i.flags = htonl(ns->flags);
			ih.i.bitrate = htons(NBS_BITRATE);
			ih.i.channels = htons(NBS_CHANNELS);
			ih.i.format = htonl(NBS_FORMAT);
			
			ns->lastinfo = NBS_BITRATE;
			broadcast(&ih.h, sizeof(ih.i));
		}
		h->ver = NBS_VERSION;
		h->ftype = NBS_FRAME_AUDIO;
		h->seqno = htons(ns->seqno++);
		h->streamid = htonl(ns->id);
		broadcast(h, res * NBS_CHANNELS * 2);
#ifdef LISTEN_SELF
		/* Pick up our stream on the way back */
		return -2;
#endif
	}
	return res;	
}

static int audio_dequeue(struct nbs_stream *ns, short *s, int samples)
{
	struct timeval tv;
	if (samples > ns->qlen) {
		if (ultradebug)
			printf("Underrun (want %d, got %d)\n", samples, ns->qlen);
		/* See if this needs to be expired */
		gettimeofday(&tv, NULL);
		if (tv.tv_sec - ns->lastrx.tv_sec > 5) {
			/* Expire */
			return -1;
		}
		return 0;
	}
	memcpy(s, ns->q, samples * 2 * NBS_CHANNELS);
	ns->qlen -= samples;
	if (ns->qlen)
		memmove(ns->q, ns->q + samples * NBS_CHANNELS, ns->qlen * 2 * NBS_CHANNELS);
	return samples;
}

static int merge_stream(short *os, struct nbs_stream *ns, int samples)
{
	struct {
		struct nbs_header h;
		short s[NBS_SAMPLES * NBS_CHANNELS];
	} PACKED h;
	int res;
	int x;
	static float realshift=1.0;
	int shift = 0;
	if (ns->s > -1) {
		res = audio_acquire(ns, &h.h, samples);
		/* Do nothing */
		if ((res == -1) && (errno == EAGAIN))
			return 0;
		if (res == -1) {
			if (debug) 
				printf("NBSD: Client %d read error (audio): %s\n", ns->num, strerror(errno));
			dc_broadcast(ns);
			return -1;
		}
		if (res == 0) {
			if (debug) 
				printf("NBSD: Client %d disconnected (audio)\n", ns->num);
			dc_broadcast(ns);
			return -1;
		}
#ifdef LISTEN_SELF
		if (res == -2) {
			/* Treat as audio dequeue */
			res = audio_dequeue(ns, h.s, samples);
			if (!res)
				return 0;
			if (res < 0) {
				return -1;
			}
		}
#endif		
	} else {
		res = audio_dequeue(ns, h.s, samples);
		/* Check for underrun condition */
		if (!res)
			return 0;
		if (res < 0)
			return -1;
	}
	if (emergencyid && (ns->id != emergencyid)) {
		/* Always mute for emergency announcement */
		return 0;
	}
	if (muteid && (ns->id != muteid) && (!emergencyid)) {
		/* Otherwise, mute for the current muteid */
		return 0;
	}
/* #define DEC_FAC	0.9685149 */
#define DEC_FAC 0.9493421
/* #define INC_FAC 1.0325086 */
#define INC_FAC 1.0533610
	
	if (!emergencyid) {
		if (overspeakid) {
			if (ns->id == overspeakid) {
				if (realshift != 0.125) {
					realshift *= DEC_FAC;
					if (realshift < 0.125)
						realshift = 0.125;
				}
			} else
				shift = 1;
		} else {
			if (realshift != 1.0)
				shift = 1;
			if (ns == streams) {
				realshift *= INC_FAC;
				if (realshift > 1.0)
					realshift = 1.0;
			}
		}
	}
	/* res is number of samples, including stereo */
	res <<= 1;
	if (shift) {
		for (x=0;x<res;x++) {
			os[x] += (h.s[x] * realshift);
		}
	} else {
		for (x=0;x<res;x++) {
			os[x] += h.s[x];
		}
	}
	return 0;	
}

static int new_connect(void)
{
	int s;
	int flags;
	struct nbs_stream *ns;
	struct sockaddr_un sun;
	int addrlen;
	addrlen = sizeof(sun);
	s = accept(usock, (struct sockaddr *)&sun, &addrlen);
	if (s < 0) {
		fprintf(stderr, "accept() failed: %s\n", strerror(errno));
		return -1;
	}
	if ((flags = fcntl(s, F_GETFL)) < 0) {
		fprintf(stderr, "Unable to retrieve flags: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
		fprintf(stderr, "Unable to set flags: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	ns = malloc(sizeof(struct nbs_stream));
	if (ns) {
		memset(ns, 0, sizeof(struct nbs_stream));
		ns->s = s;
		ns->num = ++count;
		ns->next = streams;
		streams = ns;
		if (debug)
			printf("NBSD: New client connection %d\n", ns->num);
	} else {
		fprintf(stderr, "Failed to allocate memory for new stream: %s\n", strerror(errno));
		close(s);
		return -1;
	}
	return 0;
}

static struct nbs_stream *find_ns_by_id(unsigned int id)
{
	struct nbs_stream *ns;
	ns = streams;
	while(ns) {
		if (ns->id == id)
			return ns;
		ns = ns->next;
	}
	return ns;
}

static int handle_network(void)
{
	unsigned char data[4096];
	struct nbs_header *h = (struct nbs_header *)data;
	struct sockaddr_in sin;
	int addrlen;
	int res;
	int samples;
	unsigned int id;
	struct nbs_stream *ns;
	struct nbs_streaminfo *si;
	
	addrlen = sizeof(sin);
	res = recvfrom(sock, data, sizeof(data), 0, (struct sockaddr *)&sin, &addrlen);
	if (res < 1)
		return -1;
	if (res < sizeof(struct nbs_header)) 
		return -1;
	if (h->ver != NBS_VERSION)
		return -1;
	res -= sizeof(struct nbs_header);
	id = ntohl(h->streamid);
	ns = find_ns_by_id(id);
	switch(h->ftype) {
	case NBS_FRAME_INFO:
		if (res != sizeof(struct nbs_streaminfo))
			return -1;
		si = (struct nbs_streaminfo *)(h->data);
		if (ntohs(si->channels) != NBS_CHANNELS)
			return -1;
		if (ntohs(si->bitrate) != NBS_BITRATE)
			return -1;
		if (ntohl(si->format) != NBS_FORMAT)
			return -1;
		if (!ns) {
			ns = (struct nbs_stream *)malloc(sizeof(struct nbs_stream));
			if (ns) {
				memset(ns, 0, sizeof(struct nbs_streaminfo));
				ns->id = id;
				ns->s = -1;
				ns->num = ++count;
				ns->audiomode = 1;
				gettimeofday(&ns->lastrx, NULL);
				strncpy(ns->name, si->streamname, sizeof(ns->name) - 1);
				strncpy(ns->app, si->appname, sizeof(ns->app) - 1);
				ns->flags = ntohl(si->flags);
				ns->bitrate = ntohs(si->bitrate);
				ns->channels = ntohs(si->channels);
				ns->format = ntohl(si->format);
				ns->next = streams;
				check_flags(ns);
				streams = ns;
				if (debug)
					printf("NBSD: New stream: %s/%s (flags %d) id %04x\n", ns->app, ns->name, ns->flags, ns->id);
			}
		}
		break;
	case NBS_FRAME_AUDIO:
		/* Ignore unknown audio streams */
		if (!ns)
			return 0;
#ifndef LISTEN_SELF
		/* Ignore our own audio streams */
		if (ns->s > -1) 
			return 0;
#endif			
		gettimeofday(&ns->lastrx, NULL);
		samples = QLEN - ns->qlen;
		res /= (NBS_CHANNELS * 2);
		if (samples >= res)
			samples = res;
		else {
			if (ultradebug)
				printf("Overrun (want %d, have %d/%d)\n", res, samples, ns->qlen);
		}
		/* Now we know the max # of samples we can use */
		if (samples) {
			memcpy(ns->q + ns->qlen * NBS_CHANNELS, h->data, samples * NBS_CHANNELS * 2);
			ns->qlen += samples;
		}
		break;		
	case NBS_FRAME_DESTROY:
		if (ns && (ns->s == -1)) {
			if (debug)
				printf("NBSD: Stream %s/%s (id %04x) terminated\n", ns->app, ns->name, ns->id);
			destroy_ns(ns);
		}
		break;
	default:
		fprintf(stderr, "Unknown frametype: %d\n", h->ftype);
		return -1;
	}
	return 0;
}

static void nbsl_header_init(struct nbsl_header *h, int cmd, int length)
{
	h->magic = NBSL_MAGIC;
	h->length = length;
	h->cmd = cmd;
}

static int send_msg(struct nbs_stream *ns, int cmd, void *data, int datalen)
{
	struct nbsl_message *m;
	int res;
	m = alloca(sizeof(struct nbsl_message) + datalen);
	memset(m, 0, sizeof(struct nbsl_message) + datalen);
	nbsl_header_init(&m->h, cmd, datalen);
	if (datalen)
		memcpy(m->h.data, data, datalen);
	res = write(ns->s, &m->h, sizeof(m->h) + datalen);
	if (res != sizeof(m->h) + datalen) {
		fprintf(stderr, "Short write, going to client %d\n", ns->num);
		return -1;
	}
	return 0;
}

static int timing_ready(void)
{
	short data[NBS_SAMPLES * NBS_CHANNELS];
	int res;
	int actual;
	int samples;
	struct nbs_stream *ns, *prev, *cur;
	audio_buf_info abi;
	
	if (duplex || localstream) {
		actual = read(sfd, data, sizeof(data));
		if (actual < 0) {
			fprintf(stderr, "Failed to read from audio interface: %s\n", strerror(errno));
			return -1;
		}
		if (localstream)
			nbs_write(localstream, data, actual/4);
	} else {
		res = ioctl(sfd, SNDCTL_DSP_GETOSPACE, &abi);
		if (res) {
			fprintf(stderr, "Unable to read output space\n");
			return -1;
		}
		actual = abi.fragments * abi.fragsize;
		if (actual > NBS_SAMPLES * NBS_CHANNELS * 2)
			actual = NBS_SAMPLES * NBS_CHANNELS * 2;
	}
	samples = actual / (NBS_CHANNELS * 2);
	/* actual now represents the real amount of data we can handle, and samples
	   the number of samples */	
	memset(data, 0, actual);

	ns = streams;
	prev = NULL;
	while(ns) {
		/* Add audio streams */
		if (ns->audiomode) {
			res = merge_stream(data, ns, samples);
			if (res < 0) {
				/* Drop them now */
				if (prev)
					prev->next = ns->next;
				else
					streams = ns->next;
				cur = ns;
				ns = ns->next;
				nsdrop(cur);
				continue;
			}
		}
		prev = ns;
		ns = ns->next;
	}; 
	
	res = write(sfd, data, actual);
	if (res != actual) {
		fprintf(stderr, "Writing returned %d while expecting %d\n", res, actual);
		return -1;
	}
	if (res < 0) {
		fprintf(stderr, "Failed to write audio data: %s\n", strerror(errno));
		return -1;
	}
	ns = streams;
	prev = NULL;
	while(ns) {
		/* Transmit audio to listeners */
		if (ns->listener) {
			if ((send_msg(ns, NBSL_FRAME_LISTENAUDIO, data, actual) < 0) &&
				(errno != -EAGAIN)) {
				/* Drop them now */
				if (prev)
					prev->next = ns->next;
				else
					streams = ns->next;
				cur = ns;
				ns = ns->next;
				nsdrop(cur);
				continue;
			}
		}
		prev = ns;
		ns = ns->next;
	}; 
	if (debug) {
		static int didfirst;
		if (!didfirst) {
			printf("NBSD: Timing Read/Write %d of %d\n", res, actual);
			didfirst++;
		}
	}
	return 0;
}

static int nbs_nak(struct nbs_stream *ns, int reason)
{
	return send_msg(ns, NBSL_FRAME_SETUPNAK, &reason, sizeof(reason));
}

static int nbs_ack(struct nbs_stream *ns)
{
	return send_msg(ns, NBSL_FRAME_SETUPACK, NULL, 0);
}

static int nbs_audioready(struct nbs_stream *ns)
{
	return send_msg(ns, NBSL_FRAME_AUDIOREADY, NULL, 0);
}

static int check_command(struct nbs_stream *ns)
{
	unsigned char data[1024];
	struct nbsl_message *nm = (struct nbsl_message *)data;
	int res;
	int maxlen;
	
	if ((res = read(ns->s, &nm->h, sizeof(nm->h))) != sizeof(nm->h)) {
		if (!res) {
			/* It's gone */
			if (debug)
				printf("NBSD: Disconnect from client %d\n", ns->num);
			destroy_ns(ns);
			return -1;
		}
		fprintf(stderr, "Invalid command frame received from client %d (%d of %d)\n", ns->num, res, (int)sizeof(nm->h));
		destroy_ns(ns);
		return -1;
	}
	if (nm->h.magic != NBSL_MAGIC) {
		fprintf(stderr, "Invalid magic on command frame from client %d\n", ns->num);
		destroy_ns(ns);
		return -1;
	}
	maxlen = sizeof(data) - sizeof(nm->h);
	if (nm->h.length > maxlen) {
		fprintf(stderr, "Length too long on command frame from client %d\n", ns->num);
		destroy_ns(ns);
		return -1;
	}
	res = read(ns->s, nm->h.data, nm->h.length);
	if (res < nm->h.length) {
		fprintf(stderr, "Short read on frame from client %d\n", ns->num);
		destroy_ns(ns);
		return -1;
	}
	switch(nm->h.cmd) {
	case NBSL_FRAME_SETUP:
		if (nm->h.length != sizeof(struct nbsl_setup)) {
			fprintf(stderr, "Invalid setup length from client %d\n", ns->num);
			destroy_ns(ns);
			return -1;
		}
		if ((nm->u.s.bitrate < 8000) ||
			(nm->u.s.bitrate > 44100)) {
			fprintf(stderr, "Unsupported bitrate %d from client %d\n", nm->u.s.bitrate, ns->num);
			nbs_nak(ns, NBSL_REASON_UBITRATE);
			return -1;
		}
		if ((nm->u.s.channels != 1) && (nm->u.s.channels != 2)) {
			fprintf(stderr, "Unsupported channel count %d from client %d\n", nm->u.s.channels, ns->num);
			nbs_nak(ns, NBSL_REASON_UCHANNELS);
			return -1;
		}
		if (nm->u.s.format != NBS_FORMAT) {
			fprintf(stderr, "Unsupported format %d from client %d\n", nm->u.s.format, ns->num);
			nbs_nak(ns, NBSL_REASON_UFORMAT);
			return -1;
		}
		strncpy(ns->name, nm->u.s.streamname, sizeof(ns->name) - 1);
		strncpy(ns->app, nm->u.s.appname, sizeof(ns->app) - 1);
		ns->flags = nm->u.s.flags;
		ns->bitrate = nm->u.s.bitrate;
		ns->channels = nm->u.s.channels;
		ns->audiolen = nm->u.s.audiolen;
		ns->format = nm->u.s.format;
		ns->id = nm->u.s.id;
		gettimeofday(&ns->lastrx, NULL);
		setblocksize(ns, ns->audiolen);
		check_flags(ns);
		nbs_ack(ns);
		if (debug)
			printf("NBSD: Client %d is %s/%s (%08x)\n", ns->num, ns->app, ns->name, ns->id);
		break;
	case NBSL_FRAME_LISTENER:
		if (debug)
			printf("NBSD: Client %d going into listener mode\n", ns->num);
		ns->listener = 1;
		break;
	case NBSL_FRAME_AUDIOMODE:
		ns->audiomode = 1;
		nbs_audioready(ns);
		if (debug)
			printf("NBSD: Client %d going into audio mode\n", ns->num);
		break;
	default:
		fprintf(stderr, "Unknown command %d from client %d\n", nm->h.cmd, ns->num);
	}
	return 0;
}

static int main_loop(void)
{
	int max;
	fd_set rfds, wfds;
	int res;
	struct nbs_stream *ns;
	struct sched_param sp;
	
	/* Run one timing loop */
	timing_ready();
	
	if (!nopriority) {
		memset(&sp, 0, sizeof(sp));
		sp.sched_priority = 1;
		if (sched_setscheduler(0, SCHED_RR, &sp)) {
			fprintf(stderr, "NBSD: Warning: Unable to set realtime priority\n");
		}
	}
	
	if (bcast.sin_addr.s_addr && debug)
		printf("NBSD: Running on broadcast %s\n", inet_ntoa(bcast.sin_addr));
	for(;;) {
		max = -1;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_SET(usock, &rfds);
		if (usock > max)
			max = usock;
		if (bcast.sin_addr.s_addr) {
			FD_SET(sock, &rfds);
			if (sock > max)
				max = sock;
		}
		/* If full duplex, we time on reads, otherwise we time on writes */
		if (duplex)
			FD_SET(sfd, &rfds);
		else
			FD_SET(sfd, &wfds);
		if (sfd > max)
			max = sfd;

		/* Watch any streams that are still in command mode */
		ns = streams;
		while(ns) {
			if ((ns->s > -1) && !ns->audiomode) {
				FD_SET(ns->s, &rfds);
				if (ns->s > max)
					max = ns->s;
			}
			ns = ns->next;
		}
		res = select(max +1, &rfds, &wfds, NULL, NULL);
		if (res < 0) {
			fprintf(stderr, "NBSD: Dying on signal\n");
			return 0;
		}
		if (FD_ISSET(usock, &rfds)) {
			/* Got a new connection, make it live */
			new_connect();
		}
		if (bcast.sin_addr.s_addr) {
			if (FD_ISSET(sock, &rfds)) {
				handle_network();
			}
		}
		if (FD_ISSET(sfd, &rfds) || FD_ISSET(sfd, &wfds)) {
			timing_ready();
		}
		/* Look for any streams ready in command mode */
		ns = streams;
		while(ns) {
			if ((ns->s > -1) && !ns->audiomode) {
				if (FD_ISSET(ns->s, &rfds))
					check_command(ns);
			}
			ns = ns->next;
		}
	} 
	unlink(NBSL_SOCKET);
	return 0;
}

static int choose_bcast(struct sockaddr_in *sin)
{
	struct ifreq ifr;
	int res;
	int x;
	for (x=0;x<sizeof(ifaces) / sizeof(ifaces[0]); x++) {
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, ifaces[x]);
		if (!(res = ioctl(sock, SIOCGIFBRDADDR, &ifr))) {
			memcpy(sin, &ifr.ifr_broadaddr, sizeof(*sin));
			sin->sin_port = htons(NBS_PORT);
			break;
		}
	}
	if (x<sizeof(ifaces) / sizeof(ifaces[0])) 
		return 0;
	return -1;
}

static void *build_local_stream(void *ign)
{
	NBS *tmp;
	sleep(2);
	tmp = nbs_newstream("localbroadcast", "localhost", 0);
	if (tmp) {
		if (nbs_connect(tmp)) {
			fprintf(stderr, "Failed to connect local stream!\n");
			nbs_delstream(tmp);
			tmp = NULL;
		}
		nbs_setblocking(tmp, 0);
	}
	localstream = tmp;
	return NULL;
}

int main(int argc, char *argv[])
{
	int speed = NBS_BITRATE;
	int format = AFMT_S16_LE;
	int channels = NBS_CHANNELS - 1;
	int frag = ((NBS_SAMPLES * NBS_CHANNELS * 2/ NBS_CHUNK) << 16) | (NBS_CHUNK_PWR);
	struct sockaddr_in sin;
	struct sockaddr_un sun;
	int x;
	int res;
	char c;

	while((c = getopt(argc, argv, "dulpfb:")) != EOF) {
		switch(c) {
		case 'd':
			debug = 1;
			break;
		case 'u':
			ultradebug = 1;
			break;
		case 'f':
			nofork = 1;
			break;
		case 'p':
			nopriority = 1;
			break;
		case 'b':
			bcast.sin_family = AF_INET;
			bcast.sin_port = htons(NBS_PORT);
			if (!inet_aton(optarg, &bcast.sin_addr)) {
				fprintf(stderr, "Invalid broadcast address\n");
				exit(1);
			} 
			break;
		case 'l':
			streamlocal = 1;
			break;
		case '?':
			exit(1);
		}
	}
	
	sfd = open(NBS_DSP, O_RDWR | O_NONBLOCK);
	if (sfd < 0) {
		fprintf(stderr, "Unable to open real audio device: %s\n", strerror(errno));
		exit(1);
	}
	res = ioctl(sfd, SNDCTL_DSP_SETDUPLEX, 0);
	if (res < 0) {
		fprintf(stderr, "NBSD: Warning: Unable to set full duplex mode: %s\n", strerror(errno));
	} else
		duplex = 1;
	if (!duplex) {
		close(sfd);
		sfd = open(NBS_DSP, O_WRONLY | O_NONBLOCK);
		if (sfd < 0) {
			fprintf(stderr, "Unable to reopen real audio device: %s\n", strerror(errno));
			exit(1);
		}
	}
	res = ioctl(sfd, SNDCTL_DSP_SETFMT, &format);
	if ((res < 0) || (format != AFMT_S16_LE))  {
		fprintf(stderr, "Unable to set linear mode: %s\n", strerror(errno));
		exit(1);
	}
	res = ioctl(sfd, SNDCTL_DSP_STEREO, &channels);
	if ((res < 0) || (channels != NBS_CHANNELS - 1)) {
		fprintf(stderr, "Unable to set stereo mode: %s\n", strerror(errno));
		exit(1);
	}
	res = ioctl(sfd, SNDCTL_DSP_SPEED, &speed);
	if ((res < 0) || (speed != NBS_BITRATE)) {
		fprintf(stderr, "NBSD: Warning: Unable to set bitrate to %d (%d): %s\n", NBS_BITRATE, speed, strerror(errno));
	}
	res = ioctl(sfd, SNDCTL_DSP_SETFRAGMENT, &frag);
	if ((res < 0)) {
		fprintf(stderr, "Unable to set fragment size: %s\n", strerror(errno));
		exit(1);
	}
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stderr, "Unable to open socket: %s\n", strerror(errno));
		exit(1);
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(NBS_PORT);
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin))) {
		fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
		exit(1);
	}
	x = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &x, sizeof(x))) {
		fprintf(stderr, "Failed to put socket in broadcast mode: %s\n", strerror(errno));
		exit(1);
	}
	if (!bcast.sin_addr.s_addr) {
		if (choose_bcast(&bcast)) {
			fprintf(stderr, "NBSD: Warning: No broadcast, running local mode only.\n");
			close(sock);
			sock = -1;
		}
	}
	unlink(NBSL_SOCKET);
	usock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (usock < 0) {
		fprintf(stderr, "Unable to create local domain socket: %s\n", strerror(errno));
		exit(1);
	}
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path, NBSL_SOCKET);
	if (bind(usock, (struct sockaddr *)&sun, sizeof(sun))) {
		fprintf(stderr, "Failed to bind local domain socket: %s\n", strerror(errno));
		exit(1);
	}
	chmod(NBSL_SOCKET, 0x666);
	if (listen(usock, 2)) {
		fprintf(stderr, "Failed to listen on local domain socket: %s\n", strerror(errno));
		close(usock);
		unlink(NBSL_SOCKET);
		exit(1);
	}
	if (streamlocal) {
		pthread_t st;
		pthread_create(&st, NULL, build_local_stream, NULL);
	}
	if (!nofork) {
		if (daemon(0, 0)) {
		}
	}
	main_loop();
	exit(0);
}
