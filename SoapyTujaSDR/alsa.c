//
//  alsa.c
//  SoapyTujaSDR
//
//  Created by Albin Stigö on 21/11/2018
//  Copyright © 2017 Albin Stigo. All rights reserved.
//

#include "alsa.h"

/* Try to get an ALSA capture handle */
snd_pcm_t* alsa_pcm_handle(const char* pcm_name,
                           unsigned int rate,
                           const unsigned int periods,
                           snd_pcm_uframes_t frames,
                           snd_pcm_stream_t stream) {

    const unsigned int channels = 2;
    
    snd_pcm_t *pcm_handle = NULL;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    
    int err = 0;
    
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
    
    /* Open normal blocking */
    if ((err = snd_pcm_open(&pcm_handle, pcm_name, stream, 0)) < 0) {
        fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Init hwparams with full configuration space */
    if ((err = snd_pcm_hw_params_any(pcm_handle, hwparams)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_any: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Interleaved access. (IQ interleaved). */
    if ((err = snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_access: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Set number of channels */
    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hwparams, channels)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_channels_near: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Set sample format */
    /* Use native format of device to avoid costly conversions */
    if ((err = snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S32)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_format: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Set sample rate. If the exact rate is not supported exit */
    if ((err = snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_rate: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Period size */
    int dir = 0;
    if ((err = snd_pcm_hw_params_set_period_size(pcm_handle, hwparams, frames, dir)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_period_size: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Set number of periods. Periods used to be called fragments. */
    if ((err = snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params_set_periods: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* Apply HW parameter settings to */
    /* PCM device and prepare device  */
    if ((err = snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
        fprintf(stderr, "snd_pcm_hw_params: %s\n", snd_strerror(err));
        return NULL;
    }
    
    // swparams
    if ((err = snd_pcm_sw_params_current(pcm_handle, swparams)) < 0)
    {
        fprintf(stderr, "snd_pcm_sw_params_current: %s\n", snd_strerror(err));
        return NULL;
    }
    
    // Start playback when buffer is full
    if ((err = snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, periods * frames)) < 0) {
        fprintf(stderr, "snd_pcm_sw_params_set_start_threshold: %s\n", snd_strerror(err));
        return NULL;
    }
    
    /* // TODO: Should we set a stop threshold?
     // err = snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, 64);
     if (err < 0) {
     fprintf(stderr, "snd_pcm_sw_params_set_stop_threshold: %s\n", snd_strerror(err));
     return NULL;
     }*/
    
    // We want to at least be able to write this amount of data
    if ((err = snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, 256)) < 0) {
        fprintf(stderr, "snd_pcm_sw_params_set_avail_min: %s\n", snd_strerror(err));
        return NULL;
    }
    
    // enable timestamps
    if ((err = snd_pcm_sw_params_set_tstamp_mode(pcm_handle, swparams,
                                                 SND_PCM_TSTAMP_ENABLE)) < 0)
    {
        fprintf(stderr, "snd_pcm_sw_params_set_tstamp_mode: %s\n", snd_strerror(err));
        return NULL;
    }
    
    if ((err = snd_pcm_sw_params_set_tstamp_type(pcm_handle, swparams, SND_PCM_TSTAMP_TYPE_MONOTONIC) < 0)) {
        fprintf(stderr, "snd_pcm_sw_params_set_tstamp_mode: %s\n", snd_strerror(err));
        return NULL;
    }
    
    // apply
    if((err = snd_pcm_sw_params(pcm_handle, swparams)) < 0) {
        fprintf(stderr, "snd_pcm_sw_params: %s\n", snd_strerror(err));
        return NULL;
    }
    
    // Everything ok, return handle
    return pcm_handle;
}
