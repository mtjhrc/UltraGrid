/*
 * FILE:    audio/audio.h
 * AUTHORS: Martin Benes     <martinbenesh@gmail.com>
 *          Lukas Hejtmanek  <xhejtman@ics.muni.cz>
 *          Petr Holub       <hopet@ics.muni.cz>
 *          Milos Liska      <xliska@fi.muni.cz>
 *          Jiri Matela      <matela@ics.muni.cz>
 *          Dalibor Matura   <255899@mail.muni.cz>
 *          Ian Wesley-Smith <iwsmith@cct.lsu.edu>
 *
 * Copyright (c) 2005-2021 CESNET z.s.p.o.
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
 *      This product includes software developed by CESNET z.s.p.o.
 * 
 * 4. Neither the name of CESNET nor the names of its contributors may be used 
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
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
 *
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_

#define AUDIO_PROTOCOLS "JACK, rtsp, sdp or ultragrid_rtp" // available audio protocols
#define PORT_AUDIO              5006

#include "audio/types.h"
#include "tv.h"

struct module;
struct state_audio;

#ifdef __cplusplus
#include <chrono>

struct audio_options {
        const char *host = NULL;
        int recv_port = 0;
        int send_port = 0;
        const char *recv_cfg = "none";
        const char *send_cfg = "none";
        const char *proto = "ultragrid_rtp";
        const char *proto_cfg = "";
        const char *fec_cfg = "none";
        char *channel_map = nullptr;
        const char *scale = "none";
        bool echo_cancellation = false;
        const char *codec_cfg = "PCM";
        const char *filter_cfg = "";
};

struct state_audio * audio_cfg_init(struct module *parent,
                struct audio_options *opt,
                const char *encryption,
                int force_ip_version, const char *mcast_iface,
                long long int bitrate, volatile int *audio_delay,
                time_ns_t start_time,
                int mtu, int ttl, struct exporter *exporter);
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int audio_init_state_ok;

void audio_start(struct state_audio *s);
void audio_done(struct state_audio *s);
void audio_join(struct state_audio *s);

void audio_sdi_send(struct state_audio *s, struct audio_frame *frame);
struct audio_frame * sdi_get_frame(void *state);
void sdi_put_frame(void *state, struct audio_frame *frame);
void audio_register_display_callbacks(struct state_audio *s, void *udata, void (*putf)(void *, const struct audio_frame *), int (*reconfigure)(void *, int, int, int), int (*get_property)(void *, int, void *, size_t *));

struct audio_frame * audio_get_frame(struct state_audio *s);

unsigned int audio_get_display_flags(struct state_audio *s);

#ifdef __cplusplus
}
#endif


#endif
