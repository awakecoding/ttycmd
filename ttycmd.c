#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

typedef unsigned char uint8;
typedef unsigned short uint16;

static int tty_fd;
static pthread_t cmd_thread;
static pthread_t comm_thread;
static void* cmd_thread_status;
static void* comm_thread_status;

char default_tty_dev[] = "/dev/ttyACM0";

#define CMD_TEENSY_MODE		(0x80 | 0x01)
#define CMD_CHANGE_STATE	(0x80 | 0x02)
#define CMD_HARD_TURN		(0x80 | 0x11)
#define CMD_SOFT_TURN		(0x80 | 0x12)
#define CMD_SET_DIRECTION	(0x80 | 0x13)
#define CMD_DIST_CENTER		(0x80 | 0x21)
#define CMD_DIST_LEFT		(0x80 | 0x22)
#define CMD_DIST_RIGHT		(0x80 | 0x23)
#define CMD_SPEED		(0x80 | 0x31)
#define CMD_HELP		(0x00 | 0x01)
#define CMD_QUIT		(0x00 | 0x02)
#define CMD_UNKNOWN		(0x80 | 0x00)

#define STATE_NOTHING	0x00
#define STATE_BASIC	0x10
#define STATE_ORDERS	0x20
#define STATE_DANCE	0x30
#define STATE_UNKNOWN	0xFF

struct state_s
{
	uint8 id;
	char* name;
};
typedef struct state_s state_t;

static state_t states[] =
{
	{ STATE_NOTHING, "nothing" },
	{ STATE_BASIC, "basic" },
	{ STATE_ORDERS, "orders" },
	{ STATE_DANCE, "dance" },
	{ STATE_UNKNOWN, "" }
};

struct command_s
{
	uint8 id;
	char* name;
};
typedef struct command_s command_t;

static command_t commands[] =
{
	{ CMD_TEENSY_MODE, "mode" },
	{ CMD_CHANGE_STATE, "state" },
	{ CMD_HARD_TURN, "hard-turn" },
	{ CMD_SOFT_TURN, "soft-turn" },
	{ CMD_SET_DIRECTION, "set-direction" },
	{ CMD_DIST_CENTER, "dist-center" },
	{ CMD_DIST_LEFT, "dist-left" },
	{ CMD_DIST_RIGHT, "dist-right" },
	{ CMD_SPEED, "speed" },
	{ CMD_HELP, "help" },
	{ CMD_QUIT, "quit" },
	{ CMD_UNKNOWN, "" }
};

void print_command_list()
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(command_t); i++)
	{
		printf("\t%s\n", commands[i].name);
	}
}

char* get_command_name(uint8 cmd_id)
{
	int i;

	for (i = 0; i < sizeof(commands) / sizeof(command_t); i++)
	{
		if (commands[i].id == cmd_id)
			return commands[i].name;
	}

	return commands[i].name;
}

uint8 get_command_id(char* cmd_name)
{
	int i;

	if (cmd_name == NULL)
		return CMD_UNKNOWN;

	for (i = 0; i < sizeof(commands) / sizeof(command_t); i++)
	{
		if (strcmp(commands[i].name, cmd_name) == 0)
			return commands[i].id;
	}

	return CMD_UNKNOWN;
}

char* get_state_name(uint8 state_id)
{
	int i;

	for (i = 0; i < sizeof(states) / sizeof(state_t); i++)
	{
		if (states[i].id == state_id)
			return states[i].name;
	}

	return states[i].name;
}

uint8 get_state_id(char* state_name)
{
	int i;

	if (state_name == NULL)
		return STATE_UNKNOWN;

	for (i = 0; i < sizeof(states) / sizeof(state_t); i++)
	{
		if (strcmp(states[i].name, state_name) == 0)
			return states[i].id;
	}

	return STATE_UNKNOWN;
}

void send_command(int fd, uint8 cmd, uint8 val)
{
	write(fd, &cmd, 1);
	write(fd, &val, 1);	
}

void* CmdThreadProc(void* data)
{
	char* p;
	uint8 state;
	uint8 val = 0;
	uint8 cmd = 0;
	char* cmd_str;
	char* val_str;
	char input[128];

	while (1)
	{
		printf("cmd: ");
		scanf("%s", input);

		p = strchr(input, ':');
		val_str = cmd_str = NULL;

		if (p != NULL)
		{
			val_str = p + 1;
			input[(p - input)] = '\0';
		}

		cmd_str = input;

		cmd = get_command_id(cmd_str);

		if (cmd == CMD_UNKNOWN)
		{
			printf("unknown command!\n");
			continue;
		}

		switch (cmd)
		{
			case CMD_TEENSY_MODE:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_CHANGE_STATE:
				state = get_state_id(val_str);
				
				if (state == STATE_UNKNOWN)
				{
					printf("unknown state!\n");
					continue;
				}

				send_command(tty_fd, cmd, state);
				break;

			case CMD_HARD_TURN:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_SOFT_TURN:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_SET_DIRECTION:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_DIST_CENTER:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_DIST_LEFT:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_DIST_RIGHT:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_SPEED:
				send_command(tty_fd, cmd, 0);
				break;

			case CMD_HELP:
				val = get_command_id(val_str);

				if (val == CMD_UNKNOWN)
				{
					printf("command syntax: <command>:<value>\n");
					print_command_list();
				}
				else
				{
					switch (val)
					{
						case CMD_CHANGE_STATE:
							printf("state:<state>, where <state> is one of the following:\n");
							printf("nothing, basic, orders, dance.\n");
							break;

						default:
							printf("command syntax: <command>:<value>\n");
							print_command_list();
							break;
					}
				}

				break;

			case CMD_QUIT:
				exit(0);
				break;
		}
	}

	pthread_exit(NULL);
}

void* CommThreadProc(void* data)
{
        uint8 comm = 0;

        while (0)
        {
                if (read(tty_fd, &comm, 1) > 0)
                {
                        printf("0x%02X\n", comm);
                }
        }

        pthread_exit(NULL);
}

int main(int argc, char** argv)
{
	fd_set rdset;
	char* tty_dev;
	struct termios tio;
	uint8 b = 0;

	tty_dev = default_tty_dev;

	if (argc > 1)
	{
		printf("using device: %s\n", argv[1]);
		tty_dev = argv[1];
	}

	printf("command syntax: <command>:<value>\n");
	print_command_list();

	memset(&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_cflag = CS8 | CREAD | CLOCAL; /* 8n1, see termios.h for more information */
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 5;

	tty_fd = open(tty_dev, O_RDWR | O_NONBLOCK);
	cfsetospeed(&tio, B9600); /* baud */
	cfsetispeed(&tio, B9600); /* baud */

	tcsetattr(tty_fd, TCSANOW, &tio);

	pthread_create(&cmd_thread, NULL, CmdThreadProc, NULL);
	pthread_create(&comm_thread, NULL, CommThreadProc, NULL);

	pthread_join(cmd_thread, cmd_thread_status);
	pthread_join(comm_thread, &comm_thread_status);

	close(tty_fd);

	return 0;
}
