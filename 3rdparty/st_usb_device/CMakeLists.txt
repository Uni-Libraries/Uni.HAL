add_library(st_usb_device INTERFACE)

target_sources(st_usb_device INTERFACE
    src_core/usbd_core.c
    src_core/usbd_ctlreq.c
    src_core/usbd_ioreq.c

    src_cdc/usbd_cdc.c
)

target_include_directories(st_usb_device INTERFACE include_core)
target_include_directories(st_usb_device INTERFACE include_cdc)

target_link_libraries(st_usb_device INTERFACE st_stm32h7)
