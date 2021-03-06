/* j4cDAC
 *
 * Copyright 2010, 2011 Jacob Potter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <LPC17xx.h>
#include <serial.h>
#include "ether.h"
#include <lwip/init.h>
#include <lwip/tcp.h>
#include <lwip/dhcp.h>
#include <ipv4/lwip/autoip.h>
#include <string.h>
#include <broadcast.h>
#include <assert.h>
#include <attrib.h>
#include <lightengine.h>
#include <skub.h>
#include <tables.h>
#include <playback.h>
#include <dac.h>
#include <ilda-player.h>

volatile uint32_t time;
volatile uint32_t mtime;

enum playback_source playback_src;
int playback_source_flags;

extern void tp_trianglewave_run(void);

void SysTick_Handler(void) {
	mtime++;
	if (mtime % 10 == 0) time++;
}

struct periodic_event {
	void (*f)(void);
	int period;
	const char *msg;
	int start;
} const events[] = {
	{ tcp_tmr, 250, "tcp", 100 },
	{ dhcp_coarse_tmr, 60000, "dhcp c", 35 },
	{ dhcp_fine_tmr, 500, "dhcp f", 25 },
	{ autoip_tmr, AUTOIP_TMR_INTERVAL, "autoip", 10 },
	{ broadcast_send, 1000, "broadcast", 10 }
};

int events_last[sizeof(events) / sizeof(events[0])];

TABLE(initializer_t, protocol);
TABLE(initializer_t, hardware);
TABLE(initializer_t, poll);

void playback_refill() {
	dac_point_t *ptr = 0;
	int i;

	int dlen = dac_request(&ptr);

	/* Have we underflowed? */
	if (dlen < 0) {
		if (le_get_state() != LIGHTENGINE_READY)
			return;

		outputf("*U*");
		dac_prepare();
		return;
	}

	/* If we don't have any more room... */
	if (dlen == 0) {
		if (dac_get_state() == DAC_PREPARED)
			dac_start();
		return;
	}

	switch (playback_src) {
	case SRC_ILDAPLAYER:
		if (!(playback_source_flags & ILDA_PLAYER_PLAYING))
			break;

		outputf("%d", dlen);

		i = ilda_read_points(dlen, ptr);

		if (i < 0) {
			outputf("err: %d", i);
			playback_source_flags &= ~ILDA_PLAYER_PLAYING;
		} else if (i == 0) {
			ilda_reset_file();

			if (playback_source_flags & ILDA_PLAYER_REPEAT) {
				outputf("rep");
			} else {
				outputf("done");

				/* If the whole file didn't fit in the
				 * buffer, we may have to start it now. */
				dlen = 0;

				playback_source_flags &= ~ILDA_PLAYER_PLAYING;
			}
		} else {
			dac_advance(i);
		}

		break;
	default:
		panic("bad playback source");
	}

	/* If the buffer is nearly full, start it up */
	if (dlen < 100 && dac_get_state() == DAC_PREPARED)
		dac_start();
}

int main(int argc, char **argv) {
	time = 0;

	int i;

	SysTick_Config(SystemCoreClock / 10000);
	serial_init();

	/* LEDs */
	LPC_GPIO0->FIODIR |= (1 << 0);
	LPC_GPIO1->FIODIR |= (1 << 28);
	LPC_GPIO1->FIOSET = (1 << 28);
	LPC_GPIO1->FIODIR |= (1 << 29);

	outputf("=== j4cDAC ===");

	outputf("skub_init()");
	skub_init();

	outputf("lwip_init()");
	lwip_init();

	outputf("== hardware ==");

	for (i = 0; i < TABLE_LENGTH(hardware); i++) {
		outputf("%s()", hardware_table[i].name);
		hardware_table[i].f();
	}

	outputf("== protocol ==");

	for (i = 0; i < TABLE_LENGTH(protocol); i++) {
		outputf("%s()", protocol_table[i].name);
		protocol_table[i].f();
	}

	outputf("ilda player");
	ilda_open("ildatest.ild");

	outputf("Entering main loop...");

/*
	playback_src = SRC_ILDAPLAYER;
	playback_source_flags = ILDA_PLAYER_PLAYING | ILDA_PLAYER_REPEAT;
*/

	__enable_irq();

	int status = 0;

	for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
		events_last[i] = events[i].start + time;
	}

	dac_set_rate(12000);
	ilda_set_fps_limit(30);

	while (1) {
		/* If we're playing from something other than the network,
		 * refill the point buffer. */
		if (playback_src != SRC_NETWORK)
			playback_refill();

		if (!(LPC_GPIO1->FIOPIN & (1 << 26))) {
			outputf("Blocking...");
			while (!(LPC_GPIO1->FIOPIN & (1 << 26)));
		}


//		LPC_GPIO1->FIOCLR = (1 << 28);

		if (status) {
			LPC_GPIO0->FIOPIN = 1;
			LPC_GPIO1->FIOPIN = 0;
			status = 0;
		} else {
			LPC_GPIO0->FIOPIN = 0;
			LPC_GPIO1->FIOPIN = (1 << 29);
			status = 1;
		}

//		LPC_GPIO1->FIOSET = (1 << 28);

		/* Check for periodic events */
		for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
			if (time > events_last[i] + events[i].period) {
				events[i].f();
				events_last[i] += events[i].period;
			}
		}

		/* Check the stuff we check on each loop iteration. */
		for (i = 0; i < TABLE_LENGTH(poll); i++) {
			poll_table[i].f();
		}
	}
}
