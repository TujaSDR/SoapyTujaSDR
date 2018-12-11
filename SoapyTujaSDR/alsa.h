//
//  alsa.h
//  fcdsdr
//
//  Created by Albin Stigö on 21/11/2017.
//  Copyright © 2017 Albin Stigo. All rights reserved.
//

#pragma once

#include <stdio.h>
#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
    
    snd_pcm_t* alsa_pcm_handle(const char* pcm_name,
                               unsigned int rate,
                               const unsigned int periods,
                               snd_pcm_uframes_t frames,
                               snd_pcm_stream_t stream);
#ifdef __cplusplus
}
#endif
