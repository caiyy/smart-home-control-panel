idf_component_register(SRCS "src/smart_control_panel_main.c" "drivers/st7701s.c" "src/smart_control_panel_init.c" "src/event_system.c" "src/ntp_time.c" "src/mqtt_client.c" "src/homeassistant.c" "src/http_service.c" "src/album_art_manager.c" "fonts/ht16.c" "fonts/time_100.c" "screens/main/main_screen.c" "ui/ui_manager.c" "ui/ui_common.c" "images/uiIcons.c"
                       REQUIRES GT911
                        PRIV_REQUIRES esp_lcd driver esp_timer nvs_flash esp_http_client esp_wifi esp_event mqtt json espressif__esp_jpeg
                       INCLUDE_DIRS "." "include" "drivers" "screens/main" "ui"
                       EMBED_TXTFILES "screens/main/main.xml")
