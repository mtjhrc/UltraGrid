/*
 * FILE:   video_capture.c
 * AUTHOR: Colin Perkins <csp@csperkins.org>
 *         Martin Benes     <martinbenesh@gmail.com>
 *         Lukas Hejtmanek  <xhejtman@ics.muni.cz>
 *         Petr Holub       <hopet@ics.muni.cz>
 *         Milos Liska      <xliska@fi.muni.cz>
 *         Jiri Matela      <matela@ics.muni.cz>
 *         Dalibor Matura   <255899@mail.muni.cz>
 *         Ian Wesley-Smith <iwsmith@cct.lsu.edu>
 *
 * Copyright (c) 2001-2004 University of Southern California
 * Copyright (c) 2005-2010 CESNET z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 
 *      This product includes software developed by the University of Southern
 *      California Information Sciences Institute. This product also includes
 *      software developed by CESNET z.s.p.o.
 * 
 * 4. Neither the name of the University, Institute, CESNET nor the names of
 *    its contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "lib_common.h"
#include "video_codec.h"
#include "video_capture.h"
#include "video_capture/DirectShowGrabber.h"
#include "video_capture/aggregate.h"
#include "video_capture/bluefish444.h"
#include "video_capture/decklink.h"
#include "video_capture/deltacast.h"
#include "video_capture/deltacast_dvi.h"
#include "video_capture/dvs.h"
#include "video_capture/import.h"
#include "video_capture/linsys.h"
#include "video_capture/null.h"
#include "video_capture/quicktime.h"
#include "video_capture/screen.h"
#include "video_capture/swmix.h"
#include "video_capture/testcard.h"
#include "video_capture/testcard2.h"
#include "video_capture/v4l2.h"
#include "video_capture/rtsp.h"

#define VIDCAP_MAGIC	0x76ae98f0

void (*vidcap_free_devices_extrn)() = vidcap_free_devices;
void (*vidcap_done_extrn)(struct vidcap *) = vidcap_done;
vidcap_id_t (*vidcap_get_null_device_id_extrn)(void) = vidcap_get_null_device_id;
struct vidcap_type *(*vidcap_get_device_details_extrn)(int index) = vidcap_get_device_details;
int (*vidcap_init_extrn)(vidcap_id_t id, char *fmt, unsigned int flags, struct vidcap **) = vidcap_init;
struct video_frame *(*vidcap_grab_extrn)(struct vidcap *state, struct audio_frame **audio) = vidcap_grab;
int (*vidcap_get_device_count_extrn)(void) = vidcap_get_device_count;
int (*vidcap_init_devices_extrn)(void) = vidcap_init_devices;

int vidcap_init_noerr;

struct vidcap {
        void *state;
        int index;
        uint32_t magic;         /* For debugging */
};

struct vidcap_device_api {
        vidcap_id_t id;

        const char              *library_name;

        struct vidcap_type    *(*func_probe) (void);
        const char              *func_probe_str;
        void                  *(*func_init) (char *fmt, unsigned int flags);
        const char              *func_init_str;
        void                   (*func_done) (void *state);
        const char              *func_done_str;
        struct video_frame    *(*func_grab) (void *state, struct audio_frame **audio);
        const char              *func_grab_str;

        void                    *handle;
};

struct vidcap_device_api vidcap_device_table[] = {
        {
         /* The aggregate capture card */
         0,
         NULL,
         MK_STATIC(vidcap_aggregate_probe),
         MK_STATIC(vidcap_aggregate_init),
         MK_STATIC(vidcap_aggregate_done),
         MK_STATIC(vidcap_aggregate_grab),
         NULL
        },
        {
         0,
         NULL,
         MK_STATIC(vidcap_import_probe),
         MK_STATIC(vidcap_import_init),
         MK_STATIC(vidcap_import_done),
         MK_STATIC(vidcap_import_grab),
         NULL
        },
#if defined HAVE_RTSP
        {
         0,
         "rtsp",
         MK_NAME(vidcap_rtsp_probe),
         MK_NAME(vidcap_rtsp_init),
         MK_NAME(vidcap_rtsp_done),
         MK_NAME(vidcap_rtsp_grab),
         NULL
        },
#endif
#if defined HAVE_SWMIX || defined BUILD_LIBRARIES
        {
         /* The SW mix capture card */
         0,
         "swmix",
         MK_NAME(vidcap_swmix_probe),
         MK_NAME(vidcap_swmix_init),
         MK_NAME(vidcap_swmix_done),
         MK_NAME(vidcap_swmix_grab),
         NULL
        },
#endif
#if defined HAVE_BLUEFISH444 || defined BUILD_LIBRARIES
        {
         /* The Bluefish444 capture card */
         0,
         "bluefish444",
         MK_NAME(vidcap_bluefish444_probe),
         MK_NAME(vidcap_bluefish444_init),
         MK_NAME(vidcap_bluefish444_done),
         MK_NAME(vidcap_bluefish444_grab),
         NULL
        },
#endif /* HAVE_BLUEFISH444 */
#if defined HAVE_DSHOW || defined BUILD_LIBRARIES
        {
         /* The DirectShow capture card */
         0,
         "dshow",
         MK_NAME(vidcap_dshow_probe),
         MK_NAME(vidcap_dshow_init),
         MK_NAME(vidcap_dshow_done),
         MK_NAME(vidcap_dshow_grab),
         NULL
        },
#endif /* HAVE_DSHOW */
#if defined HAVE_SCREEN_CAP || defined BUILD_LIBRARIES
        {
         /* The screen capture card */
         0,
         "screen",
         MK_NAME(vidcap_screen_probe),
         MK_NAME(vidcap_screen_init),
         MK_NAME(vidcap_screen_done),
         MK_NAME(vidcap_screen_grab),
         NULL
        },
#endif /* HAVE_SCREEN */
#if defined HAVE_DVS || defined BUILD_LIBRARIES
        {
         /* The DVS capture card */
         0,
         "dvs",
         MK_NAME(vidcap_dvs_probe),
         MK_NAME(vidcap_dvs_init),
         MK_NAME(vidcap_dvs_done),
         MK_NAME(vidcap_dvs_grab),
         NULL
        },
#endif                          /* HAVE_DVS */
#if defined HAVE_DECKLINK || defined BUILD_LIBRARIES
        {
         /* The Blackmagic DeckLink capture card */
         0,
         "decklink",
         MK_NAME(vidcap_decklink_probe),
         MK_NAME(vidcap_decklink_init),
         MK_NAME(vidcap_decklink_done),
         MK_NAME(vidcap_decklink_grab),
         NULL
        },
#endif                          /* HAVE_DECKLINK */
#if defined HAVE_DELTACAST || defined BUILD_LIBRARIES
        {
         /* The Blackmagic DeckLink capture card */
         0,
         "deltacast",
         MK_NAME(vidcap_deltacast_probe),
         MK_NAME(vidcap_deltacast_init),
         MK_NAME(vidcap_deltacast_done),
         MK_NAME(vidcap_deltacast_grab),
         NULL
        },
        {
         0,
         "deltacast",
         MK_NAME(vidcap_deltacast_dvi_probe),
         MK_NAME(vidcap_deltacast_dvi_init),
         MK_NAME(vidcap_deltacast_dvi_done),
         MK_NAME(vidcap_deltacast_dvi_grab),
         NULL
        },
#endif                          /* HAVE_DELTACAST */
#if defined HAVE_LINSYS || defined BUILD_LIBRARIES
        {
         /* The HD-SDI Master Quad capture card */
         0,
         "linsys",
         MK_NAME(vidcap_linsys_probe),
         MK_NAME(vidcap_linsys_init),
         MK_NAME(vidcap_linsys_done),
         MK_NAME(vidcap_linsys_grab),
         NULL
        },
#endif                          /* HAVE_LINSYS */
#if defined HAVE_MACOSX
        {
         /* The QuickTime API */
         0,
         "quicktime",
         MK_NAME(vidcap_quicktime_probe),
         MK_NAME(vidcap_quicktime_init),
         MK_NAME(vidcap_quicktime_done),
         MK_NAME(vidcap_quicktime_grab),
         NULL
        },
#endif                          /* HAVE_MACOSX */
        {
         /* Dummy sender for testing purposes */
         0,
         "testcard",
         MK_NAME(vidcap_testcard_probe),
         MK_NAME(vidcap_testcard_init),
         MK_NAME(vidcap_testcard_done),
         MK_NAME(vidcap_testcard_grab),
         NULL
        },
#if defined HAVE_TESTCARD2 || defined BUILD_LIBRARIES
        {
         /* Dummy sender for testing purposes */
         0,
         "testcard2",
         MK_NAME(vidcap_testcard2_probe),
         MK_NAME(vidcap_testcard2_init),
         MK_NAME(vidcap_testcard2_done),
         MK_NAME(vidcap_testcard2_grab),
         NULL
        },
#endif /* HAVE_SDL */
#if defined HAVE_V4L2 || defined BUILD_LIBRARIES
        {
         /* Dummy sender for testing purposes */
         0,
         "v4l2",
         MK_NAME(vidcap_v4l2_probe),
         MK_NAME(vidcap_v4l2_init),
         MK_NAME(vidcap_v4l2_done),
         MK_NAME(vidcap_v4l2_grab),
         NULL
        },
#endif /* HAVE_V4L2 */
        {
         0,
         NULL,
         MK_STATIC(vidcap_null_probe),
         MK_STATIC(vidcap_null_init),
         MK_STATIC(vidcap_null_done),
         MK_STATIC(vidcap_null_grab),
         NULL
        }
};

#define VIDCAP_DEVICE_TABLE_SIZE (sizeof(vidcap_device_table)/sizeof(struct vidcap_device_api))

/* API for probing capture devices ****************************************************************/

static struct vidcap_type *available_devices[VIDCAP_DEVICE_TABLE_SIZE];
static int available_device_count = 0;

#ifdef BUILD_LIBRARIES
/* definded in video_display.c */
void *open_library(const char *name);
static void *vidcap_open_library(const char *vidcap_name)
{
        char name[128];
        snprintf(name, sizeof(name), "vidcap_%s.so.%d", vidcap_name, VIDEO_CAPTURE_ABI_VERSION);

        return open_library(name);
}

static int vidcap_fill_symbols(struct vidcap_device_api *device)
{
        void *handle = device->handle;

        device->func_probe = (struct vidcap_type *(*) (void))
                dlsym(handle, device->func_probe_str);
        device->func_init = (void *(*) (char *, unsigned int))
                dlsym(handle, device->func_init_str);
        device->func_done = (void (*) (void *))
                dlsym(handle, device->func_done_str);
        device->func_grab = (struct video_frame *(*) (void *, struct audio_frame **))
                dlsym(handle, device->func_grab_str);
        if(!device->func_probe || !device->func_init ||
                        !device->func_done || !device->func_grab) {
                fprintf(stderr, "Library %s opening error: %s \n", device->library_name, dlerror());
                return FALSE;
        }
        return TRUE;
}
#endif

int vidcap_init_devices(void)
{
        unsigned int i;
        struct vidcap_type *dt;

        assert(available_device_count == 0);

        for (i = 0; i < VIDCAP_DEVICE_TABLE_SIZE; i++) {
                //printf("probe: %d\n",i);
#ifdef BUILD_LIBRARIES
                vidcap_device_table[i].handle = NULL;
                if(vidcap_device_table[i].library_name) {
                        vidcap_device_table[i].handle =
                                vidcap_open_library(vidcap_device_table[i].library_name);
                        if(vidcap_device_table[i].handle) {
                                int ret;
                                ret = vidcap_fill_symbols(&vidcap_device_table[i]);
                                if(!ret) continue;
                        } else {
                                continue;
                        }
                }
#endif

                dt = vidcap_device_table[i].func_probe();
                if (dt != NULL) {
                        vidcap_device_table[i].id = dt->id;
                        available_devices[available_device_count++] = dt;
                }
        }

        return available_device_count;
}

void vidcap_free_devices(void)
{
        int i;

        for (i = 0; i < available_device_count; i++) {
                free(available_devices[i]);
                available_devices[i] = NULL;
        }
        available_device_count = 0;
}

int vidcap_get_device_count(void)
{
        return available_device_count;
}

struct vidcap_type *vidcap_get_device_details(int index)
{
        assert(index < available_device_count);
        assert(available_devices[index] != NULL);

        return available_devices[index];
}

vidcap_id_t vidcap_get_null_device_id(void)
{
        return VIDCAP_NULL_ID;
}

/* API for video capture **************************************************************************/

int vidcap_init(vidcap_id_t id, char *fmt, unsigned int flags, struct vidcap **state)
{
        unsigned int i;

        for (i = 0; i < VIDCAP_DEVICE_TABLE_SIZE; i++) {
                if (vidcap_device_table[i].id == id) {
                        struct vidcap *d =
                            (struct vidcap *)malloc(sizeof(struct vidcap));
                        d->magic = VIDCAP_MAGIC;
                        d->state = vidcap_device_table[i].func_init(fmt, flags);
                        d->index = i;
                        if (d->state == NULL) {
                                debug_msg
                                    ("Unable to start video capture device 0x%08lx\n",
                                     id);
                                free(d);
                                return -1;
                        }
                        if(d->state == &vidcap_init_noerr) {
                                free(d);
                                return 1;
                        }
                        *state = d;
                        return 0;
                }
        }
        debug_msg("Unknown video capture device: 0x%08x\n", id);
        return -1;
}

void vidcap_done(struct vidcap *state)
{
        assert(state->magic == VIDCAP_MAGIC);
        vidcap_device_table[state->index].func_done(state->state);
        free(state);
}

struct video_frame *vidcap_grab(struct vidcap *state, struct audio_frame **audio)
{
        assert(state->magic == VIDCAP_MAGIC);
        return vidcap_device_table[state->index].func_grab(state->state, audio);
}
