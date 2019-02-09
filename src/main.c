#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include "game.h"

int init(void);
void cleanup(void);
long get_msec(void);

static const char *termfile = "/dev/tty";
static struct termios saved_term;
static struct timeval tv0;

int main(int argc, char **argv)
{
	int res, c;
	long msec, next;
	struct timeval tv;

	if(argc > 1) {
		termfile = argv[1];
	}

	if(init() == -1) {
		return 1;
	}

	gettimeofday(&tv0, 0);

	tv.tv_sec = tick_interval / 1000;
	tv.tv_usec = (tick_interval % 1000) * 1000;

	for(;;) {
		fd_set rdset;
		FD_ZERO(&rdset);
		FD_SET(0, &rdset);

		while((res = select(1, &rdset, 0, 0, &tv)) == -1 && errno == EINTR);

		if(res > 0 && FD_ISSET(0, &rdset)) {
			while((c = fgetc(stdin)) >= 0) {
				game_input(c);
				if(quit) goto end;
			}
		}

		msec = get_msec();
		next = update(msec);

		tv.tv_sec = next / 1000;
		tv.tv_usec = (next % 1000) * 1000;
	}

end:
	cleanup();
	return 0;
}

int init(void)
{
	int fd;
	struct termios term;

	if((fd = open(termfile, O_RDWR | O_NONBLOCK)) == -1) {
		fprintf(stderr, "failed to open terminal device: %s: %s\n", termfile, strerror(errno));
		return -1;
	}

	if(tcgetattr(fd, &term) == -1) {
		fprintf(stderr, "failed to get terminal attributes: %s\n", strerror(errno));
		return -1;
	}
	saved_term = term;
	term.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	term.c_oflag &= ~OPOST;
	term.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	term.c_cflag = (term.c_cflag & ~(CSIZE | PARENB)) | CS8;

	if(tcsetattr(fd, TCSAFLUSH, &term) == -1) {
		fprintf(stderr, "failed to change terminal attributes: %s\n", strerror(errno));
		return -1;
	}

	close(0);
	close(1);
	close(2);
	dup(fd);
	dup(fd);

	umask(002);
	open("ansitris.log", O_WRONLY | O_CREAT, 0664);


	if(init_game() == -1) {
		return -1;
	}

	return 0;
}

void cleanup(void)
{
	cleanup_game();
	tcsetattr(0, TCSAFLUSH, &saved_term);
}

long get_msec(void)
{
	struct timeval tv;

	gettimeofday(&tv, 0);

	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}