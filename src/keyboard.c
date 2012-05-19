/* 
 * keyboard.c
 * 
 * Copyright 2012 Dustin Dorroh <dustindorroh@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <kernel.h>
#include <keyboard.h>
extern pipe_buffer *input_pipe;

static unsigned convert(unsigned key);
static void write_kbd(unsigned adr, unsigned data);

/* 
 * keyboard_handler
 * 
 * This function is called every time a key is pressed or released.
 */
void keyboard_handler(regs * r)
{
	unsigned key = inb(KEYBOARD_INPUT_PORT);

	unsigned char i = convert(key);
	if ((i != 0) && input_pipe)
		write_to_pipe(input_pipe, &i, 1);

	/* reset hardware interrupt at 8259 chip */
	outb(0x20, 0x20);

	context_switch(r);
}

static void write_kbd(unsigned adr, unsigned data)
{
	unsigned long timeout;
	unsigned stat;

	/* Linux code didn't have a timeout here... */
	for (timeout = 500000L; timeout != 0; timeout--) {
		stat = inb(KEYBOARD_STATUS_PORT);
		/* loop until 8042 input buffer empty */
		if ((stat & KEYBOARD_INPUT_BUF_STATUS) == 0)
			break;
	}
	if (timeout != 0)
		outb(adr, data);
}

static unsigned convert(unsigned key)
{
	static const unsigned char key_map[] = {
		/* 00 */ 0, 0x1B, '1', '2', '3', '4', '5', '6',
		/* 08 */ '7', '8', '9', '0', '-', '=', '\b', '\t',
		/* 10 */ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
		/* 1Dh is left Ctrl */
		/* 18 */ 'o', 'p', '[', ']', '\n', 0, 'a', 's',
		/* 20 */ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
		/* 2Ah is left Shift */
		/* 28 */ '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
		/* 36h is right Shift */
		/* 30 */ 'b', 'n', 'm', ',', '.', '/', 0, 0,
		/* 38h is left Alt, 3Ah is Caps Lock */
		/* 38 */ 0, ' ', 0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		/* 45h is Num Lock, 46h is Scroll Lock */
		/* 40 */ KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
		/* 48 */ KEY_UP, KEY_PGUP, '-', KEY_LFT, '5', KEY_RT, '+', KEY_END,
		/* 50 */ KEY_DN, KEY_PGDN, KEY_INS, KEY_DEL, 0, 0, 0, KEY_F11,
		/* 58 */ KEY_F12
	};
	static const unsigned char key_map_shift[] = {
		/* 00 */ 0, 0x1B, '!', '@', '#', '$', '%', '^',
		/* 08 */ '&', '*', '(', ')', '_', '+', '\b', '\t',
		/* 10 */ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
		/* 1Dh is left Ctrl */
		/* 18 */ 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
		/* 20 */ 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
		/* 2Ah is left Shift */
		/* 28 */ '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
		/* 36h is right Shift */
		/* 30 */ 'B', 'N', 'M', '<', '>', '?', 0, '*',
		/* 38h is left Alt, 3Ah is Caps Lock */
		/* 38 */ 0, ' ', 0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		/* 45h is Num Lock, 46h is Scroll Lock */
		/* 40 */ KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
		/* 48 */ KEY_UP, KEY_PGUP, '-', KEY_LFT, '5', KEY_RT, '+', KEY_END,
		/* 50 */ KEY_DN, KEY_PGDN, KEY_INS, KEY_DEL, 0, 0, 0, KEY_F11,
		/* 58 */ KEY_F12
	};
	static const unsigned char key_map_caps[] = {
		/* 00 */ 0, 0x1B, '1', '2', '3', '4', '5', '6',
		/* 08 */ '7', '8', '9', '0', '-', '=', '\b', '\t',
		/* 10 */ 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
		/* 1Dh is left Ctrl */
		/* 18 */ 'O', 'P', '[', ']', '\n', 0, 'A', 'S',
		/* 20 */ 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',
		/* 2Ah is left Shift */
		/* 28 */ '\'', '`', 0, '\\', 'Z', 'X', 'C', 'V',
		/* 36h is right Shift */
		/* 30 */ 'B', 'N', 'M', ',', '.', '/', 0, 0,
		/* 38h is left Alt, 3Ah is Caps Lock */
		/* 38 */ 0, ' ', 0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		/* 45h is Num Lock, 46h is Scroll Lock */
		/* 40 */ KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
		/* 48 */ KEY_UP, KEY_PGUP, '-', KEY_LFT, '5', KEY_RT, '+', KEY_END,
		/* 50 */ KEY_DN, KEY_PGDN, KEY_INS, KEY_DEL, 0, 0, 0, KEY_F11,
		/* 58 */ KEY_F12
	};
	static const unsigned char key_map_caps_shift[] = {
		/* 00 */ 0, 0x1B, '!', '@', '#', '$', '%', '^',
		/* 08 */ '&', '*', '(', ')', '_', '+', '\b', '\t',
		/* 10 */ 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
		/* 1Dh is left Ctrl */
		/* 18 */ 'o', 'p', '{', '}', '\n', 0, 'a', 's',
		/* 20 */ 'd', 'f', 'g', 'h', 'j', 'k', 'l', ':',
		/* 2Ah is left Shift */
		/* 28 */ '"', '~', 0, '|', 'z', 'x', 'c', 'v',
		/* 36h is right Shift */
		/* 30 */ 'b', 'n', 'm', '<', '>', '?', 0, '*',
		/* 38h is left Alt, 3Ah is Caps Lock */
		/* 38 */ 0, ' ', 0, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5,
		/* 45h is Num Lock, 46h is Scroll Lock */
		/* 40 */ KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, 0, 0, KEY_HOME,
		/* 48 */ KEY_UP, KEY_PGUP, '-', KEY_LFT, '5', KEY_RT, '+', KEY_END,
		/* 50 */ KEY_DN, KEY_PGDN, KEY_INS, KEY_DEL, 0, 0, 0, KEY_F11,
		/* 58 */ KEY_F12
	};

	static unsigned short kbd_status, saw_break_code;
	 /**/ unsigned short temp;

	/* check for break key (i.e. a key is released) */
	if (key >= 0x80) {
		saw_break_code = 1;
		key &= 0x7F;
	}
	/* the only break codes we're interested in are Shift, Ctrl, Alt */
	if (saw_break_code) {
		if (key == RAW1_LEFT_ALT || key == RAW1_RIGHT_ALT)
			kbd_status &= ~KBD_META_ALT;
		else if (key == RAW1_LEFT_CTRL || key == RAW1_RIGHT_CTRL)
			kbd_status &= ~KBD_META_CTRL;
		else if (key == RAW1_LEFT_SHIFT || key == RAW1_RIGHT_SHIFT)
			kbd_status &= ~KBD_META_SHIFT;
		saw_break_code = 0;
		return 0;
	}
	/* it's a make key: check the "meta" keys, as above */
	if (key == RAW1_LEFT_ALT || key == RAW1_RIGHT_ALT) {
		kbd_status |= KBD_META_ALT;
		return 0;
	}
	if (key == RAW1_LEFT_CTRL || key == RAW1_RIGHT_CTRL) {
		kbd_status |= KBD_META_CTRL;
		return 0;
	}
	if (key == RAW1_LEFT_SHIFT || key == RAW1_RIGHT_SHIFT) {
		kbd_status |= KBD_META_SHIFT;
		return 0;
	}
	/* Scroll Lock, Num Lock, and Caps Lock set the LEDs. These keys have
	   on-off (toggle or XOR) action, instead of momentary action */
	if (key == RAW1_SCROLL_LOCK) {
		kbd_status ^= KBD_META_SCRL;
		goto LEDS;
	}
	if (key == RAW1_NUM_LOCK) {
		kbd_status ^= KBD_META_NUM;
		goto LEDS;
	}
	if (key == RAW1_CAPS_LOCK) {
		kbd_status ^= KBD_META_CAPS;
 LEDS:		write_kbd(0x60, 0xED);	/* "set LEDs" command */
		temp = 0;
		if (kbd_status & KBD_META_SCRL)
			temp |= 1;
		if (kbd_status & KBD_META_NUM)
			temp |= 2;
		if (kbd_status & KBD_META_CAPS)
			temp |= 4;
		write_kbd(0x60, temp);	/* bottom 3 bits set LEDs */
		return 0;
	}

	/* Ignore invalid scan codes */
	if (key >= sizeof(key_map) / sizeof(key_map[0]))
		return 0;

	/* 
	 * Convert raw scancode to ASCII 
	 */
	if (kbd_status & KBD_META_CAPS) {
		/* Handle Caps Lock and Shift key conversion */
		temp = (kbd_status & KBD_META_SHIFT) ?
		    key_map_caps_shift[key] : key_map_caps[key];
	} else {
		/* Handle regular and Shift conversions */
		temp = (kbd_status & KBD_META_SHIFT) ?
		    key_map_shift[key] : key_map[key];
	}

	/* Defective keyboard? non-US keyboard? more than 104 keys? */
	if (temp == 0)
		return temp;

	/* handle the three-finger salute */
	if ((kbd_status & KBD_META_CTRL) && (kbd_status & KBD_META_ALT)
	    && (temp == KEY_DEL)) {
		kprintf("\n" "\x1B[42;37;1m" "*** rebooting!");
		reboot();
	}
	/* I really don't know what to do yet with Alt, Ctrl, etc. -- punt */
	return temp;
}
