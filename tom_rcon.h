/* tom_rcon.h: Single-header, minimal dependencies RCON server implementation
 *
 * Copyright (C) 2026 Thomas Oltmann
 *
 * WHY USE THE RCON PROTOCOL?
 *
 * RCON has become a de-facto standard in the video games industry.
 * It is found in such disparate titles as Minecraft, Team Fortress 2 (Source Engine),
 * Ark: Survival Evolved (Unreal Engine 4), and Palworld (Unity Engine).
 * Consequently, there is a large amount of software tooling readily available,
 * and many end users are already familiar with the concept.
 *
 * Notably, that is the full extent of RCONs advantages as a protocol.
 * It is an ill-designed network protocol in most aspects.
 * Passwords are transmitted over the network in plain text.
 * RCON client programs have to exploit unintended behaviour of the original
 * RCON implementation in the Source Dedicated Server (SRCDS) to function properly.
 *
 * FEATURES
 *
 * - Supports multiple active connections
 * - The password is checked using a constant-time string comparison algorithm
 * - I/O abstraction layer allows you to transport RCON over protocols other than TCP
 *
 * DEPENDENCIES
 *
 * string.h: strlen(), memcpy()
 *
 * If you did not disable the platform abstraction layer for TCP sockets,
 * then this library will additionally depend on
 * Winsock2 on Microsoft Windows systems, or
 * POSIX.1-2008 interfaces on Unixoid systems.
 *
 * THREAD SAFETY
 *
 * This library performs no multithreading or locking.
 *
 */
#ifndef _TOM_RCON_H_
#define _TOM_RCON_H_

#include <stddef.h>

#define RCON_PORT                "7023"
#define RCON_TCP_LISTEN_BACKLOG  4
#define RCON_MAX_PASSWORD_LENGTH 4086

/* 'idx' refers to the index of the open connection.
 * They are numbered starting from 1;
 * Number 0 refers to the listener socket.
 */

struct rcon_io_impl {
	int  (*cb_accept)(void *);
	int  (*cb_send)(void *, int, const void *, unsigned);
	int  (*cb_recv)(void *, int, void *, unsigned);
	/* Closes the connection with the given index.
	 * Passing an index of 0 means closing everything,
	 * including the release of any memory that is still being held.
	 */
	void (*cb_close)(void *, int);
	int  (*cb_waitany)(void *, long);
	int  (*cb_hasdata)(void *, int);
	void (*cb_strerror)(void *, int, char *, unsigned);
};

typedef struct rcon Rcon;

struct rcon_client {
	int recvd;
	int inUse;
	int authd;
	unsigned char buffer[4 + 4096];
};

struct rcon {
	struct rcon_client *clients;

	char     *(*eval)(void *userdata, const char *msg, size_t len);
	void       *userdata;
	int         maxClients;
	const char *password;

	const struct rcon_io_impl *io;
	void *iodata;
};

void rcon_tcp_init(void);
void rcon_tcp_uninit(void);
int  rcon_tcp_open(const char *hostname, const char *port, int maxClients, void **userdata);
int  rcon_tcp_accept(void *userdata);
int  rcon_tcp_send(void *userdata, int idx, const void *data, unsigned len);
int  rcon_tcp_recv(void *userdata, int idx, void *data, unsigned max);
void rcon_tcp_close(void *userdata, int idx);
int  rcon_tcp_waitany(void *userdata, long timeoutMs);
int  rcon_tcp_hasdata(void *userdata, int idx);
void rcon_tcp_strerror(void *userdata, int err, char *buf, unsigned max);

static const struct rcon_io_impl rcon_tcp_io_impl = {
	.cb_accept   = rcon_tcp_accept,
	.cb_send     = rcon_tcp_send,
	.cb_recv     = rcon_tcp_recv,
	.cb_close    = rcon_tcp_close,
	.cb_waitany  = rcon_tcp_waitany,
	.cb_hasdata  = rcon_tcp_hasdata,
	.cb_strerror = rcon_tcp_strerror,
};

void rcon_strerror(Rcon *rc, int err, char *buf, unsigned max);

int  rcon_create (Rcon *rc, const struct rcon_io_impl *io, void *iodata, int maxClients, void *userdata);
int  rcon_create_tcp(Rcon *rc, const char *hostname, int port, int maxClients, void *userdata);
void rcon_destroy(Rcon *rc);
void rcon_update (Rcon *rc, long timeoutMs);

/* Sets the server password.
 * The password is not copied and stored, RCON only stores the pointer that you pass in.
 * It is your responsibility to make sure that it is not free'd prematurely.
 * A password of NULL results in RCON not accepting any new authentication requests.
 * If the argument string is too long to be used as a password, 0 is returned,
 * and internally the password is set to NULL (meaning no authentication succeeds).
 * Returns 1 on success.
 */
int  rcon_set_password(Rcon *rc, const char *password);

#ifdef RCON_IMPLEMENTATION

/* There's multiple different error sources at play, each with their own
 * conventions for error numbers. Luckily, they are all just integers.
 * We can represent any error code in a unified way by 'boxing' them in a larger error code.
 * Boxed error codes are always negative values.
 */

// Errors related to the RCON protocol
#define RCON_ERRSRC_PROTO  0
// Errors reported by system APIs (errno or WSAGetLastError())
#define RCON_ERRSRC_SYSTEM 1
// Errors stemming from getaddrinfo()
#define RCON_ERRSRC_GAI    2

#define RCON_ERROR_CODE_SHIFT  2
#define RCON_ERROR_SOURCE_MASK ((1u<<RCON_ERROR_CODE_SHIFT)-1)
#define RCON_BOX_ERROR(src, code) (-(((code)<<RCON_ERROR_CODE_SHIFT)|(src)))

// On Unixoid systems, we need POSIX-specific interfaces that won't be visible normally.
#ifndef _WIN32
# define _POSIX_C_SOURCE 200809L
#endif

#ifdef _WIN32

# include <winsock2.h>
# include <ws2tcpip.h>

// Winsock brings its own closesocket, INVALID_SOCKET, SOCKET_ERROR

static inline int
is_benign_error(void)
{
	int err = WSAGetLastError();
	return err == WSAEINTR || err == WSAEWOULDBLOCK;
}

static inline int
get_last_error(void)
{
	return WSAGetLastError();
}

#else

# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <unistd.h>
# include <fcntl.h>
# include <netdb.h>
# include <poll.h>
# include <errno.h>

// We mimic Winsock on Unixoid systems here, since it's easier than
// the other way around.
# define closesocket close
# define INVALID_SOCKET -1
# define SOCKET_ERROR -1

static inline int
is_benign_error(void)
{
	return errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN;
}

static inline int
get_last_error(void)
{
	return errno;
}

#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define rcon_warn(...) printf(__VA_ARGS__)
#define rcon_log(...) printf(__VA_ARGS__)

#define RCON_MIN(a,b) ((a)<(b)?(a):(b))
#define RCON_MAX(a,b) ((a)>(b)?(a):(b))

#define RCON_SERVERDATA_AUTH           3
#define RCON_SERVERDATA_AUTH_RESPONSE  2
#define RCON_SERVERDATA_EXECCOMMAND    2
#define RCON_SERVERDATA_RESPONSE_VALUE 0

/* Compare a given string with a secret string,
 * without leaking through timings how close the match is,
 * or how long the secret string is.
 * Both strings must be NUL terminated.
 * The string must be at most 2^16 bytes long.
 * If both strings are the same, 1 is returned, otherwise 0.
 * If either string is NULL, 0 is returned.
 */
int
rcon_streq_consttime(const char *given, const char *secret)
{
	unsigned i = 0, j = 0;
	char r = 0;
	if (!given || !secret) {
		return 0;
	}
	for (;;) {
		r |= given[i] ^ secret[j];

		if (given[i] == '\0') {
			break;
		}
		i++;

		// Mix so thoroughly that if known[i] is not NUL,
		// then all lower 16 bits in mask will be set.
		unsigned mask = secret[j];
		mask |= mask << 8;
		mask |= mask << 4;
		mask |= mask << 2;
		mask |= mask << 1;
		mask |= mask >> 4;
		mask |= mask >> 2;
		mask |= mask >> 1;

		// Perform branchless increment with wraparound.
		j = (j + 1) & mask;
	}
	return r == 0;
}

void
rcon_strerror(Rcon *rc, int err, char *buf, unsigned max)
{
	int src  = (-err) & RCON_ERROR_SOURCE_MASK;
	int code = (-err) >> RCON_ERROR_CODE_SHIFT;
	switch (src) {
	case RCON_ERRSRC_PROTO:
		// TODO
		(void)code;
		break;
	default:
		rc->io->cb_strerror(rc->iodata, err, buf, max);
		break;
	}
}

int
rcon_create(Rcon *rc, const struct rcon_io_impl *io, void *iodata, int maxClients, void *userdata)
{
	rc->maxClients = maxClients;
	rc->clients = calloc(rc->maxClients, sizeof *rc->clients);
	if (!rc->clients) {
		return RCON_BOX_ERROR(RCON_ERRSRC_PROTO, 1); // TODO
	}
	rc->userdata = userdata;
	rc->io = io;
	rc->iodata = iodata;
	return 0;
}

int
rcon_create_tcp(Rcon *rc, const char *hostname, int port, int maxClients, void *userdata)
{
	void *iodata;
	int s;
	s = rcon_tcp_open(hostname, RCON_PORT, maxClients, &iodata);
	if (s < 0) return s;
	s = rcon_create(rc, &rcon_tcp_io_impl, iodata, maxClients, userdata);
	if (s < 0) {
		rcon_tcp_io_impl.cb_close(iodata, 0);
	}
	return s;
}

void
rcon_destroy(Rcon *rc)
{
	rc->io->cb_close(rc->iodata, 0);
	free(rc->clients);
}

static inline void
rcon_write_i32(unsigned char *b, int32_t v)
{
	b[0] = v;
	b[1] = v >> 8;
	b[2] = v >> 16;
	b[3] = v >> 24;
}

static inline int32_t
rcon_read_i32(const unsigned char *b)
{
	return (int32_t)b[0] | (int32_t)b[1] << 8 |
		(int32_t)b[2] << 16 | (int32_t)b[3] << 24;
}

static void
rcon_send(Rcon *rc, int cidx, int32_t rid, int32_t type, const unsigned char *payload, int length)
{
	unsigned char packet[4+4096];
	do {
		// Split into fragments
		int fragPayLen = RCON_MIN(4096 - 10, length);
		rcon_write_i32(packet + 0, fragPayLen + 10);
		rcon_write_i32(packet + 4, rid);
		rcon_write_i32(packet + 8, type);
		memcpy(packet + 12, payload, fragPayLen);
		packet[12 + fragPayLen] = 0;
		packet[12 + fragPayLen + 1] = 0;
		rc->io->cb_send(rc->iodata, cidx, packet, 4 + 10 + fragPayLen);
		payload += fragPayLen;
		length -= fragPayLen;
	} while (length);
}

static int
rcon_process(Rcon *rc, int cidx, int pktLen, unsigned char *packet)
{
	if (pktLen < 10) return -1;
	int32_t rid = rcon_read_i32(packet);
	int32_t type = rcon_read_i32(packet + 4);
	char *payload = (char *)(packet + 8);
	int payLen = pktLen - 10;
	// FIXME make sure that payload is NUL-terminated!
	switch (type) {
	case RCON_SERVERDATA_AUTH:
		{
			// Weird nonsensical packet that is sent by SRCDS
			rcon_send(rc, cidx, rid, RCON_SERVERDATA_RESPONSE_VALUE, NULL, 0);

			int matches = rcon_streq_consttime(payload, rc->password);
			rcon_send(rc, cidx, matches ? rid : -1,
				RCON_SERVERDATA_AUTH_RESPONSE, NULL, 0);
			// TODO store auth
		}
		break;

	case RCON_SERVERDATA_EXECCOMMAND:
		{
			// TODO check auth
			char *result = rc->eval(rc->userdata, payload, payLen);
			rcon_send(rc, cidx, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)result, (int)strlen(result));
			free(result);
		}
		break;

	default:
		// Try to emulate weird SRCDS behaviour
		rcon_send(rc, cidx, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)payload, payLen);
		rcon_send(rc, cidx, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)"\1\0", 2);
		break;
	}
	return 0;
}

void
rcon_update(Rcon *rc, long timeoutMs)
{
	if (rc->io->cb_waitany(rc->iodata, timeoutMs) <= 0) return;
	for (int i = 0; i < rc->maxClients; i++) {
		struct rcon_client *client = &rc->clients[i];
		int cidx = i + 1;
		if (!client->inUse) continue;
		if (!rc->io->cb_hasdata(rc->iodata, cidx)) continue;
		int drop = 0;
		// TODO proper handling of huge input
		int got = rc->io->cb_recv(rc->iodata, cidx,
			client->buffer + client->recvd,
			sizeof client->buffer - client->recvd);
		if (got == 0) {
			drop = 1;
		} else if (got < 0) {
			rcon_warn("TCP socket error");
			drop = 1;
		} else {
			client->recvd += got;
			for (;;) {
				if (client->recvd < 4) break;
				int pktLen = rcon_read_i32(client->buffer);
				if (client->recvd < 4 + pktLen) break;
				if (rcon_process(rc, cidx, pktLen, client->buffer + 4) < 0) {
					drop = 1;
					break;
				}
				client->recvd -= 4 + pktLen;
				memmove(client->buffer, client->buffer + 4 + pktLen, client->recvd);
			}
		}
		if (drop) {
			rcon_log("Dropping RCON client.");
			rc->io->cb_close(rc->iodata, cidx);
			client->inUse = 0;
		}
	}
	if (rc->io->cb_hasdata(rc->iodata, 0)) {
		int newidx = rc->io->cb_accept(rc->iodata);
		if (newidx) {
			int i = newidx - 1;
			rcon_log("Got a new RCON client.");
			rc->clients[i].recvd = 0;
			rc->clients[i].inUse = 1;
		}
	}
}

int
rcon_set_password(Rcon *rc, const char *password)
{
	if (password && strlen(password) > RCON_MAX_PASSWORD_LENGTH) {
		rc->password = NULL;
		return 0;
	}
	rc->password = password;
	return 1;
}

static char *
rcon_port_to_string(int port, char *buf, int max)
{
	char *p = buf + max;
	*--p = 0;
	do {
		*--p = (port % 10) + '0';
		port /= 10;
	} while (port);
	return p;
}

/* ---- Global Initialization (Windows only) ---- */

struct rcon_tcp_block {
	int nfds;
	struct pollfd pfds[];
};

void
rcon_tcp_init(void)
{
#ifdef _WIN32
	WSADATA wsaData;
	// Most recent version of Winsock at time of writing
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		rcon_warn("Unable to initialize Winsock library.");
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		rcon_warn("Version 2.2 of Winsock is not available.");
		WSACleanup();
		return;
	}
#endif
}

void
rcon_tcp_uninit(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

int
rcon_tcp_open(const char *hostname, const char *port, int maxClients, void **userdata)
{
	// Find contender addresses via getaddrinfo()
	struct addrinfo *ai, hints = {
		.ai_flags    = AI_NUMERICSERV | AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	int s = getaddrinfo(hostname, port, &hints, &ai);
	if (s) {
#ifdef _WIN32
		// gai_strerror() is not thread-safe on Windows.
		// WSAGetLastError() can be used instead.
		return RCON_BOX_ERROR(RCON_ERRSRC_SYSTEM, get_last_error());
#else
		return RCON_BOX_ERROR(RCON_ERRSRC_GAI, -s);
#endif
	}

	// Loop through all the results and bind to the first we can
	struct addrinfo *p;
	for (p = ai; p; p = p->ai_next) {
		// Create socket file descriptor
		int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (fd == INVALID_SOCKET) continue;

		// Re-use address
#ifndef _WIN32
		const int iyes = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &iyes, sizeof(int));
#endif
		
		// Bind to the address
		if (bind(fd, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR) {
			closesocket(fd);
			continue;
		}

		// Listen for incoming connections
		if (listen(fd, RCON_TCP_LISTEN_BACKLOG) == SOCKET_ERROR) {
			closesocket(fd);
			continue;
		}

		// Set socket to non-blocking mode
#ifdef _WIN32
		u_long ulyes = 1;
		ioctlsocket(fd, FIONBIO, &ulyes);
#else
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

		freeaddrinfo(ai);

		// Success! Construct a pollfd array and return it.
		struct rcon_tcp_block *tcp = calloc(sizeof *tcp +
			(1 + maxClients) * sizeof *tcp->pfds, 1);
		tcp->nfds = 1 + maxClients;
		for (int idx = 0; idx < tcp->nfds; idx++) {
			tcp->pfds[idx].fd = -1;
			tcp->pfds[idx].events = POLLIN;
		}
		tcp->pfds[0].fd = fd;
		*userdata = tcp;
		return 0;
	}

	freeaddrinfo(ai);
	return RCON_BOX_ERROR(RCON_ERRSRC_PROTO, 1); // TODO
}

int
rcon_tcp_accept(void *userdata)
{
	struct rcon_tcp_block *tcp = userdata;
	int idx, fd;

	fd = accept(tcp->pfds[0].fd, NULL, NULL);
	if (fd < 0) {
		return RCON_BOX_ERROR(RCON_ERRSRC_SYSTEM, get_last_error());
	}

	for (idx = 1; idx < tcp->nfds; idx++) {
		if (tcp->pfds[idx].fd < 0) {
			tcp->pfds[idx].fd = fd;
			return idx;
		}
	}
	
	closesocket(fd);
	return RCON_BOX_ERROR(RCON_ERRSRC_PROTO, 1); // TODO
}

int
rcon_tcp_send(void *userdata, int idx, const void *data, unsigned len)
{
	struct rcon_tcp_block *tcp = userdata;
	int fd = tcp->pfds[idx].fd;
	const unsigned char *uchars = data;
	unsigned sent = 0;
	while (sent < len) {
		int s = (int)send(fd, uchars + sent, len - sent, 0);
		if (s == SOCKET_ERROR) {
			if (is_benign_error()) continue;
			else return RCON_BOX_ERROR(RCON_ERRSRC_SYSTEM, get_last_error());
		}
		sent += s;
	}
	return sent;
}

int
rcon_tcp_recv(void *userdata, int idx, void *data, unsigned max)
{
	struct rcon_tcp_block *tcp = userdata;
	int fd = tcp->pfds[idx].fd;
	for (;;) {
		int s = (int)recv(fd, data, max, 0);
		if (s == SOCKET_ERROR) {
			if (is_benign_error()) continue;
			else return RCON_BOX_ERROR(RCON_ERRSRC_SYSTEM, get_last_error());
		}
		return s;
	}
}

void
rcon_tcp_close(void *userdata, int idx)
{
	struct rcon_tcp_block *tcp = userdata;
	if (idx == 0) {
		for (int i = 1; i < tcp->nfds; i++) {
			closesocket(tcp->pfds[i].fd);
		}
		closesocket(tcp->pfds[0].fd);
		free(tcp);
	} else {
		closesocket(tcp->pfds[idx].fd);
		tcp->pfds[idx].fd = -1;
	}
}

int
rcon_tcp_waitany(void *userdata, long timeoutMs)
{
	struct rcon_tcp_block *tcp = userdata;
	int n = poll(tcp->pfds, tcp->nfds, timeoutMs);
	if (n < 0) {
		return RCON_BOX_ERROR(RCON_ERRSRC_SYSTEM, get_last_error());
	} else {
		return n;
	}
}

int
rcon_tcp_hasdata(void *userdata, int idx)
{
	struct rcon_tcp_block *tcp = userdata;
	return !!(tcp->pfds[idx].revents & POLLIN);
}

void
rcon_tcp_strerror(void *userdata, int err, char *buf, unsigned max)
{
	(void)userdata;
	const char *p;
	size_t len;
	int src  = (-err) & RCON_ERROR_SOURCE_MASK;
	int code = (-err) >> RCON_ERROR_CODE_SHIFT;
	switch (src) {
	case RCON_ERRSRC_SYSTEM:
		// Assuming XSI-compliant strerror_r()
		strerror_r(code, buf, max);
		break;
	case RCON_ERRSRC_GAI:
		p = gai_strerror(-code);
		len = strlen(p) + 1;
		len = RCON_MIN(len, max);
		memcpy(buf, p, len);
		break;
	default:
		if (max) buf[0] = 0;
		break;
	}
}

#endif
#endif
