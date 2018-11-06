#include "SoapyTujaSDR.hpp"
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/ConverterPrimitives.hpp>
#include <algorithm>
#include <stdexcept>


// CS32 <> CF32
static void genericCS32toCF32(const void *srcBuff, void *dstBuff, const size_t numElems, const double scaler)
{
    const size_t elemDepth = 2;
    
    if (scaler == 1.0)
    {
        auto *src = (int32_t*)srcBuff;
        auto *dst = (float*)dstBuff;
        for (size_t i = 0; i < numElems*elemDepth; i++)
        {
            dst[i] = SoapySDR::S32toF32(src[i]);
        }
    }
    else
    {
        auto *src = (int32_t*)srcBuff;
        auto *dst = (int32_t*)dstBuff;
        for (size_t i = 0; i < numElems*elemDepth; i++)
        {
            dst[i] = SoapySDR::S32toF32(src[i]) * scaler;
        }
    }
}

// CS32 <> CS16 scaler ignored
static void genericCS32toCS16(const void *srcBuff, void *dstBuff, const size_t numElems, const double scaler)
{
    const size_t elemDepth = 2;
    
    auto *src = (int32_t*)srcBuff;
    auto *dst = (int16_t*)dstBuff;
    for (size_t i = 0; i < numElems*elemDepth; i++)
    {
        dst[i] = SoapySDR::S32toS16(src[i]);
    }
}


SoapyTujaSDR::SoapyTujaSDR(const std::string &alsa_device) :
d_pcm_handle(nullptr),
d_period_frames(2048),
d_channels(2),
d_sample_rate(89286),
d_frequency(0),
d_alsa_device(alsa_device)
{
    d_freq_f.open("/sys/class/sdr/vfzsdr/frequency");
    // Sample buffer
    d_buff.resize(d_channels * d_period_frames);
}

SoapyTujaSDR::~SoapyTujaSDR()
{
    d_freq_f.close();
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
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

bool SoapyTujaSDR::getFullDuplex(const int direction, const size_t channel) const
{
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
    // TODO
    fullScale = (1 << 24);
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
    // Register format converters once
    static SoapySDR::ConverterRegistry registerGenericCS32toCF32(SOAPY_SDR_CS32, SOAPY_SDR_CF32, SoapySDR::ConverterRegistry::GENERIC, &genericCS32toCF32);
    static SoapySDR::ConverterRegistry registerGenericCS32toCS16(SOAPY_SDR_CS32, SOAPY_SDR_CS16, SoapySDR::ConverterRegistry::GENERIC, &genericCS32toCS16);
    
    if (direction != SOAPY_SDR_RX) {
        throw std::runtime_error("setupStream only RX supported");
    }
    
    //check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
    {
        throw std::runtime_error("setupStream invalid channel selection");
    }
    
    SoapySDR_logf(SOAPY_SDR_DEBUG, "Wants format %s", format.c_str());
    
    // Format converter function
    d_converter_func = SoapySDR::ConverterRegistry::getFunction("CS32", format);
    if (d_converter_func == nullptr) {
        throw std::runtime_error("SoapySDR::ConverterRegistry");
    }
    
    d_pcm_handle = alsa_pcm_handle(d_alsa_device.c_str(), d_sample_rate, d_period_frames, SND_PCM_STREAM_CAPTURE);
    if (d_pcm_handle == nullptr) {
        throw std::runtime_error("alsa_pcm_handle");
    }
    
    return (SoapySDR::Stream *) this;
}

void SoapyTujaSDR::closeStream(SoapySDR::Stream *stream)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "close stream");
    if (d_pcm_handle != nullptr) {
        snd_pcm_drop(d_pcm_handle);
        snd_pcm_close(d_pcm_handle);
    }
}

size_t SoapyTujaSDR::getStreamMTU(SoapySDR::Stream *stream) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "get mtu");
    return d_period_frames;
}

int SoapyTujaSDR::activateStream(SoapySDR::Stream *stream,
                                 const int flags,
                                 const long long timeNs,
                                 const size_t numElems)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "activate stream");

    // snd_pcm_prepare(d_pcm_handle);
    snd_pcm_start(d_pcm_handle);
    
    return 0;
}

int SoapyTujaSDR::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "deactivate stream");
 
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;
    
    snd_pcm_drop(d_pcm_handle);
    snd_pcm_prepare(d_pcm_handle);
    
    return 0;
}

int SoapyTujaSDR::readStream(SoapySDR::Stream *stream,
                             void * const *buffs,
                             const size_t numElems,
                             int &flags,
                             long long &timeNs,
                             const long timeoutUs)
{
    // This function has to be well defined at all times
    if (d_pcm_handle == nullptr) {
        return 0;
    }
    
    // Are we running?
    if (snd_pcm_state(d_pcm_handle) != SND_PCM_STATE_RUNNING) {
        return 0;
    }
    
    // Timeout if not ready
    if(snd_pcm_wait(d_pcm_handle, int(timeoutUs / 1000)) == 0) {
        return SOAPY_SDR_TIMEOUT;
    }
    
    // Read from ALSA
    snd_pcm_sframes_t frames = 0;
    int err = 0;
    // no program is complete without a goto
again:
    // read numElems or d_period_size
    frames = snd_pcm_readi(d_pcm_handle, &d_buff[0], std::min<size_t>(d_period_frames, numElems));
    // try to handle xruns
    if(frames < 0) {
        err = (int) frames;
        if(snd_pcm_recover(d_pcm_handle, err, 0) == 0) {
            SoapySDR_logf(SOAPY_SDR_ERROR, "readStream recoverd from %s", snd_strerror(err));
            goto again;
        } else {
            SoapySDR_logf(SOAPY_SDR_ERROR, "readStream error: %s", snd_strerror(err));
            return SOAPY_SDR_STREAM_ERROR;
        }
    }
    
    // Convert. Format is setup in setupStream. Scaler = 1.0
    d_converter_func(&d_buff[0], buffs[0], frames, 1.0);
    
    return (int)frames;
}


std::vector<std::string> SoapyTujaSDR::listAntennas(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_DEBUG, "listAntennas");
    
    std::vector<std::string> antennas;
    antennas.push_back("RX");
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
    return "RX";
    // return "TX";
}

bool SoapyTujaSDR::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return false;
}

std::vector<std::string> SoapyTujaSDR::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;
    // results.push_back("AUDIO");
    return results;
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
    
    if (name == "RF" && d_frequency != frequency)
    {
        d_freq_f << int(frequency);
        d_freq_f.clear();
        d_freq_f.seekg(0, std::ios::beg);
        d_frequency = frequency;
    }
}

double SoapyTujaSDR::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    SoapySDR_logf(SOAPY_SDR_DEBUG, "getFrequency");
    return d_frequency;
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

static SoapySDR::Registry registerTujaSDR("tujasdr", &findTujaSDR, &makeTujaSDR, SOAPY_SDR_ABI_VERSION);
