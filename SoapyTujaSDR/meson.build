project('SoapyTujaSDR', 'cpp', 'c',
	default_options : [ 'warning_level=3',
                        'buildtype=debugoptimized',
                        'cpp_std=c++11',
                        'c_std=c11', ],
	version : '0.0.1',
	license : 'MIT')

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')

# Optimizer arguments for Raspberry Pi 3
c_args = ['-ggdb', '-mcpu=cortex-a53', '-mfpu=neon-fp-armv8', '-mfloat-abi=hard', '-funsafe-math-optimizations',]

soapysdr_dep = dependency('SoapySDR')
tuja_dep = cpp.find_library('tuja')
alsa_dep = dependency('alsa')
volk_dep = dependency('volk')

sources = ['SoapyTujaSDR.cpp', 'alsa.c']
soapy_vfzsdr_lib = shared_library('soapytujasdr',
                        sources,
                        c_args: c_args,
                        cpp_args: c_args,
                        dependencies : [soapysdr_dep, tuja_dep, volk_dep, alsa_dep],
                        install : true,
                        install_dir : '/usr/local/lib/SoapySDR/modules0.7')
