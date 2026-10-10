#include "bonds.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned long now = 10000;
static unsigned long millis()
{
	return now;
}

struct FakeUSB {
	bool mount = true;
	bool sleep = false;
	bool mounted() const
	{
		return mount;
	}
	bool suspended() const
	{
		return sleep;
	}
} USBDevice;

struct FakeHID {
	bool busy = false;
	bool ready() const
	{
		return USBDevice.mounted() && !busy;
	}
};

static FakeHID hid[NSLOT];
Slot g_slot[NSLOT] = {};
unsigned long g_connReplyMs[NSLOT] = {};
static unsigned long g_steamAliveMs = now;
static unsigned long g_resumeMs;
static unsigned long g_powerOffMs[NSLOT];
static uint8_t g_linkRssi[NSLOT], g_battery[NSLOT], g_batteryState[NSLOT];
static const unsigned long DISC_RESEND_MS = 6000;
static const int RSSI_DBM_OFFSET = 0;
static unsigned int connected[NSLOT], disconnected[NSLOT];

static bool slotPoweringOff(int slot)
{
	return g_powerOffMs[slot] && now - g_powerOffMs[slot] < 2000;
}
static bool lizardActive()
{
	return false;
}
static void wakeNudgeTask()
{
}
static void rfLizardRelease(FakeHID *, FakeHID *, uint8_t, uint8_t)
{
}
static void hapLogAdd(uint8_t, uint8_t, const uint8_t *, uint16_t)
{
}
static void usbTxHid(FakeHID *dest, uint8_t rid, const void *data, uint16_t)
{
	if (rid != 0x79)
		return;
	int slot = (int)(dest - hid);
	uint8_t state = *(const uint8_t *)data;
	if (state == 2)
		connected[slot]++;
	else if (state == 1)
		disconnected[slot]++;
}

class SteamPuckController {
    public:
	void task();
};
#include "puck_task.inc"

static SteamPuckController puck;
static bool expectCount(const char *what, unsigned int actual,
			unsigned int expected)
{
	if (actual == expected)
		return true;
	fprintf(stderr, "FAIL %s: got %u, expected %u\n", what, actual,
		expected);
	return false;
}
static void connectSlot(int slot)
{
	g_slot[slot].used = true;
	g_connReplyMs[slot] = now;
	puck.task();
	g_steamAliveMs = now;
}

int main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	connectSlot(0);
	bool ok = expectCount("initial connect", connected[0], 1);
	if (!strcmp(argv[1], "busy")) {
		// Every HID input occupies the endpoint until the host polls it.
		for (int i = 0; i < 1000; i++) {
			now++;
			g_connReplyMs[0] = now;
			hid[0].busy = true;
			puck.task();
			hid[0].busy = false;
			puck.task();
			g_steamAliveMs = now;
		}
		ok &= expectCount("connects through endpoint traffic",
				  connected[0], 1);
		ok &= expectCount("disconnects through endpoint traffic",
				  disconnected[0], 0);
	} else if (!strcmp(argv[1], "remount")) {
		USBDevice.mount = false;
		puck.task();
		USBDevice.mount = true;
		now++;
		g_connReplyMs[0] = now;
		puck.task();
		ok &= expectCount("USB remount connect", connected[0], 2);
	} else if (!strcmp(argv[1], "rf-drop")) {
		now += 301;
		puck.task();
		ok &= expectCount("RF loss disconnect", disconnected[0], 1);
		now += 750;
		puck.task();
		ok &= expectCount("disconnect retry", disconnected[0], 2);
		connectSlot(0);
		ok &= expectCount("RF recovery connect", connected[0], 2);
	} else if (!strcmp(argv[1], "rebind")) {
		g_slot[0].used = false;
		puck.task();
		connectSlot(0);
		ok &= expectCount("rebound slot connect", connected[0], 2);
	} else if (!strcmp(argv[1], "multi-slot")) {
		connectSlot(1);
		hid[0].busy = true;
		now++;
		g_connReplyMs[0] = g_connReplyMs[1] = now;
		puck.task();
		hid[0].busy = false;
		puck.task();
		ok &= expectCount("busy slot connect", connected[0], 1);
		ok &= expectCount("other slot connect", connected[1], 1);
	} else if (!strcmp(argv[1], "power-off")) {
		g_powerOffMs[0] = now;
		puck.task();
		ok &= expectCount("power-off disconnect", disconnected[0], 1);
		now += 100;
		g_connReplyMs[0] = now;
		puck.task();
		ok &= expectCount("shutdown tail connect", connected[0], 1);
	} else if (!strcmp(argv[1], "suspend")) {
		USBDevice.sleep = true;
		hid[0].busy = true;
		now++;
		puck.task();
		USBDevice.sleep = false;
		hid[0].busy = false;
		g_connReplyMs[0] = now;
		puck.task();
		ok &= expectCount("suspend/resume connect", connected[0], 1);
	} else {
		return 2;
	}
	if (ok)
		printf("PASS %s\n", argv[1]);
	return ok ? 0 : 1;
}
