#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#include "cv.h"
#include "highgui.h"

typedef unsigned char uint8;
typedef unsigned short uint16;

static int tty_fd;
static pthread_t cmd_thread;
static pthread_t comm_thread;
static pthread_t intel_thread;
static pthread_t bt_thread;
static pthread_t camera_thread;
static void* cmd_thread_status;
static void* comm_thread_status;
static void* intel_thread_status;
static void* bt_thread_status;
static void* camera_thread_status;

char default_tty_dev[] = "/dev/ttyACM0";

#define NELEMENTS(_array)	(sizeof(_array) / sizeof(_array[0]))

// Camera stuff
#define DEBUGMODE
#define QUALIFY_THRESHOLD (83)
#define VICTORY_THRESHOLD (95)
#define DIRECTION_THRESHOLD (59) 

/* these are mainly  for bluetooth output */
int GLOBAL_SPEED = 0; 
int GLOBAL_MODE = 0; 
int GLOBAL_SENSOR_RIGHT = 0;
int GLOBAL_SENSOR_LEFT = 0; 
int GLOBAL_SENSOR_CENTER = 0; 

/*
	-2 is default (consider making teensy wait) 
	-1 is victory
	1 is left
	2 is middle
	3 is right
*/
int GLOBAL_WANTED_DIRECTION = 0; 

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
#define CMD_UNKNOWN		(0x80 | 0xFF)

#define STATE_NOTHING		0x00
#define STATE_BASIC		0x10
#define STATE_ORDERS		0x20
#define STATE_DANCE		0x30
#define STATE_UNKNOWN		0xFF

#define MOVE_FORWARD		0x10
#define MOVE_BACKWARD		0x20
#define MOVE_UNKNOWN		0xFF

#define TURN_NONE		0x00
#define TURN_RIGHT		0x10
#define TURN_LEFT		0x20
#define TURN_UNKNOWN		0xFF

struct pair_s
{
	uint8 id;
	char* name;
};
typedef struct pair_s pair_t;

typedef pair_t turn_t;
typedef pair_t move_t;
typedef pair_t state_t;
typedef pair_t command_t;

static turn_t turns[] =
{
	{ TURN_NONE, "none" },
	{ TURN_LEFT, "left" },
	{ TURN_RIGHT, "right" },
	{ TURN_UNKNOWN, "" }
};

static move_t moves[] =
{
	{ MOVE_FORWARD, "forward" },
	{ MOVE_BACKWARD, "backward" },
	{ MOVE_UNKNOWN, "" }
};

static state_t states[] =
{
	{ STATE_NOTHING, "nothing" },
	{ STATE_BASIC, "basic" },
	{ STATE_ORDERS, "orders" },
	{ STATE_DANCE, "dance" },
	{ STATE_UNKNOWN, "" }
};

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

char* get_name_from_id(uint8 id, pair_t* pairs, int npairs)
{
	int i;

	for (i = 0; i < npairs; i++)
	{
		if (pairs[i].id == id)
			return pairs[i].name;
	}

	return pairs[i].name;
}

uint8 get_id_from_name(char* name, pair_t* pairs, int npairs)
{
	int i;

	if (name == NULL)
		return 0xFF;

	for (i = 0; i < npairs; i++)
	{
		if (strcmp(pairs[i].name, name) == 0)
			return pairs[i].id;
	}

	return 0xFF;
}

char* get_turn_name(uint8 turn_id)
{
	return get_name_from_id(turn_id, turns, NELEMENTS(turns));
}

uint8 get_turn_id(char* turn_name)
{
	return get_id_from_name(turn_name, turns, NELEMENTS(turns));
}

char* get_move_name(uint8 move_id)
{
	return get_name_from_id(move_id, moves, NELEMENTS(moves));
}

uint8 get_move_id(char* move_name)
{
	return get_id_from_name(move_name, moves, NELEMENTS(moves));
}

char* get_state_name(uint8 state_id)
{
	return get_name_from_id(state_id, states, NELEMENTS(states));
}

uint8 get_state_id(char* state_name)
{
	return get_id_from_name(state_name, states, NELEMENTS(states));
}

char* get_command_name(uint8 cmd_id)
{
	return get_name_from_id(cmd_id, commands, NELEMENTS(commands));
}

uint8 get_command_id(char* cmd_name)
{
	return get_id_from_name(cmd_name, commands, NELEMENTS(commands));
}

uint8 get_decimal_value(char* decimal_str)
{
	uint8 decimal;

	if (decimal_str == NULL)
		return 0;

	decimal = (uint8) atoi(decimal_str);

	return decimal;
}

void send_command(int fd, uint8 cmd, uint8 val)
{
	printf("sending command \"%s\" (0x%02X) with value %d (0x%02X)\n",
		get_command_name(cmd), cmd, val, val);

	write(fd, &cmd, 1);
	write(fd, &val, 1);
}

void* IntelThreadProc(void* data)
{
	send_command(tty_fd, CMD_CHANGE_STATE, STATE_ORDERS);

	while(1)
	{
		if(-1 == GLOBAL_WANTED_DIRECTION) 
		{
			// VICTORY DANCE
			send_command(tty_fd, CMD_CHANGE_STATE, STATE_DANCE);
			sleep(10);
			send_command(tty_fd, CMD_CHANGE_STATE, STATE_ORDERS);
		}
		else if (35 > GLOBAL_SENSOR_CENTER)
		{
			// Do crazy backup
			printf("do crazy backup\n");
			send_command(tty_fd, CMD_SPEED, 0);
			sleep(1);
			send_command(tty_fd, CMD_SPEED, 127);
			send_command(tty_fd, CMD_HARD_TURN, TURN_LEFT);
			sleep(1);
			send_command(tty_fd, CMD_HARD_TURN, TURN_NONE);
 		} 
		else if ((55 > GLOBAL_SENSOR_LEFT) || (3 == GLOBAL_WANTED_DIRECTION))
		{
			// SOFT TURN RIGHT
			send_command(tty_fd, CMD_SOFT_TURN, TURN_RIGHT);
		}
		else if ((55 > GLOBAL_SENSOR_RIGHT) || (1 == GLOBAL_WANTED_DIRECTION))
		{
			// SOFT TURN LEFT
			send_command(tty_fd, CMD_SOFT_TURN, TURN_LEFT);
		}
		else
		{
			send_command(tty_fd, CMD_HARD_TURN, TURN_NONE);
			send_command(tty_fd, CMD_SPEED, 127);
			send_command(tty_fd, CMD_SET_DIRECTION, MOVE_FORWARD);
		}

		sleep(1);
	}
}

void* BTThreadProc(void* data)
{
  struct sockaddr_l2 addr = { 0 };
  int s, status;
  char buffer[100];
  char dest[18] = "00:02:72:16:1A:C1"; /* This is the address of the dongle on my laptop */
  char sendbuffer[100] = { 0 }; 

  while(1)
  {
    sleep(1);
    // allocate a socket
    s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);

    // set the connection parameters (who to connect to)
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm = htobs(0x1001);
    str2ba( dest, &addr.l2_bdaddr );

    // connect to server
    status = connect(s, (struct sockaddr *)&addr, sizeof(addr));

    // send a message
    if (status == 0) 
    {
        memset(sendbuffer, 0, sizeof(sendbuffer));
        strcpy(sendbuffer, "mode: "); 
        sprintf(buffer, "%i", GLOBAL_MODE); 
        strcat(sendbuffer, buffer); 
        strcat(sendbuffer, ", ");

        strcat(sendbuffer, get_state_name(GLOBAL_MODE));

        strcat(sendbuffer, "\n"); 

        strcat(sendbuffer, "speed: "); 
        sprintf(buffer, "%i", GLOBAL_SPEED);
        strcat(sendbuffer, buffer); 
        strcat(sendbuffer, "\n"); 

                    strcat(sendbuffer, "distance.right: "); 
        if (GLOBAL_SENSOR_RIGHT < 0xFF)  
        {
          sprintf(buffer, "%i", GLOBAL_SENSOR_RIGHT);
          strcat(sendbuffer, buffer); 
        }
        else 
          strcat(sendbuffer, "Far, far, away..."); 
        
        strcat(sendbuffer, "\n"); 
 
                    strcat(sendbuffer, "distance.center: "); 
        if (GLOBAL_SENSOR_CENTER < 0xFF)  
        {
          sprintf(buffer, "%i", GLOBAL_SENSOR_CENTER);
          strcat(sendbuffer, buffer); 
        }
        else 
          strcat(sendbuffer, "Far, far, away..."); 
        strcat(sendbuffer, "\n");                    

                    strcat(sendbuffer, "distance.right: "); 
        if (GLOBAL_SENSOR_LEFT < 0xFF)  
        {
          sprintf(buffer, "%i", GLOBAL_SENSOR_LEFT);
          strcat(sendbuffer, buffer); 
        }
        else 
          strcat(sendbuffer, "Far, far, away..."); 

        strcat(sendbuffer, "\n"); 
        status = write(s, sendbuffer, sizeof(sendbuffer)); 
    }

    if( status < 0 ) perror("DERP!");
  }
}

/* opencv stuff goes here */
void* CameraThreadProc(void* tdata)
{
    CvCapture *capture = 0;
    IplImage *frame = 0;
    int key = 0;
    int loop; 
    
    int arg_index; 

    /* Mainly for loops */
    int i;
    int j;
    int k;
    unsigned char screen_section; 
    unsigned int screen_segment; 

    int step; 
    int height;
    int width; 
    int channels;
    
    unsigned long count_red; 

    /* 
      Depending on these percentages, and the defined threshold,
      we will know and tell the vehicle appropriately which way to
      go. 
    */
    unsigned int total_section1 = 0;
    unsigned int total_section2 = 0;
    unsigned int total_section3 = 0;
    double percent_r_section1 = 0.0f; 
    double percent_r_section2 = 0.0f; 
    double percent_r_section3 = 0.0f; 
    char direction[10];
    
    /* Controls what color we're looking for */
    unsigned char channel_checking = 0; 

    unsigned char *data; 
 
    channel_checking = 2; // red

    /* initialize camera */
    capture = cvCaptureFromCAM( 0 );
 
    if ( !capture ) {
        fprintf( stderr, "Cannot open initialize webcam!\n" );
        return 1;
    }
 
    while(1) {
        frame = cvQueryFrame( capture );

        if (!frame) break;
       
        data = (unsigned char*) frame->imageData; 

        height = frame->height; 
        width = frame->width; 
        step = frame->widthStep;
        channels = frame->nChannels;

        screen_segment = width / 3; 

        count_red = 0 ;
        total_section1 = 0;       
        total_section2 = 0;       
        total_section3 = 0;       
        
        for(i=0;i<height;i++) 
        {
          for(j=0;j<width;j++) 
          {
            
            /* Check if white */
            if (data[i*step+j*channels+2] >= QUALIFY_THRESHOLD &&
                data[i*step+j*channels+0] >= QUALIFY_THRESHOLD && 
                data[i*step+j*channels+1] >= QUALIFY_THRESHOLD   ) 
            {
              ++count_red;

              /* Right Segment */
              if (j>=screen_segment*2)
              {
                ++total_section3;
              }
              /* Middle Segment */
              else if (j>=screen_segment)
              {
                ++total_section2; 
              }
              /* Left Segment */
              else 
              {
                ++total_section1; 
              }
  
            }

          } 
        }

        percent_r_section1 = 100 * (double) total_section1 / (screen_segment * height); 
        percent_r_section2 = 100 * (double) total_section2 / (screen_segment * height); 
        percent_r_section3 = 100 * (double) total_section3 / (screen_segment * height); 

        /*
            Note - This is your intelligence!
            Watch it crumble! When adding to the
            beagle board, you should just set
            the global flag here so that the thread
            edits said variable. 
        */

        /* left case */ 
        if( percent_r_section1 >= VICTORY_THRESHOLD )
        {
          strcpy(direction,"[victory]");      
          GLOBAL_WANTED_DIRECTION = -1; 
        }
        else if 
        ( percent_r_section3 > percent_r_section2 && 
          percent_r_section3 > percent_r_section1 && 
          percent_r_section3 >= DIRECTION_THRESHOLD) 
        {
          strcpy(direction,"[left]");      
          GLOBAL_WANTED_DIRECTION = 1; 
        }

        /* right case */
        else if 
        ( percent_r_section1 > percent_r_section2 && 
          percent_r_section1 > percent_r_section3 && 
          percent_r_section1 >= DIRECTION_THRESHOLD)
        {
          strcpy(direction,"[right]");      
          GLOBAL_WANTED_DIRECTION = 1; 
        }

        /* forward case */
        else if 
        ( percent_r_section2 > percent_r_section1 &&
          percent_r_section2 > percent_r_section3 && 
          percent_r_section2 >= DIRECTION_THRESHOLD)
        {
          strcpy(direction,"[forward]");      
          GLOBAL_WANTED_DIRECTION = 2; 
        }

        /* default */
        else 
        {
          strcpy(direction,"[default]");      
	  GLOBAL_WANTED_DIRECTION = -2; 
        }

        /*printf
        (
          "x:%d,y:%d,[r%%:%f][r%%:%f][r%%:%f] : I want to go... %s    ", 
          frame->width
          ,frame->height
          ,percent_r_section1 
          ,percent_r_section2 
          ,percent_r_section3 
          ,direction
        ); */

        for(loop=0; loop<1000; ++loop)
          printf("\b"); 
        
        /* Wait 1 millisecond */
        usleep(1*1000);
    }
 
    cvReleaseCapture( &capture );
 
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
				state = get_state_id(val_str);

				if (state == STATE_UNKNOWN)
				{
					printf("unknown mode!\n");
					continue;
				}

				send_command(tty_fd, cmd, state);
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
				val = get_turn_id(val_str);

				if (val == TURN_UNKNOWN)
				{
					printf("unknown turn!\n");
					continue;
				}

				send_command(tty_fd, cmd, val);
				break;

			case CMD_SOFT_TURN:
				val = get_turn_id(val_str);

				if (val == TURN_UNKNOWN)
				{
					printf("unknown turn!\n");
					continue;
				}

				send_command(tty_fd, cmd, val);
				break;

			case CMD_SET_DIRECTION:
				val = get_move_id(val_str);

				if (val == MOVE_UNKNOWN)
				{
					printf("unknown move direction!\n");
					continue;
				}

				send_command(tty_fd, cmd, val);
				break;

			case CMD_DIST_CENTER:
				val = get_decimal_value(val_str);
				send_command(tty_fd, cmd, val);
				break;

			case CMD_DIST_LEFT:
				val = get_decimal_value(val_str);
				send_command(tty_fd, cmd, val);
				break;

			case CMD_DIST_RIGHT:
				val = get_decimal_value(val_str);
				send_command(tty_fd, cmd, val);
				break;

			case CMD_SPEED:
				val = get_decimal_value(val_str);
				send_command(tty_fd, cmd, val);
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

        while (1)
        {
                if (read(tty_fd, &comm, 1) > 0)
                {
                        // printf("0x%02X\n", comm);

			switch (comm)
			{
				case CMD_DIST_LEFT:
					read(tty_fd, &comm, 1);
					GLOBAL_SENSOR_LEFT = comm;
					break;

				case CMD_DIST_RIGHT:
					read(tty_fd, &comm, 1);
					GLOBAL_SENSOR_RIGHT = comm;
					break;

				case CMD_DIST_CENTER:
					read(tty_fd, &comm, 1);
					GLOBAL_SENSOR_CENTER = comm;
					break;

				case CMD_TEENSY_MODE:
					read(tty_fd, &comm, 1);
					GLOBAL_MODE = comm;
					break;

			}
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
	pthread_create(&intel_thread, NULL, IntelThreadProc, NULL);
	pthread_create(&bt_thread, NULL, BTThreadProc, NULL);
	pthread_create(&camera_thread, NULL, CameraThreadProc, NULL);

	pthread_join(cmd_thread, cmd_thread_status);
	pthread_join(comm_thread, &comm_thread_status);
	pthread_join(intel_thread, &intel_thread_status);
	pthread_join(bt_thread, &bt_thread_status);
	pthread_join(camera_thread, &camera_thread_status);

	close(tty_fd);

	return 0;
}
