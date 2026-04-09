#include <stdbool.h>

#define RCON_IMPLEMENTATION
#include <tom_rcon.h>

Rcon rcon;
bool running;

static char *
eval_command(const char *cmd)
{
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
	rcon_tcp_init();
	rcon_create_tcp(&rcon, "0.0.0.0", 7023, 2, NULL);
	rcon_set_password(&rcon, "hunter2");

	running = true;
	while (running) {
		rcon_wait_for_input(&rcon, -1);
		const char *cmd;
		int s;
		while ((s = rcon_fetch_command(&rcon, &cmd))) {
			char *response = eval_command(cmd);
			rcon_complete_command(&rcon, s, response);
			free(response);
		}
	}

	rcon_destroy(&rcon);
	rcon_tcp_uninit();
	return 0;
}
