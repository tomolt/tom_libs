#include <stdbool.h>

#define RCON_IMPLEMENTATION
#include <tom_rcon.h>

Rcon *rcon;
bool running;

static char *
eval_command(void *userdata, const char *cmd, size_t length)
{
	(void)userdata;
	(void)length;
	printf("RECEIVED: %s\n", cmd);
	char *response = calloc(1, 1024);
	if (!strcmp(cmd, "quit")) {
		strcpy(response, "bye.");
		running = false;
	} else {
		strcpy(response, "???");
	}
	return response;
}

int
main()
{
	rcon_socket_init();
	rcon = rcon_create(NULL);
	rcon->eval = eval_command;

	running = true;
	while (running) {
		rcon_update(rcon, -1);
	}

	rcon_destroy(rcon);
	rcon_socket_uninit();
	return 0;
}
