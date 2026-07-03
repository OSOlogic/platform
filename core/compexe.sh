#!/bin/bash

case $1 in
    0)  
        cd core
        case $2 in
            mr)  # Make + Run (normal)
                make -j$(nproc) || exit -1
                sudo setcap cap_sys_rawio,cap_dac_override+ep build/core_plc_osologic
                ./build/core_plc_osologic
                ;;
            mc)  # Make Clean + Make + Run
                make clean -j$(nproc)
                make -j$(nproc) || exit -1
                sudo setcap cap_sys_rawio,cap_dac_override+ep build/core_plc_osologic
                ./build/core_plc_osologic

                ;;
            md)  # Make Debug + Run
                make debug -j$(nproc) || exit -1
                sudo setcap cap_sys_rawio,cap_dac_override+ep build/core_plc_osologic
                ./build/core_plc_osologic
                ;;
            mt)  # Make Trace + Run
                make trace -j$(nproc) || exit -1
                sudo setcap cap_sys_rawio,cap_dac_override+ep build/core_plc_osologic
                ./build/core_plc_osologic
                ;;
        esac
        ;;
    1)
        cd addons/services/mqtt || exit
        if [[ $2 == "mr" ]]; then
            python3 main.py
        fi
        if [[ $2 == "mc" ]]; then
            python3 main.py
        fi
        ;;
    2)
        cd addons/services/modbustcp || exit
        if [[ $2 == "mr" ]]; then
            python3 server.py
        fi
        if [[ $2 == "mc" ]]; then
            python3 server.py
        fi
        ;;
    3)
        cd addons/services/bindings/python || exit
        if [[ $2 == "mr" ]]; then
            python3 example_api.py
        fi
        if [[ $2 == "mc" ]]; then
            python3 example_api.py
        fi
        ;;
esac


