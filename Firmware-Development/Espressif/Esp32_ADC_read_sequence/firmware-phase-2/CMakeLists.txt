# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(firmware)
# set(EXTRA_COMPONENT_DIRS    components/adc_controller 
#                             components/adc_dsp 
#     )
# set(EXTRA_COMPONENT_DIRS components)
set(COMPONENT_DIRS components)

