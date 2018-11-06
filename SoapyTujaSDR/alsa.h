//
//  alsa.h
//  fcdsdr
//
//  Created by Albin Stigö on 21/11/2017.
//  Copyright © 2017 Albin Stigo. All rights reserved.
//

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <alsa/asoundlib.h>

snd_pcm_t* alsa_pcm_handle(const char* pcm_name, unsigned int rate, snd_pcm_uframes_t frames, snd_pcm_stream_t stream);

#ifdef __cplusplus
}
#endif
