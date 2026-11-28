/**
 * MagicLantern GuiMainTask override
 * This was previously camera-specific
 **/

/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <gui.h>

#include <dryos.h>
#include <property.h>
#include <propvalues.h>
#include <bmp.h>
#include <config.h>
#include <consts.h>
#include <lens.h>
#include <config-defines.h>
#include <boot-hack.h>
#include <menu.h>

/**
 * Supported cameras [E] Means it's enabled
 * [E] 1100D: counter_0x0c <-> msg_queue_0x30
 * [E] 600D : counter_0x0c <-> msg_queue_0x30
 * [E] 60D  : counter_0x0c <-> msg_queue_0x30
 * [E] 650D : counter_0x0c <-> msg_queue_0x30
 * [E] EOSM : counter_0x0c <-> msg_queue_0x30
 * [E] 700D : counter_0x0c <-> msg_queue_0x30
 * [E] 100D : counter_0x0c <-> msg_queue_0x30
 * [E] 5D3  : counter_0x0c <-> msg_queue_0x30
 * [E] 6D   : counter_0x0c <-> msg_queue_0x30
 */

/**
 * Easy to support cameras
 * [E] 550D : counter_0x04 <-> msg_queue_0x38
 * [D] 7D   : counter_0x04 <-> msg_queue_0x38
 */

/**
 * Unsupported cameras for now
 * 5D2  : counter_0x04 <-> msg_queue_0x34
 * 50D  : counter_0x04 <-> msg_queue_0x34
 * 500D : counter_0x04 <-> msg_queue_0x34
 */

struct semaphore * gui_sem;

// return 0 if you want to block this event
static int handle_buttons(struct event * event)
{
    ASSERT(event->type == 0)

    if (event->type != 0) return 1; // only handle events with type=0 (buttons)
    if (handle_common_events_startup(event) == 0) return 0;
    extern int ml_started;
    if (!ml_started) return 1;

    /* ML start-up screen still up: ignore MENU (Canon's menu would open under it) */
    extern int boot_logo_is_active(void);
    if (event->param == BGMT_MENU && boot_logo_is_active()) return 0;

    if (handle_common_events_by_feature(event) == 0) return 0;

    return 1;
}

struct gui_main_struct {
  void *          obj;        // off_0x00;
  uint32_t        counter_550d;
  uint32_t        off_0x08;
  uint32_t        counter; // off_0x0c;
  uint32_t        off_0x10;
  uint32_t        off_0x14;
  uint32_t        off_0x18;
  uint32_t        off_0x1c;
  uint32_t        off_0x20;
  uint32_t        off_0x24;
  uint32_t        off_0x28;
  uint32_t        off_0x2c;
  struct msg_queue *    msg_queue;    // off_0x30;
  struct msg_queue *    off_0x34;    // off_0x34;
  struct msg_queue *    msg_queue_550d;    // off_0x38;
  uint32_t        off_0x3c;
};

extern struct gui_main_struct gui_main_struct;

/* Power-off.  While Canon is powering off (sensor cleaning, or ML's shutdown has begun), every
 * GUI event goes straight to Canon: none of ML's handlers may hold one back then.  The camera
 * sometimes stayed on the sensor cleaning screen with every task idle, as if waiting for an
 * event that never reached Canon's GUI code.
 * The last 16 events since the power-off began are kept for the playback diagnostic screen:
 * type << 24 | param << 4 | what happened (0 = passed through ML, 1 = blocked by ML,
 * 2 = sent straight to Canon). */
static int pwr_ev_log[16];
static int pwr_ev_count = 0;
static int pwr_ev_started = 0;

int gui_pwr_event(int i)
{
    if (i >= pwr_ev_count || i >= 16) return -1;
    return pwr_ev_log[(pwr_ev_count - 1 - i) % 16];     /* 0 = most recent */
}

static void pwr_ev_record(struct event * event, int what)
{
    if (!pwr_ev_started) return;
    pwr_ev_log[pwr_ev_count % 16] = (event->type << 24) | ((event->param & 0xFFFFF) << 4) | what;
    pwr_ev_count++;
}

void ml_gui_main_task()
{
    struct event * event = NULL;
    int index = 0;
    void* funcs[GMT_NFUNCS];
    memcpy(funcs, (void*)GMT_FUNCTABLE, 4*GMT_NFUNCS);
    
    gui_init_end(); // no params?
    
    while(1)
    {
        msg_queue_receive(gui_main_struct.msg_queue, &event, 0);
        gui_main_struct.counter--;

        if (event == NULL) {
            continue;
        }

        index = event->type;

        if (event->type == 0 && (event->param == GMT_GUICMD_START_AS_CHECK ||
                                 event->param == GMT_GUICMD_OPEN_SLOT_COVER ||
                                 event->param == GMT_GUICMD_LOCK_OFF))
            pwr_ev_started = 1;

        if (sensor_cleaning || ml_shutdown_requested)
        {
            pwr_ev_started = 1;
            pwr_ev_record(event, 2);
        }
        else if (!magic_is_off())
        {

            if (event->type == 0)
            {
                if (handle_buttons(event) == 0) { // ML button/event handler
                    pwr_ev_record(event, 1);
                    continue;
                }
            }
            else
            {
                if (handle_other_events(event) == 0) {
                    pwr_ev_record(event, 1);
                    continue;
                }
            }
            pwr_ev_record(event, 0);
        }

        if (IS_FAKE(event)) {
           event->arg = 0;      /* do not pass the "fake" flag to Canon code */
        }

        if (event->type == 0 && event->param < 0) {
            continue;           /* do not pass internal ML events to Canon code */
        }

        if ((index >= GMT_NFUNCS) || (index < 0)) {
            continue;
        }

        void(*f)(struct event *) = funcs[index];
        f(event);
    }
} 

TASK_OVERRIDE( gui_main_task, ml_gui_main_task);
