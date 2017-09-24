/*
 * Network Broadcast Sound
 *
 * Common include information
 *
 * Mark Spencer <markster@digium.com
 *
 * Distributed under the terms of GNU GPL.  See "LICENSE" for details.
 *
 */
#ifndef _NBS_H
#define _NBS_H


#define NBS_VERSION 	1

#define NBS_SAMPLES		256				/* Multiple of 64 */
#define NBS_BITRATE	44100
#define NBS_FORMAT  NBS_FMT_S16_LE
#define NBS_CHANNELS 2
#define NBS_CHUNK_PWR	10	
#define NBS_CHUNK (1 << NBS_CHUNK_PWR)
#define NBS_BLOCKSIZE	8192			/* Default block size (in samples) */

#define NBS_FLAG_MUTE		(1 << 0)	/* Mute other streams for duration */
#define NBS_FLAG_OVERSPEAK 	(1 << 1)	/* Turn down, but do not mute other stream */
#define NBS_FLAG_OVERRIDE	(1 << 2)	/* Any local stream overrides remote streams */
#define NBS_FLAG_EMERGENCY	(1 << 3)	/* Overrides all other sounds */
#define NBS_FLAG_PRIVATE	(1 << 4)	/* Do not broadcast */

#define NBS_FMT_MU_LAW	0x00000001
#define NBS_FMT_A_LAW 	0x00000002
#define NBS_FMT_IMA_ADPCM    0x00000004
#define NBS_FMT_U8    0x00000008
#define NBS_FMT_S16_LE	0x00000010      /* Little endian signed 16*/
#define NBS_FMT_S16_BE	0x00000020      /* Big endian signed 16 */
#define NBS_FMT_S8    0x00000040
#define NBS_FMT_U16_LE	0x00000080      /* Little endian U16 */
#define NBS_FMT_U16_BE	0x00000100      /* Big endian U16 */
#define NBS_FMT_MPEG  	0x00000200      /* MPEG (2) audio */
#define NBS_FMT_AC3		0x00000400      /* Dolby Digital AC3 */


struct nbs;

typedef struct nbs NBS;

/* Create new NBS stream */
extern NBS *nbs_newstream(const char *app, const char *name, const int flags);

/* Enable/disable debug */
extern void nbs_setdebug(NBS *nbs, int debug);

/* Connect to local server using all parameters */
extern int nbs_connect(NBS *nbs);

/* Listen to local server */
extern int nbs_listen(NBS *nbs);

/* Set blocksize */
extern int nbs_setblocksize(NBS *nbs, const int blocksize);

/* Set desired bitrate */
extern void nbs_setbitrate(NBS *nbs, const int bitrate);

/* Set desired channels */
extern void nbs_setchannels(NBS *nbs, const int channels);

/* Destroy existing NBS stream */
extern void nbs_delstream(NBS *nbs);

/* Get file descriptor for NBS stream */
extern int nbs_fd(NBS *nbs);

/* Get number of bytes left before blocking */
extern int nbs_freespace(NBS *nbs);

/* Write audio data to stream */
extern int nbs_write(NBS *nbs, const void *data, const int samples);

extern struct nbsl_header *nbs_read(NBS *n, void *buf, int len);

/* Check for status on a read NBS (0 == ok, -1 == dead) */
extern int nbs_status(NBS *nbs);

/* Set to blocking or non-blocking mode */
extern int nbs_setblocking(NBS *nbs, const int blocking);
#ifdef _NBS_PRIVATE

#define PACKED __attribute__ ((__packed__))

/*
 * NBS Network protocol definitions
 */

#define NBS_PORT	4303
#define NBS_ADDR	255.255.255.255

struct nbs_header {
	unsigned char ver;			/* Protocol version */
	unsigned char ftype;		/* Frametype */
	unsigned short seqno;		/* Sequence number */
	unsigned int streamid;		/* Unique stream identifier */
	unsigned char data[0];		/* Data follows */
} PACKED;

#define NBS_FRAME_AUDIO		1	/* Little-endian 16-bit stereo audio, 
									44100 Hz */
#define NBS_FRAME_INFO		2	/* Stream information */
#define NBS_FRAME_DESTROY	3	/* Destroy stream immediately */


struct nbs_streaminfo {
	unsigned char streamname[80];
	unsigned char appname[16];
	unsigned int flags;
	unsigned short bitrate;
	unsigned short channels;
	unsigned int format;
} PACKED;

struct nbs_audio {
	short data[0];				/* Little-endian audio data, 2-channel
							    	interleaved */
};

/*
 * NBS Local protocol definitions
 */
#define NBS_DSP		"/dev/dsp"
#define NBSL_SOCKET	"/var/run/nbssocket"
#define NBSL_MAGIC	0xac1c

#define NBSL_FRAME_SETUP		1	/* Setup audio parameters */
#define NBSL_FRAME_AUDIOMODE	2	/* Proceed to unframed audio mode */
#define NBSL_FRAME_SETUPACK		3	/* Ack of audio setup */
#define NBSL_FRAME_SETUPNAK		4	/* Nak of audio setup */
#define NBSL_FRAME_AUDIOREADY	5	/* Audiomode ready */
#define NBSL_FRAME_LISTENER		6	/* Go into listener mode */
#define NBSL_FRAME_LISTENAUDIO	7	/* Listening audio */

#define NBSL_REASON_UBITRATE	1	/* Unsupported bitrate */
#define NBSL_REASON_UFORMAT		2	/* Unsupported format */
#define NBSL_REASON_UCHANNELS	3	/* Unsupported channel count */

struct nbsl_header {
	unsigned int magic;		/* Same as NBSL_MAGIC */
	unsigned short length;	/* Length of body (without nbsl_header) */
	unsigned short cmd;		/* NBSL Command (see above) */
	unsigned char data[0];	/* Where data would go */
} PACKED;

struct nbsl_setup {
	unsigned char streamname[80];
	unsigned char appname[16];
	unsigned int flags;
	unsigned int id;
	int bitrate;
	int channels;
	int format;
	int audiolen;					/* Requested length of audio */
} PACKED;

struct nbsl_nak {
	int reason;
} PACKED;

struct nbsl_message {
	struct nbsl_header h;
	union { 
		struct nbsl_setup s;
		struct nbsl_nak   n;
	} u;
} PACKED;

#endif
#endif
