idf_component_register(
    SRCS
        "bsp/esp32_s3_szp.c"
        "event/app_event.c"
        "storage/sd_manager.c"
        "player/player_core.c"
        "pages/page_manager.c"
        "pages/page_main_menu.c"
        "pages/page_mp3.c"
        "main.c"
        "assets/img_music_icon.c"
        "assets/img_wifiset_icon.c"
        "assets/img_btset_icon.c"
        "assets/img_camera_icon.c"
        "assets/img_att_icon.c"
        "assets/img_sd_icon.c"
        "assets/font_alipuhui20.c"
    INCLUDE_DIRS
        "."
        "bsp"
        "event"
        "storage"
        "player"
        "pages"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
