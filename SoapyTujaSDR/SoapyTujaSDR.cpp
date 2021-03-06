#include "SoapyTujaSDR.hpp"
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/ConverterPrimitives.hpp>
#include <algorithm>
#include <stdexcept>
#include <errno.h>
#include <volk/volk.h>


// TODO: write generic
// CS32 => CF32
static void volkCS32toCF32(const void *srcBuff, void *dstBuff, const size_t numElems, const double scaler)
{
    // 2 samples per element
    const size_t elemDepth = 2;
    const float scaling_factor = INT32_MAX * scaler; // S32 = 2^(32-1)-1
    
    volk_32i_s32f_convert_32f((float*)dstBuff, (const int32_t*)srcBuff, scaling_factor,
                              static_cast<unsigned int>(numElems * elemDepth));
}

// TODO write generic
// CF32 => CS32
static void volkCF32toCS32(const void *srcBuff, void *dstBuff, const size_t numElems, const double scaler)
{
    // 2 samples per element
    const size_t elemDepth = 2;
    const float scaling_factor = INT32_MAX * scaler; // S32 = 2^(32-1)-1
    
    volk_32f_s32f_convert_32i((int32_t*)dstBuff, (const float*)srcBuff, scaling_factor,
                              static_cast<unsigned int>(numElems * elemDepth));
}

// CS32 => CS16 scaler ignored
static void volkCS32toCS16(const void *srcBuff, void *dstBuff, const size_t numElems, const double scaler)
{
    const size_t elemDepth = 2;
    const float scaling_factor = INT16_MAX * scaler; // S16 max = 2^(16-1)-1
    
    volk_32f_s32f_convert_16i((int16_t*)dstBuff, (const float*)srcBuff, scaling_factor,
                              static_cast<unsigned int>(numElems * elemDepth));
}

SoapyTujaSDR::SoapyTujaSDR(const std::string &alsa_device) :
d_pcm_capture_handle(nullptr),
d_pcm_playback_handle(nullptr),
d_converter_func_rx(nullptr),
d_converter_func_tx(nullptr),
d_channels(2),
d_sample_rate(89286),
d_periods(4),
d_period_frames(1024),
d_center_frequency(0),
d_alsa_device(alsa_device),
d_tuja(NULL)
{
    // TODO: maybe make buffer size and periods configurable
    // we have to experiment with these values
    int err;
    
    err = tuja_open("/dev/i2c-1", 0x23, &d_tuja);
    if (err < 0) {
        throw std::runtime_error("tuja_open" + std::string(strerror(err)));
    }
    // Sample buffer
    int buff_size = d_channels * d_period_frames;
    d_buff_rx.resize(buff_size);
    d_buff_tx.resize(buff_size);
    
    SoapySDR_setLogLevel(SOAPY_SDR_INFO);
}

SoapyTujaSDR::~SoapyTujaSDR()
{
    tuja_close(d_tuja);
}

// Identification API
std::string SoapyTujaSDR::getDriverKey() const
{
    return "TujaSDRDriver";
}

std::string SoapyTujaSDR::getHardwareKey() const
{
    return "TujaSDRHW";
}

// Channels API
size_t SoapyTujaSDR::getNumChannels(const int dir) const
{
    return 1;
    //return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapyTujaSDR::getFullDuplex(const int direction, const size_t channel) const
{
    // Seems to be ignored by SoapyRemote at least...
    SoapySDR_log(SOAPY_SDR_DEBUG, "getFullDuplex");
    return false;
}

// Stream API
std::vector<std::string> SoapyTujaSDR::getStreamFormats(const int direction, const size_t channel) const
{
    std::vector<std::string> formats;
    formats.push_back("CS16");
    formats.push_back("CS32");
    formats.push_back("CF32");
    return formats;
}

std::string SoapyTujaSDR::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
    fullScale = INT32_MAX;
    return "CS32";
}

SoapySDR::ArgInfoList SoapyTujaSDR::getStreamArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList streamArgs;
    /*
     SoapySDR::ArgInfo chanArg;
     chanArg.key = "chan";
     chanArg.value = "stereo_iq";
     chanArg.name = "Channel Setup";
     chanArg.description = "Input channel configuration.";
     chanArg.type = SoapySDR::ArgInfo::STRING;
     
     std::vector<std::string> chanOpts;
     std::vector<std::string> chanOptNames;
     
     chanOpts.push_back("stereo_iq");
     chanOptNames.push_back("Complex L/R = I/Q");
     
     chanArg.options = chanOpts;
     chanArg.optionNames = chanOptNames;
     
     streamArgs.push_back(chanArg);
     */
    return streamArgs;
}

SoapySDR::Stream *SoapyTujaSDR::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setupStream");
    
    // Check the format
    if (format == "CF32") {}
    else if (format == "CS32") {}
    else if (format == "CS16") {}
    else throw std::runtime_error("setupStream invalid format " + format);
    
    // TODO: Check this
    // Check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }
    
    if (direction == SOAPY_SDR_RX) {
        // RX
        d_converter_func_rx = SoapySDR::ConverterRegistry::getFunction("CS32", format);
        // reset buffer so there's zero chance we get garbage
        std::fill(d_buff_rx.begin(), d_buff_rx.end(), 0);
        if (d_converter_func_rx == nullptr) {
            throw std::runtime_error("SoapySDR::ConverterRegistry function not found: " + format);
        }
        d_pcm_capture_handle = alsa_pcm_handle(d_alsa_device.c_str(),
                                               d_sample_rate,
                                               d_periods,
                                               d_period_frames,
                                               SND_PCM_STREAM_CAPTURE);
        
        if (d_pcm_capture_handle == nullptr) {
            throw std::runtime_error("alsa_pcm_handle");
        }
    }
    
    else if (direction == SOAPY_SDR_TX) {
        // TX
        d_converter_func_tx = SoapySDR::ConverterRegistry::getFunction(format, "CS32");
        std::fill(d_buff_tx.begin(), d_buff_tx.end(), 0);
        if (d_converter_func_tx == nullptr) {
            throw std::runtime_error("SoapySDR::ConverterRegistry function not found: " + format);
        }
        d_pcm_playback_handle = alsa_pcm_handle(d_alsa_device.c_str(),
                                                d_sample_rate,
                                                d_periods,
                                                d_period_frames,
                                                SND_PCM_STREAM_PLAYBACK);
        
        if (d_pcm_playback_handle == nullptr) {
            throw std::runtime_error("alsa_pcm_handle");
        }
    }
    
    // Stream can apparently be anything
    return (SoapySDR::Stream *)(new int(direction));
}

void SoapyTujaSDR::closeStream(SoapySDR::Stream *stream)
{
    const int direction = *reinterpret_cast<int *>(stream);
    
    SoapySDR_log(SOAPY_SDR_DEBUG, "closeStream");
    
    if (direction == SOAPY_SDR_RX) {
        snd_pcm_close(d_pcm_capture_handle); // close handle
        d_converter_func_rx = nullptr;
        d_pcm_capture_handle = nullptr;
    }
    else if (direction == SOAPY_SDR_TX) {
        snd_pcm_close(d_pcm_playback_handle); // close handle
        d_converter_func_tx = nullptr;
        d_pcm_playback_handle = nullptr;
    }
}

size_t SoapyTujaSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "get mtu");
    // Stream MTU in number of elements
    return d_period_frames;
}

int SoapyTujaSDR::activateStream(SoapySDR::Stream *stream,
                                 const int flags,
                                 const long long timeNs,
                                 const size_t numElems)
{
    const int direction = *reinterpret_cast<int *>(stream);
    snd_pcm_state_t snd_state;
    int err = 0;
    
    
    switch (direction) {
        case SOAPY_SDR_RX:
            snd_state = snd_pcm_state(d_pcm_capture_handle);
            if(snd_state != SND_PCM_STATE_RUNNING) {
                err = snd_pcm_prepare(d_pcm_capture_handle);
            }
            if (err < 0) {
                SoapySDR_logf(SOAPY_SDR_ERROR, "activateStream (SOAPY_SDR_RX): %s snd_pcm_prepare %s",
                              alsa_state_str(snd_state), snd_strerror(err));
                return err;
            } break;
        case SOAPY_SDR_TX:
            snd_state = snd_pcm_state(d_pcm_playback_handle);
            if(snd_state != SND_PCM_STATE_RUNNING) {
                err = snd_pcm_prepare(d_pcm_playback_handle);
            }
            if (err < 0) {
                SoapySDR_logf(SOAPY_SDR_ERROR, "activateStream (SOAPY_SDR_TX): %s snd_pcm_prepare %s",
                              alsa_state_str(snd_state), snd_strerror(err));
                return err;
            } break;
    }
    
    return err;
}

int SoapyTujaSDR::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    const int direction = *reinterpret_cast<int *>(stream);
    snd_pcm_state_t snd_state;
    int err = 0;
    
    switch (direction) {
        case SOAPY_SDR_RX:
            snd_state = snd_pcm_state(d_pcm_capture_handle);
            if(snd_state == SND_PCM_STATE_RUNNING) {
                err = snd_pcm_drop(d_pcm_capture_handle); // stop and drop
            }
            if(err < 0) {
                SoapySDR_logf(SOAPY_SDR_ERROR, "deactivateStream (SOAPY_SDR_RX): %s snd_pcm_drop %s",
                              alsa_state_str(snd_state), snd_strerror(err));
                return err;
            } break;
        case SOAPY_SDR_TX:
            snd_state = snd_pcm_state(d_pcm_playback_handle);
            if(snd_state == SND_PCM_STATE_RUNNING) {
                err = snd_pcm_drop(d_pcm_playback_handle); // stop and drop
            }
            if (err < 0) {
                SoapySDR_logf(SOAPY_SDR_ERROR, "deactivateStream (SOAPY_SDR_RX): %s snd_pcm_drop %s",
                              alsa_state_str(snd_state), snd_strerror(err));
                return err;
            } break;
    }
    
    return err;
}

int SoapyTujaSDR::readStream(SoapySDR::Stream *stream,
                             void * const *buffs,
                             const size_t numElems,
                             int &flags,
                             long long &timeNs,
                             const long timeoutUs)
{
    snd_pcm_sframes_t n_err = 0;
    int err = 0;
    
    // This function has to be well defined at all times
    if (d_pcm_capture_handle == nullptr) {
        SoapySDR_log(SOAPY_SDR_FATAL, "readStream d_pcm_capture_handle == nullptr");
        return SOAPY_SDR_STREAM_ERROR;
    }
    
    snd_pcm_state_t snd_state = snd_pcm_state(d_pcm_capture_handle);
    switch (snd_state) {
        case SND_PCM_STATE_OPEN:
            // not setup properly, we should not get here.
            SoapySDR_logf(SOAPY_SDR_FATAL, "snd_state == SND_PCM_STATE_OPEN");
            return SOAPY_SDR_STREAM_ERROR;
        case SND_PCM_STATE_SETUP:
            // not prepared
            if((err = snd_pcm_prepare(d_pcm_capture_handle)) < 0) {
                // could not prepare
                SoapySDR_logf(SOAPY_SDR_ERROR, "snd_pcm_prepare %s", snd_strerror(err));
                return SOAPY_SDR_STREAM_ERROR;
            } // fallthrough
        case SND_PCM_STATE_PREPARED:
            // not started
            if((err = snd_pcm_start(d_pcm_capture_handle)) < 0) {
                // could not start
                SoapySDR_logf(SOAPY_SDR_ERROR, "snd_pcm_start %s", snd_strerror(err));
                return SOAPY_SDR_STREAM_ERROR;
            } // fallthrough
        case SND_PCM_STATE_RUNNING:
            if(snd_pcm_wait(d_pcm_capture_handle, int(timeoutUs / 1000.f)) == 0) {
                SoapySDR_logf(SOAPY_SDR_INFO, "readStream timeout");
                return SOAPY_SDR_TIMEOUT;
            }
            // not timed out, try to read
            n_err = snd_pcm_readi(d_pcm_capture_handle,
                                  d_buff_rx.data(),
                                  std::min<size_t>(numElems, d_period_frames));
            // Ok?
            if(n_err >= 0) {
                // read ok, convert and return.
                d_converter_func_rx(d_buff_rx.data(), buffs[0], n_err, 1.0);
                return (int) n_err;
            } // error, fallthrough
        case SND_PCM_STATE_XRUN:
            // try to recover
            if(snd_pcm_recover(d_pcm_capture_handle, (int) n_err, 0) == 0) {
                SoapySDR_logf(SOAPY_SDR_INFO, "readStream recoverd from overflow");
                // Recovered, let Soapy call us again
                return SOAPY_SDR_OVERFLOW;
            } else {
                if ((int)n_err == -EBADFD) {
                    // -EBADFD = file descriptor in bad state meaning the device was closed,
                    // this is expected.
                    SoapySDR_logf(SOAPY_SDR_INFO,
                                  "snd_pcm_recover: %s",
                                  snd_strerror((int)n_err));
                    return SOAPY_SDR_STREAM_ERROR;
                } else {
                    SoapySDR_logf(SOAPY_SDR_ERROR,
                                  "snd_pcm_recover: %s",
                                  snd_strerror((int)n_err));
                    return SOAPY_SDR_STREAM_ERROR;
                }
            } // this clause always returns
        case SND_PCM_STATE_DRAINING:
        case SND_PCM_STATE_PAUSED:
        case SND_PCM_STATE_SUSPENDED:
        case SND_PCM_STATE_DISCONNECTED:
            SoapySDR_logf(SOAPY_SDR_ERROR, "bad ALSA state: %s", snd_state);
            return SOAPY_SDR_STREAM_ERROR;
    }
}

int SoapyTujaSDR::writeStream (SoapySDR::Stream *stream,
                               const void *const *buffs,
                               const size_t numElems,
                               int &flags,
                               const long long timeNs,
                               const long timeoutUs)
{
    snd_pcm_sframes_t n_err;
    size_t n;
    int err;
    
    if (d_pcm_playback_handle == nullptr) {
        return SOAPY_SDR_STREAM_ERROR;
    }
    
    snd_pcm_state_t snd_state = snd_pcm_state(d_pcm_playback_handle);
    switch (snd_state) {
        case SND_PCM_STATE_OPEN:
            // not setup properly, we should not get here.
            SoapySDR_logf(SOAPY_SDR_FATAL, "writeStream: snd_state == SND_PCM_STATE_OPEN");
            return SOAPY_SDR_STREAM_ERROR;
        case SND_PCM_STATE_SETUP:
            if((err = snd_pcm_prepare(d_pcm_playback_handle)) < 0) {
                SoapySDR_logf(SOAPY_SDR_ERROR, "writeStream: snd_pcm_prepare: %s", snd_strerror(err));
                return SOAPY_SDR_STREAM_ERROR;
            }
            break;
        case SND_PCM_STATE_RUNNING:
            // if running wait
            if(snd_pcm_wait(d_pcm_playback_handle, int(timeoutUs / 1000)) == 0) {
                return SOAPY_SDR_TIMEOUT;
            } // fallthrough
        case SND_PCM_STATE_PREPARED:

            // not started, it will autostart when buffer is full
            n = std::min<size_t>(numElems, d_period_frames);
            d_converter_func_tx(buffs[0], d_buff_tx.data(), n, 1.0);
            n_err = snd_pcm_writei(d_pcm_playback_handle,
                                   d_buff_tx.data(),
                                   n);
            if (n_err > 0) {
                // ok return
                // printf("write %d\n", n_err);
                return (int) n_err;
            }  // error, fallthrough
        case SND_PCM_STATE_XRUN:
            if((n_err = snd_pcm_recover(d_pcm_playback_handle, (int) n_err, 0)) == 0) {
                SoapySDR_logf(SOAPY_SDR_INFO, "writeStream recoverd from underflow");
                // Recovered, let Soapy call us again
                return SOAPY_SDR_UNDERFLOW;
            } else {
                // could not recover, check error
                if ((int)n_err == -EBADFD) {
                    // device was closed
                    SoapySDR_logf(SOAPY_SDR_INFO, "writeStream: snd_pcm_recover: %s", snd_strerror((int)n_err));
                    return 0;
                } else {
                    // some other error
                    SoapySDR_logf(SOAPY_SDR_ERROR, "writeStream: snd_pcm_recover: %s", snd_strerror((int)n_err));
                    return SOAPY_SDR_STREAM_ERROR;
                } // always returns
            }
        case SND_PCM_STATE_DRAINING:
        case SND_PCM_STATE_PAUSED:
        case SND_PCM_STATE_SUSPENDED:
        case SND_PCM_STATE_DISCONNECTED:
            // should not end up here.
            SoapySDR_logf(SOAPY_SDR_ERROR, "bad ALSA state: %s", snd_state);
            return SOAPY_SDR_STREAM_ERROR;
    }
}


std::vector<std::string> SoapyTujaSDR::listAntennas(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "listAntennas");
    
    std::vector<std::string> antennas;
    antennas.push_back("RF0");
    // antennas.push_back("TX");
    return antennas;
}

void SoapyTujaSDR::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setAntenna");
    // TODO
}

std::string SoapyTujaSDR::getAntenna(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getAntenna");
    return "RF0";
    // return "TX";
}

bool SoapyTujaSDR::hasHardwareTime (const std::string &what) const {
    return true;
}

long long SoapyTujaSDR::getHardwareTime (const std::string &what) const {
    return 0LL;
}

std::vector<std::string> SoapyTujaSDR::listSensors (void) const {
    std::vector<std::string> sensors;
    return sensors;
}


void SoapyTujaSDR::setIQBalance (const int direction, const size_t channel, const std::complex< double > &balance) {
    // TODO
}

std::complex<double> SoapyTujaSDR::getIQBalance (const int direction, const size_t channel) const {
    return std::complex<double>(1,1);
}

/*
 TODO:
 bool SoapyTujaSDR::hasDCOffsetMode(const int direction, const size_t channel) const
 {
 return false;
 }*/

std::vector<std::string> SoapyTujaSDR::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> gains;
    // gains.push_back("AUDIO");
    return gains;
}

bool SoapyTujaSDR::hasGainMode(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "hasGainMode");
    return false;
}

void SoapyTujaSDR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setGainMode");
    // SoapySDR::Device::setGainMode(direction, channel, automatic);
}

bool SoapyTujaSDR::getGainMode(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getGainMode");
    return false;
}

void SoapyTujaSDR::setGain(const int direction, const size_t channel, const double value)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setGain");
    // SoapySDR::Device::setGain(direction, channel, value);
}

void SoapyTujaSDR::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting gain: %f", value);
}

double SoapyTujaSDR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getGain");
    return 0;
}

SoapySDR::Range SoapyTujaSDR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getGainRange");
    return SoapySDR::Range(0, 100);
}

// Frequency
void SoapyTujaSDR::setFrequency(const int direction,
                                const size_t channel,
                                const std::string &name,
                                const double frequency,
                                const SoapySDR::Kwargs &args)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setFrequency");
    
    if (name == "RF" && d_center_frequency != frequency)
    {
        tuja_set_frequency(d_tuja, frequency);
        d_center_frequency = frequency;
    }
}

double SoapyTujaSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "getFrequency");
    return d_center_frequency;
}

std::vector<std::string> SoapyTujaSDR::listFrequencies(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "listFrequencies");
    
    std::vector<std::string> names;
    names.push_back("RF");
    return names;
}

SoapySDR::RangeList SoapyTujaSDR::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getFrequencyRange");
    
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        // There's a filter bank switch at 15MHz so do this for now
        results.push_back(SoapySDR::Range(0, 45000000));
    }
    return results;
}

SoapySDR::ArgInfoList SoapyTujaSDR::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getFrequencyArgsInfo");
    SoapySDR::ArgInfoList freqArgs;
    // TODO: frequency arguments
    return freqArgs;
}

void SoapyTujaSDR::setSampleRate(const int direction, const size_t channel, const double rate)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setSampleRate");
}

double SoapyTujaSDR::getSampleRate(const int direction, const size_t channel) const
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "getSampleRate %f", d_sample_rate);
    return d_sample_rate;
}

std::vector<double> SoapyTujaSDR::listSampleRates(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "listSampleRates");
    std::vector<double> rates;
    rates.push_back(d_sample_rate);
    return rates;
}


void SoapyTujaSDR::setBandwidth(const int direction, const size_t channel, const double bw)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "setBandwidth");
}

double SoapyTujaSDR::getBandwidth(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "getBandwidth");
    return d_sample_rate;
}

SoapySDR::ArgInfoList SoapyTujaSDR::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList settings;
    
    SoapySDR_log(SOAPY_SDR_DEBUG, "getSettingInfo");
    
    return settings;
}

void SoapyTujaSDR::writeSetting(const std::string &key, const std::string &value)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "writeSetting");
}

std::string SoapyTujaSDR::readSetting(const std::string &key) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "readSetting");
    
    return "empty";
}

std::vector<double> SoapyTujaSDR::listBandwidths(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "listBandwidths");
    std::vector<double> results;
    results.push_back(d_sample_rate);
    return results;
}


// Registry
SoapySDR::KwargsList findTujaSDR(const SoapySDR::Kwargs &args)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "findTujaSDR");
    
    SoapySDR::KwargsList results;
    SoapySDR::Kwargs soapyInfo;
    
    // soapyInfo["device_id"] = std::to_string(0);
    soapyInfo["device"] = "TujaSDR"; // This is usually what is diplayed
    soapyInfo["alsadevice"] = "hw:CARD=tujasdr,DEV=0";
    
    results.push_back(soapyInfo);
    
    return results;
}

SoapySDR::Device *makeTujaSDR(const SoapySDR::Kwargs &args)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "makeTujaSDR");
    
    //create an instance of the device object given the args
    //here we will translate args into something used in the constructor
    
    std::string alsa_device = args.at("alsadevice");
    return (SoapySDR::Device*) new SoapyTujaSDR(alsa_device);
}

// Register format converters
static SoapySDR::ConverterRegistry registerVolkCS32toCF32(SOAPY_SDR_CS32, SOAPY_SDR_CF32, SoapySDR::ConverterRegistry::VECTORIZED, &volkCS32toCF32);

static SoapySDR::ConverterRegistry registerVolkCF32toCS32(SOAPY_SDR_CF32, SOAPY_SDR_CS32, SoapySDR::ConverterRegistry::VECTORIZED, &volkCF32toCS32);

static SoapySDR::ConverterRegistry registerVolkCS32toCS16(SOAPY_SDR_CS32, SOAPY_SDR_CS16, SoapySDR::ConverterRegistry::VECTORIZED, &volkCS32toCS16);

// Register driver
static SoapySDR::Registry registerTujaSDR("tujasdr", &findTujaSDR, &makeTujaSDR, SOAPY_SDR_ABI_VERSION);
