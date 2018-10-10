/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <assert.h>
#include <limits.h>

#include <zephyr.h>
#include <zephyr/types.h>
#include <misc/byteorder.h>

#include <bluetooth/services/hids.h>

#include "hid_event.h"
#include "ble_event.h"

#define MODULE hids
#include "module_state_event.h"

#define SYS_LOG_DOMAIN	MODULE_NAME
#define SYS_LOG_LEVEL	CONFIG_DESKTOP_SYS_LOG_HOG_LEVEL
#include <logging/sys_log.h>


#define BASE_USB_HID_SPEC_VERSION   0x0101

#define REPORT_SIZE_MOUSE          5 /* bytes */
#define REPORT_SIZE_KEYBOARD       9 /* bytes */
#define REPORT_SIZE_MPLAYER        1 /* bytes */

#define USAGE_PAGE_MOUSE_XY		0x01
#define USAGE_PAGE_MOUSE_WHEEL		0x01
#define USAGE_PAGE_KEYBOARD		0x07
#define USAGE_PAGE_LEDS			0x08
#define USAGE_PAGE_MOUSE_BUTTONS	0x09
#define USAGE_PAGE_MPLAYER		0x0C

enum report_id {
	REPORT_ID_RESERVED,
	REPORT_ID_MOUSE,
	REPORT_ID_KEYBOARD,
	REPORT_ID_MPLAYER,

	REPORT_ID_COUNT
};

static size_t report_index[REPORT_ID_COUNT];


HIDS_DEF(hids_obj,
	REPORT_SIZE_MOUSE,
	REPORT_SIZE_KEYBOARD,
	REPORT_SIZE_MPLAYER);

enum report_mode {
	REPORT_MODE_PROTOCOL,
	REPORT_MODE_BOOT,

	REPORT_MODE_COUNT
};

static enum report_mode report_mode;
static bool report_enabled[TARGET_REPORT_COUNT][REPORT_MODE_COUNT];


static void broadcast_subscription_change(enum target_report tr,
					  enum report_mode old_mode,
					  enum report_mode new_mode)
{
	if ((old_mode != new_mode) &&
	    (report_enabled[tr][old_mode] == report_enabled[tr][new_mode])) {
		/* No change in report state. */
		return;
	}

	struct hid_report_subscription_event *event =
		new_hid_report_subscription_event();

	event->report_type = tr;
	event->enabled     = report_enabled[tr][new_mode];

	SYS_LOG_INF("Notifications %sabled", (event->enabled)?("en"):("dis"));

	EVENT_SUBMIT(event);

}

static void pm_evt_handler(enum hids_pm_evt evt, struct bt_conn *conn)
{
	enum report_mode old_mode = report_mode;

	switch (evt) {
	case HIDS_PM_EVT_BOOT_MODE_ENTERED:
		SYS_LOG_INF("Boot mode");
		report_mode = REPORT_MODE_BOOT;
		break;

	case HIDS_PM_EVT_REPORT_MODE_ENTERED:
		SYS_LOG_INF("Report mode");
		report_mode = REPORT_MODE_PROTOCOL;
		break;

	default:
		break;
	}

	if (report_mode != old_mode) {
		if (IS_ENABLED(CONFIG_DESKTOP_HID_MOUSE)) {
			broadcast_subscription_change(TARGET_REPORT_MOUSE,
					old_mode, report_mode);
		}
		if (IS_ENABLED(CONFIG_DESKTOP_HID_KEYBOARD)) {
			broadcast_subscription_change(TARGET_REPORT_MOUSE,
					old_mode, report_mode);
		}
		if (IS_ENABLED(CONFIG_DESKTOP_HID_MPLAYER)) {
			broadcast_subscription_change(TARGET_REPORT_MOUSE,
					old_mode, report_mode);
		}
	}
}

static void notif_handler(enum hids_notif_evt evt, enum target_report tr,
			  enum report_mode mode)
{
	__ASSERT_NO_MSG((evt == HIDS_CCCD_EVT_NOTIF_ENABLED) ||
			(evt == HIDS_CCCD_EVT_NOTIF_DISABLED));

	bool enabled = (evt == HIDS_CCCD_EVT_NOTIF_ENABLED);
	bool changed = (report_enabled[tr][mode] != enabled);

	report_enabled[tr][mode] = enabled;

	if ((report_mode == mode) && changed) {
		broadcast_subscription_change(tr, mode, mode);
	}
}

static void mouse_notif_handler(enum hids_notif_evt evt)
{
	SYS_LOG_INF("");
	notif_handler(evt, TARGET_REPORT_MOUSE, REPORT_MODE_PROTOCOL);
}

static void boot_mouse_notif_handler(enum hids_notif_evt evt)
{
	SYS_LOG_INF("");
	notif_handler(evt, TARGET_REPORT_MOUSE, REPORT_MODE_BOOT);
}

static void keyboard_notif_handler(enum hids_notif_evt evt)
{
	SYS_LOG_INF("");
	notif_handler(evt, TARGET_REPORT_KEYBOARD, REPORT_MODE_PROTOCOL);
}

static void boot_keyboard_notif_handler(enum hids_notif_evt evt)
{
	SYS_LOG_INF("");
	notif_handler(evt, TARGET_REPORT_KEYBOARD, REPORT_MODE_BOOT);
}

static void mplayer_notif_handler(enum hids_notif_evt evt)
{
	SYS_LOG_INF("");
	notif_handler(evt, TARGET_REPORT_MPLAYER, REPORT_MODE_PROTOCOL);
}

static int module_init(void)
{
	static const u8_t report_map[] = {
#if CONFIG_DESKTOP_HID_MOUSE
		/* Usage page */
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x02,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

		/* Report: Mouse */
		0x09, 0x01,       /* Usage (Pointer) */
		0xA1, 0x00,       /* Collection (Physical) */
		0x85, REPORT_ID_MOUSE,
		0x75, 0x01,         /* Report Size (1) */
		0x95, 0x08,         /* Report Count (8) */
		0x05, USAGE_PAGE_MOUSE_BUTTONS,
		0x19, 0x01,         /* Usage Minimum (1) */
		0x29, 0x08,         /* Usage Maximum (8) */
		0x15, 0x00,         /* Logical Minimum (0) */
		0x25, 0x01,         /* Logical Maximum (1) */
		0x81, 0x02,         /* Input (Data, Variable, Absolute) */

		0x75, 0x08,         /* Report Size (8) */
		0x95, 0x01,         /* Report Count (1) */
		0x05, USAGE_PAGE_MOUSE_WHEEL,
		0x09, 0x38,         /* Usage (Wheel) */
		0x15, 0x81,         /* Logical Minimum (-127) */
		0x25, 0x7F,         /* Logical Maximum (127) */
		0x81, 0x06,         /* Input (Data, Variable, Relative) */

		0x75, 0x0C,         /* Report Size (12) */
		0x95, 0x02,         /* Report Count (2) */
		0x05, USAGE_PAGE_MOUSE_XY,
		0x09, 0x30,         /* Usage (X) */
		0x09, 0x31,         /* Usage (Y) */
		0x16, 0x01, 0xF8,   /* Logical Maximum (2047) */
		0x26, 0xFF, 0x07,   /* Logical Minimum (-2047) */
		0x81, 0x06,         /* Input (Data, Variable, Relative) */
		0xC0,             /* End Collection (Physical) */
		0xC0,           /* End Collection (Application) */
#endif

#if CONFIG_DESKTOP_HID_KEYBOARD
		/* Usage page - Keyboard */
		0x05, 0x01,     /* Usage Page (Generic Desktop) */
		0x09, 0x06,     /* Usage (Mouse) */

		0xA1, 0x01,     /* Collection (Application) */

		/* Report: Keyboard */
		0x85, REPORT_ID_KEYBOARD,

		/* Keyboard - Modifiers */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x08,       /* Report Count (8) */
		0x05, USAGE_PAGE_KEYBOARD,
		0x19, 0xe0,       /* Usage Minimum (Left Ctrl) */
		0x29, 0xe7,       /* Usage Maximum (Right GUI) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */

		/* Keyboard - Reserved */
		0x75, 0x08,       /* Report Size (8) */
		0x95, 0x01,       /* Report Count (1) */
		0x81, 0x01,       /* Input (Constant) */

		/* Keyboard - Keys */
		0x75, 0x08,       /* Report Size (8) */
		0x95, 0x06,       /* Report Count (6) */
		0x05, USAGE_PAGE_KEYBOARD,
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x65,       /* Logical Maximum (101) */
		0x19, 0x00,       /* Usage Minimum (0) */
		0x29, 0x65,       /* Usage Maximum (101) */
		0x81, 0x00,       /* Input (Data, Array) */

		/* Keyboard - LEDs */
		0x95, 0x05,       /* Report Count (5) */
		0x75, 0x01,       /* Report Size (1) */
		0x05, USAGE_PAGE_LEDS,
		0x19, 0x01,       /* Usage Minimum (1) */
		0x29, 0x05,       /* Usage Maximum (5) */
		0x91, 0x02,       /* Output (Data, Variable, Absolute) */

		/* Keyboard - LEDs padding */
		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x03,       /* Report Size (3) (padding) */
		0x91, 0x01,       /* Output (Data, Variable, Absolute) */

		0xC0,           /* End Collection (Application) */
#endif

#if CONFIG_DESKTOP_HID_MPLAYER
		/* Usage page - Consumer Control */
		0x05, USAGE_PAGE_MPLAYER,
		0x09, 0x01,     /* Usage (Consumer Control) */

		0xA1, 0x01,     /* Collection (Application) */

		0x85, REPORT_ID_MPLAYER,
		0x15, 0x00,       /* Logical minimum (0) */
		0x25, 0x01,       /* Logical maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x01,       /* Report Count (1) */

		0x09, 0xCD,       /* Usage (Play/Pause) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x83, 0x01, /* Usage (Consumer Control Configuration) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB5,       /* Usage (Scan Next Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xB6,       /* Usage (Scan Previous Track) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */

		0x09, 0xEA,       /* Usage (Volume Down) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x09, 0xE9,       /* Usage (Volume Up) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x25, 0x02, /* Usage (AC Forward) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0x0A, 0x24, 0x02, /* Usage (AC Back) */
		0x81, 0x06,       /* Input (Data,Value,Relative,Bit Field) */
		0xC0            /* End Collection */
#endif
	};

	/* HID service configuration */
	struct hids_init hids_init_obj = { 0 };
	static const u8_t mouse_mask[ceiling_fraction(REPORT_SIZE_MOUSE, 8)] = {0x01};

	hids_init_obj.info.bcd_hid        = BASE_USB_HID_SPEC_VERSION;
	hids_init_obj.info.b_country_code = 0x00;
	hids_init_obj.info.flags          = HIDS_REMOTE_WAKE |
			HIDS_NORMALLY_CONNECTABLE;

	/* Attach report map */
	hids_init_obj.rep_map.data = report_map;
	hids_init_obj.rep_map.size = sizeof(report_map);

	/* Declare HID reports */

	struct hids_inp_rep *input_report =
		&hids_init_obj.inp_rep_group_init.reports[0];
	size_t ir_pos = 0;

	if (IS_ENABLED(CONFIG_DESKTOP_HID_MOUSE)) {
		input_report[ir_pos].id       = REPORT_ID_MOUSE;
		input_report[ir_pos].size     = REPORT_SIZE_MOUSE;
		input_report[ir_pos].handler  = mouse_notif_handler;
		input_report[ir_pos].rep_mask = mouse_mask;

		report_index[input_report[ir_pos].id] = ir_pos;
		ir_pos++;
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_KEYBOARD)) {
		input_report[ir_pos].id          = REPORT_ID_KEYBOARD;
		input_report[ir_pos].size        = REPORT_SIZE_KEYBOARD;
		input_report[ir_pos].handler     = keyboard_notif_handler;

		report_index[input_report[ir_pos].id] = ir_pos;
		ir_pos++;
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_MPLAYER)) {
		input_report[ir_pos].id          = REPORT_ID_MPLAYER;
		input_report[ir_pos].size        = REPORT_SIZE_MPLAYER;
		input_report[ir_pos].handler     = mplayer_notif_handler;

		report_index[input_report[ir_pos].id] = ir_pos;
		ir_pos++;
	}

	hids_init_obj.inp_rep_group_init.cnt = ir_pos;

	/* Boot protocol setup */
	if (IS_ENABLED(CONFIG_DESKTOP_HID_MOUSE)) {
		hids_init_obj.is_mouse = true;
		hids_init_obj.boot_mouse_notif_handler =
			boot_mouse_notif_handler;
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_KEYBOARD)) {
		hids_init_obj.is_kb = true;
		hids_init_obj.boot_kb_notif_handler =
			boot_keyboard_notif_handler;
	}

	hids_init_obj.pm_evt_handler = pm_evt_handler;

	return hids_init(&hids_obj, &hids_init_obj);
}

static void mouse_report_sent(struct bt_conn *conn)
{
	struct hid_report_sent_event *event = new_hid_report_sent_event();

	event->report_type = TARGET_REPORT_MOUSE;
	EVENT_SUBMIT(event);
}

static void send_mouse_report(const struct hid_mouse_event *event)
{
	if (report_mode == REPORT_MODE_BOOT) {
		s8_t x = max(min(event->dx, SCHAR_MAX), SCHAR_MIN);
		s8_t y = max(min(event->dy, SCHAR_MAX), SCHAR_MIN);

		hids_boot_mouse_inp_rep_send(&hids_obj, NULL, &event->button_bm,
					     x, y, mouse_report_sent);
	} else {
		s16_t wheel = max(min(event->wheel, 0x7f), -0x7f);
		s16_t x = max(min(event->dx, 0x07ff), -0x07ff);
		s16_t y = max(min(event->dy, 0x07ff), -0x07ff);

		/* Convert to little-endian. */
		u8_t x_buff[2];
		u8_t y_buff[2];

		sys_put_le16(x, x_buff);
		sys_put_le16(y, y_buff);

		/* Encode report. */
		u8_t buffer[REPORT_SIZE_MOUSE];

		static_assert(sizeof(buffer) == 5, "Invalid report size");

		buffer[0] = event->button_bm;
		buffer[1] = wheel;
		buffer[2] = x_buff[0];
		buffer[3] = (y_buff[0] << 4) | (x_buff[1] & 0x0f);
		buffer[4] = (y_buff[1] << 4) | (y_buff[0] >> 4);

		hids_inp_rep_send(&hids_obj, NULL,
				  report_index[REPORT_ID_MOUSE],
				  buffer, sizeof(buffer), mouse_report_sent);
	}
}

static void keyboard_report_sent(struct bt_conn *conn)
{
	struct hid_report_sent_event *event = new_hid_report_sent_event();

	event->report_type = TARGET_REPORT_KEYBOARD;
	EVENT_SUBMIT(event);
}

static void send_keyboard(const struct hid_keyboard_event *event)
{
	u8_t report[REPORT_SIZE_KEYBOARD];

	static_assert(ARRAY_SIZE(report) == ARRAY_SIZE(event->keys) + 3,
			"Incorrect number of keys in event");

	/* Modifiers */
	report[0] = event->modifier_bm;

	/* Reserved */
	report[1] = 0;

	/* Pressed keys */
	memcpy(&report[2], &event->keys[0], sizeof(event->keys));

	/* Led */
	report[8] = 0;

	if (report_mode == REPORT_MODE_BOOT) {
		hids_boot_kb_inp_rep_send(&hids_obj, NULL, report,
					  sizeof(report) - sizeof(report[8]),
					  keyboard_report_sent);
	} else {
		hids_inp_rep_send(&hids_obj, NULL,
				  report_index[REPORT_ID_KEYBOARD],
				  report, sizeof(report), keyboard_report_sent);
	}
}

static void notify_hids(const struct ble_peer_event *event)
{
	int err = 0;

	switch (event->state) {
	case PEER_STATE_CONNECTED:
		err = hids_notify_connected(&hids_obj, event->conn_id);
		break;

	case PEER_STATE_DISCONNECTED:
		err = hids_notify_disconnected(&hids_obj, event->conn_id);
		break;

	case PEER_STATE_SECURED:
		/* No action */
		break;

	default:
		__ASSERT_NO_MSG(false);
		break;
	}

	if (err) {
		SYS_LOG_ERR("Failed to notify the HID service about the connection");
	}
}

static bool event_handler(const struct event_header *eh)
{
	if (IS_ENABLED(CONFIG_DESKTOP_HID_MOUSE)) {
		if (is_hid_mouse_event(eh)) {
			send_mouse_report(cast_hid_mouse_event(eh));

			return false;
		}
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_KEYBOARD)) {
		if (is_hid_keyboard_event(eh)) {
			send_keyboard(cast_hid_keyboard_event(eh));

			return false;
		}
	}

	if (is_ble_peer_event(eh)) {
		notify_hids(cast_ble_peer_event(eh));

		return false;
	}

	if (is_module_state_event(eh)) {
		struct module_state_event *event = cast_module_state_event(eh);

		if (check_state(event, MODULE_ID(ble_state), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			if (module_init()) {
				SYS_LOG_ERR("service init failed");

				return false;
			}
			SYS_LOG_INF("service initialized");

			module_set_state(MODULE_STATE_READY);
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
EVENT_LISTENER(MODULE, event_handler);
EVENT_SUBSCRIBE(MODULE, hid_keyboard_event);
EVENT_SUBSCRIBE(MODULE, hid_mouse_event);
EVENT_SUBSCRIBE(MODULE, module_state_event);
EVENT_SUBSCRIBE_EARLY(MODULE, ble_peer_event);
