/*
 * Copyright 2013 Ian Pilcher <arequipeno@gmail.com>
 *
 * This program is free software.  You can redistribute it or modify it under
 * the terms of version 2 of the GNU General Public License (GPL), as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY -- without even the implied warranty of MERCHANTIBILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the text of the GPL for more details.
 *
 * Version 2 of the GNU General Public License is available at:
 *
 *   http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include "freecusd.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h>

int fcd_tty_open(const char *tty)
{
	struct termios tio;
	int fd;

	fd = open(tty, O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd == -1)
		FCD_PFATAL(tty);

	if (tcgetattr(fd, &tio) == -1) {
		FCD_PERROR("tcgetattr");
		goto tty_setup_failed;
	}

	/*
	 * Thecus software calls tcflush twice for some reason; maybe they
	 * hit this bug?
	 *
         * http://lkml.indiana.edu/hypermail/linux/kernel/0707.3/1776.html
	 */

	if (tcflush(fd, TCIFLUSH) == -1)
		FCD_PERROR("tcflush");

        tio.c_iflag = IGNPAR;
        tio.c_oflag = 0;
        tio.c_cflag = CLOCAL | HUPCL | CREAD | CS8 | B9600;
        tio.c_lflag = 0;
        tio.c_cc[VTIME] = 0;
        tio.c_cc[VMIN] = 1;

	if (cfsetospeed(&tio, B9600) == -1)
		FCD_PERROR("cfsetospeed");

	if (tcsetattr(fd, TCSANOW, &tio) == -1)
		FCD_PERROR("tcsetattr");

	/* Check that tcsetattr actually made *all* changes */

        if (tcgetattr(fd, &tio) == -1) {
		FCD_PERROR("tcgetattr");
		FCD_WARN("Cannot check LCD serial port parameters\n");
	}
	else {
		if (	tio.c_iflag != IGNPAR ||
			tio.c_oflag != 0 ||
			tio.c_cflag != (CLOCAL | HUPCL | CREAD | CS8 | B9600) ||
			tio.c_lflag != 0 ||
			tio.c_cc[VTIME] != 0 ||
			tio.c_cc[VMIN] != 1 ||
			cfgetospeed(&tio) != B9600	)
		{
			goto tty_setup_failed;
		}
	}

	return fd;

tty_setup_failed:
	FCD_WARN("Failed to set LCD serial port parameters\n");
	return fd;
}

void fcd_tty_write_msg(int fd, struct fcd_monitor *mon)
{
	static uint8_t seq = 1;
	int ret;

	mon->buf[0]  = 0x02;
	mon->buf[1]  = seq++;
	mon->buf[2]  = 0x00;
	mon->buf[3]  = 0x3d;
	mon->buf[4]  = 0x11;
	mon->buf[65] = 0x03;

	ret = write(fd, mon->buf, sizeof mon->buf);
	if (ret == -1)
		FCD_PERROR("write");
	else if (ret != sizeof mon->buf)
		FCD_ERR("Incomplete write (%d bytes)\n", ret);
}
