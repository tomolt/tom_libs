#ifndef _TOM_RCON_H_
#define _TOM_RCON_H_

#define RCON_MAX_IP_ADDR_SIZE 128
#define RCON_PORT        7023
#define RCON_MAX_CLIENTS 4

typedef struct rcon_tcp_socket   RCON_TcpSocket;
typedef struct rcon_socket_strip RCON_SocketStrip;

typedef struct rcon Rcon;

void rcon_socket_init(void);
void rcon_socket_uninit(void);

RCON_TcpSocket *rcon_tcp_listen (const char *hostname, int port);
RCON_TcpSocket *rcon_tcp_accept(RCON_TcpSocket *sock);

int  rcon_tcp_send (RCON_TcpSocket *sock, const void *buf, int len);
int  rcon_tcp_recv (RCON_TcpSocket *sock, void *buf, int len);
void rcon_tcp_close(RCON_TcpSocket *sock);

RCON_SocketStrip *rcon_socket_strip_alloc(void);
int  rcon_socket_strip_check (RCON_SocketStrip *strip, unsigned timeoutMs);
int  rcon_socket_strip_add   (RCON_SocketStrip *strip, RCON_TcpSocket *sock);
int  rcon_socket_strip_remove(RCON_SocketStrip *strip, RCON_TcpSocket *sock);
void rcon_socket_strip_free  (RCON_SocketStrip *strip);
int  rcon_socket_strip_ready (RCON_SocketStrip *strip, RCON_TcpSocket *sock);

Rcon *rcon_create (void *cmdState);
void  rcon_destroy(Rcon *rc);
void  rcon_update (Rcon *rc);

#ifdef RCON_IMPLEMENTATION

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define rcon_warn(...) printf(__VA_ARGS__)
#define rcon_log(...) printf(__VA_ARGS__)

#define RCON_MIN(a,b) ((a)<(b)?(a):(b))
#define RCON_MAX(a,b) ((a)>(b)?(a):(b))

#define RCON_SERVERDATA_AUTH           3
#define RCON_SERVERDATA_AUTH_RESPONSE  2
#define RCON_SERVERDATA_EXECCOMMAND    2
#define RCON_SERVERDATA_RESPONSE_VALUE 0

struct rcon_client {
	RCON_TcpSocket *socket;
	int recvd;
	int inUse;
	unsigned char buffer[4 + 4096];
};

struct rcon {
	RCON_SocketStrip  *strip;
	RCON_TcpSocket    *server;
	struct rcon_client clients[RCON_MAX_CLIENTS];
	char            *(*eval)(void *userdata, const char *msg, size_t len);
	void              *cmdState;
};

Rcon *
rcon_create(void *cmdState)
{
	Rcon *rc = calloc(1, sizeof *rc);
	if (!rc) return NULL;
	rc->cmdState = cmdState;
	rc->strip = rcon_socket_strip_alloc();
	rc->server = rcon_tcp_listen("0.0.0.0", RCON_PORT);
	rcon_socket_strip_add(rc->strip, rc->server);
	return rc;
}

void
rcon_destroy(Rcon *rc)
{
	if (!rc) return;
	rcon_socket_strip_free(rc->strip);
	for (int i = 0; i < RCON_MAX_CLIENTS; i++) {
		rcon_tcp_close(rc->clients[i].socket);
		rc->clients[i].inUse = 0;
	}
	rcon_tcp_close(rc->server);
	free(rc);
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
rcon_send(struct rcon_client *client, int32_t rid, int32_t type, const unsigned char *payload, int length)
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
		//int sent = 0;
		//while ((sent += SDLNet_TCP_Send(client->socket, packet + sent, 4 + 10 + fragPayLen - sent)) < fragPayLen) {}
		rcon_tcp_send(client->socket, packet, 4 + 10 + fragPayLen);
		payload += fragPayLen;
		length -= fragPayLen;
	} while (length);
}

static int
rcon_process(Rcon *rc, struct rcon_client *client, int pktLen, unsigned char *packet)
{
	if (pktLen < 10) return -1;
	int32_t rid = rcon_read_i32(packet);
	int32_t type = rcon_read_i32(packet + 4);
	char *payload = (char *)(packet + 8);
	int payLen = pktLen - 10;
	switch (type) {
	case RCON_SERVERDATA_AUTH:
		// Weird nonsensical packet that is sent by SRCDS
		rcon_send(client, rid, RCON_SERVERDATA_RESPONSE_VALUE, NULL, 0);
		// Failure
		//rcon_send(client, -1, RCON_SERVERDATA_AUTH_RESPONSE, NULL, 0);
		// Success
		rcon_send(client, rid, RCON_SERVERDATA_AUTH_RESPONSE, NULL, 0);
		break;

	case RCON_SERVERDATA_EXECCOMMAND:
		{
			char *result = rc->eval(rc->cmdState, payload, payLen);
			rcon_send(client, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)result, (int)strlen(result));
			free(result);
		}
		break;

	default:
		// Try to emulate weird SRCDS behaviour
		rcon_send(client, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)payload, payLen);
		rcon_send(client, rid, RCON_SERVERDATA_RESPONSE_VALUE,
				(const unsigned char *)"\1\0", 2);
		break;
	}
	return 0;
}

void
rcon_update(Rcon *rc)
{
	if (rcon_socket_strip_check(rc->strip, 0) <= 0) return;
	for (int i = 0; i < RCON_MAX_CLIENTS; i++) {
		struct rcon_client *client = &rc->clients[i];
		if (!client->inUse) continue;
		if (!rcon_socket_strip_ready(rc->strip, client->socket)) continue;
		int drop = 0;
		// TODO proper handling of huge input
		int got = rcon_tcp_recv(client->socket,
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
				if (rcon_process(rc, client, pktLen, client->buffer + 4) < 0) {
					drop = 1;
					break;
				}
				client->recvd -= 4 + pktLen;
				memmove(client->buffer, client->buffer + 4 + pktLen, client->recvd);
			}
		}
		if (drop) {
			rcon_log("Dropping RCON client.");
			rcon_socket_strip_remove(rc->strip, client->socket);
			rcon_tcp_close(client->socket);
			client->inUse = 0;
		}
	}
	if (rcon_socket_strip_ready(rc->strip, rc->server)) {
		RCON_TcpSocket *socket = rcon_tcp_accept(rc->server);
		if (socket) {
			int i;
			for (i = 0; i < RCON_MAX_CLIENTS; i++)
				if (!rc->clients[i].inUse) break;
			if (i < RCON_MAX_CLIENTS) {
				rcon_log("Got a new RCON client.");
				rc->clients[i].socket = socket;
				rc->clients[i].recvd = 0;
				rc->clients[i].inUse = 1;
				rcon_socket_strip_add(rc->strip, socket);
			} else {
				rcon_tcp_close(socket);
			}
		}
	}
}

// On Unixoid systems, we need POSIX-specific interfaces that won't be visible normally.
#ifndef _WIN32
# define _POSIX_C_SOURCE 200809L
#endif

// On Windows: bump up max number of sockets per select() call.
// On any system: Use this number as limit for socket strip size.
#define FD_SETSIZE 128

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

#else

# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <unistd.h>
# include <fcntl.h>
# include <netdb.h>
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

#endif

static char *
port2str(int port, char *buf, int max)
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

void
rcon_socket_init(void)
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
rcon_socket_uninit(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

/* ---- TCP Sockets ---- */

struct rcon_tcp_socket {
	int fd;
	struct sockaddr_storage myAddr;
	socklen_t myAddrLen;
};

static RCON_TcpSocket *
make_tcp_socket(int fd, const void *addr, socklen_t addrLen)
{
	if (fd == INVALID_SOCKET) return NULL;
	
	// Allocate & populate socket struct
	RCON_TcpSocket *sock = calloc(1, sizeof *sock);
	if (!sock) {
		closesocket(fd);
		return NULL;
	}
	sock->fd = fd;
	memcpy(&sock->myAddr, addr, addrLen);
	sock->myAddrLen = addrLen;
	return sock;
}

RCON_TcpSocket *
rcon_tcp_listen(const char *hostname, int port)
{
	char portbuf[10], *portstr;
	portstr = port2str(port, portbuf, 10);

	// Find contender addresses via getaddrinfo()
	struct addrinfo *ai, hints = {
		.ai_flags    = AI_NUMERICSERV | AI_PASSIVE,
		.ai_family   = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	int s = getaddrinfo(hostname, portstr, &hints, &ai);
	if (s) {
		rcon_warn("getaddrinfo: %s", gai_strerror(s));
		return NULL;
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
		if (listen(fd, 4) == SOCKET_ERROR) {
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

		// Success! Construct a socket struct and return it.
		RCON_TcpSocket *sock = make_tcp_socket(fd, &p->ai_addr, p->ai_addrlen);
		freeaddrinfo(ai);
		return sock;
	}

	rcon_warn("Unable to listen on TCP address '%s' port %d.", hostname, port);
	freeaddrinfo(ai);
	return NULL;
}

RCON_TcpSocket *
rcon_tcp_accept(RCON_TcpSocket *sock)
{
	// Accept a new file descriptor
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof addr;
	int fd = accept(sock->fd, (struct sockaddr *)&addr, &addrlen);

	// Re-enable blocking mode for socket
	// (It has inherited non-blocking mode from the listening socket)
#ifdef _WIN32
	u_long ulno = 0;
	ioctlsocket(fd, FIONBIO, &ulno);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif

	return make_tcp_socket(fd, &addr, addrlen);
}

int
rcon_tcp_send(RCON_TcpSocket *sock, const void *buf, int len)
{
	const unsigned char *uchars = buf;
	int sent = 0;
	while (sent < len) {
		int s = (int)send(sock->fd, uchars + sent, len - sent, 0);
		if (s == SOCKET_ERROR) {
			if (is_benign_error()) continue;
			else return -1;
		}
		sent += s;
	}
	return sent;
}

int
rcon_tcp_recv(RCON_TcpSocket *sock, void *buf, int len)
{
	for (;;) {
		int s = (int)recv(sock->fd, buf, len, 0);
		if (s == SOCKET_ERROR) {
			if (is_benign_error()) continue;
			else return -1;
		}
		return s;
	}
}

void
rcon_tcp_close(RCON_TcpSocket *sock)
{
	if (!sock) return;
	closesocket(sock->fd);
	free(sock);
}

/* ---- Socket Strips ---- */

struct rcon_socket_strip {
	int fds[FD_SETSIZE];
	fd_set rfds;
	int count;
};

RCON_SocketStrip *
rcon_socket_strip_alloc(void)
{
	RCON_SocketStrip *strip = calloc(1, sizeof *strip);
	FD_ZERO(&strip->rfds);
	return strip;
}

void
rcon_socket_strip_free(RCON_SocketStrip *strip)
{
	free(strip);
}

int
rcon_socket_strip_check(RCON_SocketStrip *strip, unsigned timeoutMs)
{
	int fdlimit = 0;

	// Reset read file descriptor set
	FD_ZERO(&strip->rfds);
	for (int i = 0; i < strip->count; i++) {
		FD_SET(strip->fds[i], &strip->rfds);
		fdlimit = RCON_MAX(fdlimit, strip->fds[i] + 1);
	}

	// Fill out time-out struct
	struct timeval tv;
	tv.tv_sec  = timeoutMs / 1000;
	tv.tv_usec = (timeoutMs % 1000) * 1000;
	
	// Perform select() syscall
	int s;
	do {
		s = select(fdlimit, &strip->rfds, NULL, NULL, &tv);
	} while (s < 0 && is_benign_error());
	return s;
}

int
rcon_socket_strip_add(RCON_SocketStrip *strip, RCON_TcpSocket *sock)
{
	if (strip->count == FD_SETSIZE) return -1;
	strip->fds[strip->count++] = sock->fd;
	FD_SET(sock->fd, &strip->rfds);
	return 0;
}

int
rcon_socket_strip_remove(RCON_SocketStrip *strip, RCON_TcpSocket *sock)
{
	// Find the sockets file descriptor in the strip
	int i;
	for (i = 0; i < strip->count; i++) {
		if (strip->fds[i] == sock->fd) break;
	}
	if (i == strip->count) return -1;

	strip->fds[i] = strip->fds[--strip->count];
	FD_CLR(sock->fd, &strip->rfds);
	return 0;
}

int
rcon_socket_strip_ready(RCON_SocketStrip *strip, RCON_TcpSocket *sock)
{
	return FD_ISSET(sock->fd, &strip->rfds);
}

#endif
#endif
