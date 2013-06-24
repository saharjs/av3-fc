/*
 * gps.c
 *
 *  Created on: Jun 22, 2013
 *      Author: theo
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/poll.h>

#include "fcfutils.h"
#include "psas_packet.h"
#include "gps.h"

// cf 7.1  Crescent Integrators Manual

struct msg {
	char 		magic[4];		// "$BIN"
	uint16_t	type;			//  1,  2, 80, 93-99
	uint16_t	len;			// 52, 16, 40, 56, 96, 128, 300, 28, 68, 304 
	union {
		char raw[304];
		struct msg1 m1;
		struct msg99 m99;
		// define more of them if we ever need any
	};
};


static int handle_msg(const char *id, const void *data, uint16_t len)
{
	// TODO: real timestamp
	GPS_packet p = { .timestamp={0,0,0,0,0,0}, .data_length=htons(len) };
	memcpy(&p.ID, id, 4);
	memcpy(&p.raw, data, len);
	sendGPSData(&p);
	return 0;
}

static int handle_packet(struct msg *m)
{
	switch (m->type)
	{
	case 1:
		if (m->len == sizeof(struct msg1))
			return handle_msg("GPS1", &m->m1, m->len);
		fprintf(stderr, "bad length %u != %lu\n", m->len, sizeof(struct msg1));
		return -3;
	case 99:
		if (m->len == sizeof(struct msg99))
			return handle_msg("GP99", &m->m99, m->len);
		fprintf(stderr, "bad length %u != %lu\n", m->len, sizeof(struct msg99));
		return -3;
	case 2:
	case 80:
	case 93: case 94: case 95: case 96: case 97: case 98:
		// log all GPS packets for groundside analysis
		return handle_msg("GPSX", &m->raw, m->len);
	default:
		fprintf(stderr, "unknown GPS packet type: %u length %u\n", m->type, m->len);
		return -4;
	}
}

static unsigned char buffer[4096], *end = buffer;

static uint16_t sum(uint8_t *packet, int len)
{
	uint16_t s = 0;
	while (len--)
		s += *packet++;
	return s;
}

// return 0 if could parse a good packet out of the data within [buffer-end)
static int get_packet(struct msg *m)
{
	unsigned char *p = buffer;
	uint16_t checksum;

	while ((p = memchr(p, '$', end - p)) != NULL)
	{
		// sync stream to "$BIN"
		if (memcmp(p, "$BIN", 4) != 0) {
			p++;
			continue;
		}
		memcpy(m, p, 8);
		if (m->type > 99 || m->len > 304) {
			fprintf(stderr, "bad header\n");
			p += 4;
			continue;
		}
		if (8 + m->len + 4 < end - p) {
			// incomplete, need more data
			return 0;
		}
		memcpy(&checksum, p + 8 + m->len, 2);
		if (sum(p + 8, m->len) == checksum) {
			// good packet: advance and return success
			memcpy(m->raw, p + 8, m->len);

			p += 8 + m->len + 4;			// header + data + checksum + \r\n
			memmove(buffer, p, end - p);
			end = buffer + (end - p);
			return 1;
		}
		fprintf(stderr, "bad checksum %u (expected %u)\n", sum(p + 8, m->len), checksum);
		p += 4;
	}
	end = buffer;
	return 0;
}

int fd;
static void data_callback(struct pollfd *pfd){
	int act_len;
	struct msg m;

	act_len = read(fd, end, end-buffer+sizeof(buffer));
	if (act_len <= 0) {
		perror("read from GPS device failed");
		return;
	}
	end += act_len;

	while (get_packet(&m))
		handle_packet(&m);
}

void gps_init() {
	fd = open("/dev/ttyusb0", O_RDONLY);
	if (fd < 0)
		perror("Can't open GPS device");
	else
		fcf_add_fd(fd, POLLIN, data_callback);
}

void gps_final() {
	close(fd);
}