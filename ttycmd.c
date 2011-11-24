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

#define STATE_NOTHING	0x00
#define STATE_BASIC	0x10
#define STATE_ORDERS	0x20
#define STATE_DANCE	0x30

static char help_str[] =
	"\tteensy-mode\n"
	"\tchange-state\n"
	"\thard-turn\n"
	"\tsoft-turn\n"
	"\tset-direction\n"
	"\tdist-center\n"
	"\tdist-left\n"
	"\tdist-right\n"
	"\tspeed\n";

static char CMD_TEENSY_MODE_STRING[] = "teensy-mode";
static char CMD_CHANGE_STATE_STRING[] = "change-state";
static char CMD_HARD_TURN_STRING[] = "hard-turn";
static char CMD_SOFT_TURN_STRING[] = "soft-turn";
static char CMD_SET_DIRECTION_STRING[] = "set-dir";
static char CMD_DIST_CENTER_STRING[] = "dist-center";
static char CMD_DIST_LEFT_STRING[] = "dist-left";
static char CMD_DIST_RIGHT_STRING[] = "dist-right";
static char CMD_SPEED_STRING[] = "speed";
static char CMD_UNKNOWN_STRING[] = "unknown";

struct command_s
{
	uint8 id;
	char* name;
};
typedef struct command_s command_t;

char* get_command_name(uint8 cmd)
{
	switch (cmd)
	{
		case CMD_TEENSY_MODE:
			return CMD_TEENSY_MODE_STRING;
			break;

                case CMD_CHANGE_STATE:
                        return CMD_CHANGE_STATE_STRING;
                        break;

                case CMD_HARD_TURN:
                        return CMD_HARD_TURN_STRING;
                        break;

                case CMD_SOFT_TURN:
                        return CMD_SOFT_TURN_STRING;
                        break;

                case CMD_SET_DIRECTION:
                        return CMD_SET_DIRECTION_STRING;
                        break;

                case CMD_DIST_CENTER:
                        return CMD_DIST_CENTER_STRING;
                        break;

                case CMD_DIST_LEFT:
                        return CMD_DIST_LEFT_STRING;
                        break;

                case CMD_DIST_RIGHT:
                        return CMD_DIST_RIGHT_STRING;
                        break;

                case CMD_SPEED:
                        return CMD_SPEED_STRING;
                        break;

		default:
			return CMD_UNKNOWN_STRING;
			break;
	}
}

void* CmdThreadProc(void* data)
{
	uint8 cmd = 0;
	uint8 value = 0;
	char command[128];

	while (1)
	{
		//printf("cmd: ");
		//scanf("%s", command);

		printf("STATE_NOTHING\n");
		cmd = CMD_CHANGE_STATE;
		value = STATE_NOTHING;
		write(tty_fd, &cmd, 1);
		write(tty_fd, &value, 1);
		sleep(5);

		printf("STATE_BASIC\n");
		cmd = CMD_CHANGE_STATE;
		value = STATE_BASIC;
		write(tty_fd, &cmd, 1);
		write(tty_fd, &value, 1);
		sleep(5);
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
		if (strcmp(argv[1], "-h") == 0)
		{
			printf("help:\n%s", help_str);
			exit(0);
		}
		else
		{
			printf("using device: %s\n", argv[1]);
			tty_dev = argv[1];
		}
	}

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
