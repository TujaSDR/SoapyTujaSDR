//
//  alsa.c
//  fcdsdr
//
//  Created by Albin Stigö on 21/11/2017.
//  Copyright © 2017 Albin Stigo. All rights reserved.
//

#include "alsa.h"

/* Try to get an ALSA capture handle */
snd_pcm_t* alsa_pcm_handle(const char* pcm_name, unsigned int rate, snd_pcm_uframes_t frames, snd_pcm_stream_t stream) {
    snd_pcm_t *pcm_handle = NULL;
    snd_pcm_hw_params_t *hwparams;
    
    // const unsigned int rate = 89286;  // Fixed sample rate of VFZSDR.
    const unsigned int periods = 4;   // Number of periods in ALSA ringbuffer.
    
    snd_pcm_hw_params_alloca(&hwparams);
    
    /* Open normal blocking */
    if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
        fprintf(stderr, "Error opening PCM device %s\n", pcm_name);
        return NULL;
        //exit(EXIT_FAILURE);
    }
    
    /* Init hwparams with full configuration space */
    if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "Can not configure this PCM device.\n");
        return NULL;
        // exit(EXIT_FAILURE);
    }
    
    /* Interleaved access. (IQ interleaved). */
    if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        fprintf(stderr, "Error setting access.\n");
        return NULL;
        //exit(EXIT_FAILURE);
    }
    
    unsigned int channels = 2;
    /* Set number of channels */
    if (snd_pcm_hw_params_set_channels_near(pcm_handle, hwparams, &channels) < 0) {
        fprintf(stderr, "Error setting channels.\n");
        return NULL;
        //exit(EXIT_FAILURE);
    }
    
    /* Set sample format */
    /* Use native format of device to avoid costly conversions */
    if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, SND_PCM_FORMAT_S32) < 0) {
        fprintf(stderr, "Error setting format.\n");
        return NULL;
        // exit(EXIT_FAILURE);
    }
    
    /* Set sample rate. If the exact rate is not supported exit */
    if (snd_pcm_hw_params_set_rate(pcm_handle, hwparams, rate, 0) < 0) {
        fprintf(stderr, "Error setting rate.\n");
        return NULL;
        exit(EXIT_FAILURE);
    }
    
    /* Period size */
    int dir = 0;
    if (snd_pcm_hw_params_set_period_size(pcm_handle, hwparams, frames, dir) < 0) {
        fprintf(stderr, "Error setting period size.\n");
        return NULL;
        // exit(EXIT_FAILURE);
    }
        
    /* Set number of periods. Periods used to be called fragments. */
    if (snd_pcm_hw_params_set_periods(pcm_handle, hwparams, periods, 0) < 0) {
        fprintf(stderr, "Error setting periods.\n");
        return NULL;
        // exit(EXIT_FAILURE);
    }
    
    /* Apply HW parameter settings to */
    /* PCM device and prepare device  */
    if (snd_pcm_hw_params(pcm_handle, hwparams) < 0) {
        fprintf(stderr, "Error setting HW params.\n");
        return NULL;
        // exit(EXIT_FAILURE);
    }
    
    snd_pcm_uframes_t bufs = 0;
    snd_pcm_hw_params_get_buffer_size(hwparams, &bufs);
    
    return pcm_handle;
}
