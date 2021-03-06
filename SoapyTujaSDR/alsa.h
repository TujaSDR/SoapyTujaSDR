//
//  alsa.h
//  SoapyTujaSDR
//
//  Created by Albin Stigö on 21/11/2018.
//  Copyright © 2017 Albin Stigo. All rights reserved.
//

#pragma once

#include <stdio.h>
#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C"
{
#endif
    
    const char* alsa_state_str(snd_pcm_state_t state);
    
    snd_pcm_t* alsa_pcm_handle(const char* pcm_name,
                               unsigned int rate,
                               const unsigned int periods,
                               snd_pcm_uframes_t frames,
                               snd_pcm_stream_t stream);
    
#ifdef __cplusplus
}
#endif
