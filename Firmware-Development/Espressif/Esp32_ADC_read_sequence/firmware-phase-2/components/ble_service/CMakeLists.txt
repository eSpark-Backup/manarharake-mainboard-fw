if(IDF_TARGET STREQUAL "esp32")
    set(component_srcs "src/ble_service.c" )

    idf_component_register(SRCS "${component_srcs}"
                           INCLUDE_DIRS "include"
                           PRIV_INCLUDE_DIRS ""
                           PRIV_REQUIRES bt nvs_flash
                           REQUIRES                                   
                                    )
    
    component_compile_options(-w)
    target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
endif()
