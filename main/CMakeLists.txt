idf_component_register(
    SRCS
        "bsp/esp32_s3_szp.c"
        "event/app_event.c"
        "pages/page_manager.c"
        "pages/page_main_menu.c"
        "main.c"
    INCLUDE_DIRS
        "."
        "bsp"
        "event"
        "pages"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)