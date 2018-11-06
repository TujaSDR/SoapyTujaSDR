project('SoapyTujaSDR', 'cpp', 'c',
	default_options : [ 'warning_level=3',
                        'buildtype=debugoptimized',
                        'cpp_std=c++11' ],
	version : '0.0.1',
	license : 'MIT')

# put in /usr/local/lib/SoapySDR/modules0.7

soapysdr_dep = dependency('SoapySDR')
alsa_dep = dependency('alsa')

sources = ['SoapyTujaSDR.cpp', 'alsa.c']
soapy_vfzsdr_lib = shared_library('soapytujasdr',
                        sources,
                        dependencies : [soapysdr_dep, alsa_dep],
                        install : true)
