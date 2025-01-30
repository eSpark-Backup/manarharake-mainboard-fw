if(IDF_TARGET STREQUAL "esp32")
    set(component_srcs "src/adc_controller.c")

    idf_component_register(SRCS "${component_srcs}"
                           INCLUDE_DIRS "include"
                           PRIV_INCLUDE_DIRS ""
                           PRIV_REQUIRES "driver"
                           REQUIRES "esp_adc_cal")
    
    component_compile_options(-w)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
endif()
