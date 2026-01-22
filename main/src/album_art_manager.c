#include "album_art_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "jpeg_decoder.h"
#include "lvgl.h"
#include "event_system.h"

static const char *TAG = "ALBUM_ART";

// 专辑封面更新请求队列项
typedef struct {
    char url[512];
    int width;
    int height;
    bool is_background;
} album_art_request_t;

// 图片缓存管理 (存放在 PSRAM)
typedef struct {
    uint16_t* buffer;
    int width;
    int height;
    int brightness; // 仅背景图使用
    bool is_in_use; // 标记图片是否正在被使用
} image_cache_t;

static char s_current_cache_url[512] = {0};
// 缓存槽位：0: 100x100
static image_cache_t s_image_caches[1] = {0};
static SemaphoreHandle_t s_cache_mutex = NULL;

// 专辑封面更新请求队列
static QueueHandle_t s_album_art_queue = NULL;
// 专辑封面任务句柄
static TaskHandle_t s_album_art_task_handle = NULL;

static void init_cache_mutex() {
    if (s_cache_mutex == NULL) {
        s_cache_mutex = xSemaphoreCreateMutex();
    }
}

static void init_album_art_queue() {
    if (s_album_art_queue == NULL) {
        s_album_art_queue = xQueueCreate(5, sizeof(album_art_request_t));
    }
}



// Helper function to encode URL
static void url_encode(const char *src, char *dest) {
    static const char hex[] = "0123456789ABCDEF";
    while (*src) {
        if (isalnum((int)*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~' || *src == '/') {
            *dest++ = *src;
        } else if (*src == ' ') {
            *dest++ = '%';
            *dest++ = '2';
            *dest++ = '0';
        } else {
            *dest++ = '%';
            *dest++ = hex[(*src >> 4) & 0x0F];
            *dest++ = hex[*src & 0x0F];
        }
        src++;
    }
    *dest = '\0';
}

// Download URL content to buffer (caller must free)
static uint8_t* download_url_to_buffer(const char* url, size_t* out_len) {
    ESP_LOGI(TAG, "Downloading from: %s", url);
    
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return NULL;
    }
    
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection");
        esp_http_client_cleanup(client);
        return NULL;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "Content length: %d", content_length);
    
    if (content_length <= 0) {
        content_length = 50 * 1024;
    }
    
    // 使用PSRAM分配内存
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes from PSRAM", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }
    
    int total_read = 0;
    while (1) {
        int read_len = esp_http_client_read(client, (char*)buffer + total_read, content_length - total_read);
        if (read_len > 0) {
            total_read += read_len;
        } else if (read_len == 0) {
            break;
        } else {
            ESP_LOGE(TAG, "Error reading data");
            break;
        }
        if (total_read >= content_length) break;
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    *out_len = total_read;
    ESP_LOGI(TAG, "Downloaded %d bytes", total_read);
    return buffer;
}

// Parse JPEG header to get dimensions
static bool get_jpeg_size(const uint8_t* data, size_t len, int* width, int* height) {
    size_t i = 0;
    if (len < 2 || data[0] != 0xFF || data[1] != 0xD8) return false; 
    i += 2;
    
    while (i < len) {
        if (data[i] != 0xFF) return false; 
        while (i < len && data[i] == 0xFF) i++; 
        if (i >= len) return false;
        
        uint8_t marker = data[i];
        i++;
        
        if (marker == 0xC0 || marker == 0xC2) {
            if (i + 7 > len) return false;
            *height = (data[i + 3] << 8) | data[i + 4];
            *width = (data[i + 5] << 8) | data[i + 6];
            return true;
        }
        
        if (i + 2 > len) return false;
        uint16_t segment_len = (data[i] << 8) | data[i + 1];
        i += segment_len;
    }
    return false;
}

// Decode JPG to RGB565
static uint16_t* decode_jpg_to_rgb565(const uint8_t* jpg_data, size_t jpg_len, int* out_width, int* out_height) {
    int w = 0, h = 0;
    if (!get_jpeg_size(jpg_data, jpg_len, &w, &h)) {
        ESP_LOGE(TAG, "Failed to parse JPEG dimensions");
        return NULL;
    }
    
    size_t out_size = w * h * 2;
    uint16_t* pixels = (uint16_t*)heap_caps_malloc(out_size, MALLOC_CAP_SPIRAM);
    if (!pixels) {
        ESP_LOGE(TAG, "Failed to allocate RGB buffer of size %d", out_size);
        return NULL;
    }
    
    esp_jpeg_image_cfg_t jpeg_cfg = {
        .indata = (uint8_t *)jpg_data,
        .indata_size = jpg_len,
        .outbuf = (uint8_t*)pixels,
        .outbuf_size = out_size,
        .out_format = JPEG_IMAGE_FORMAT_RGB565,
        .out_scale = JPEG_IMAGE_SCALE_0,
        .flags = {
            .swap_color_bytes = 0,
        }
    };
    
    esp_jpeg_image_output_t outimg;
    esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &outimg);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %d", ret);
        heap_caps_free(pixels);
        return NULL;
    }
    
    *out_width = outimg.width;
    *out_height = outimg.height;
    return pixels;
}

static void album_art_task(void* pvParameters) {
    ESP_LOGI(TAG, "专辑封面处理任务启动");
    
    while (1) {
        album_art_request_t request;
        if (xQueueReceive(s_album_art_queue, &request, portMAX_DELAY) == pdPASS) {
            char* raw_url = request.url;
            int req_width = request.width;
            int req_height = request.height;
            
            ESP_LOGI(TAG, "开始处理专辑封面请求，URL: %s", raw_url);
            
            // 提取URL中的路径部分
            char* proto_end = strstr(raw_url, "://");
            if (!proto_end) {
                ESP_LOGE(TAG, "无效的URL格式: %s", raw_url);
                continue;
            }
            
            // 跳过协议部分，找到主机部分结束位置
            char* host_start = proto_end + 3;
            char* path_start = strchr(host_start, '/');
            
            if (!path_start) {
                ESP_LOGE(TAG, "无法解析URL中的路径部分: %s", raw_url);
                continue;
            }
            
            // 构建完整的路径，包括所有参数，但不包含开头的"/"
            char full_path[1024];
            if (path_start[0] == '/') {
                strncpy(full_path, path_start + 1, sizeof(full_path) - 1);
            } else {
                strncpy(full_path, path_start, sizeof(full_path) - 1);
            }
            full_path[sizeof(full_path) - 1] = '\0';
            
            ESP_LOGD(TAG, "提取的完整路径: %s", full_path);
            
            // 直接对完整路径进行URL编码，用于构建convert_music_image.php请求
            char encoded_path[2048];
            url_encode(full_path, encoded_path);
            ESP_LOGD(TAG, "编码后的路径: %s", encoded_path);
            
            char convert_url[4096]; // 增加缓冲区大小以避免溢出
            // 使用更可靠的方式构建URL
            snprintf(convert_url, sizeof(convert_url), 
                     "http://192.168.1.218/convert_music_image.php?music_path=%s&format=jpg&width=%d&height=%d",
                     encoded_path, req_width, req_height);
                     
            ESP_LOGI(TAG, "下载地址: %s", convert_url);
            
            size_t data_len = 0;
            uint8_t* raw_data = download_url_to_buffer(convert_url, &data_len);
            
            if (raw_data && data_len > 0) {
                ESP_LOGI(TAG, "下载成功，数据长度: %d bytes", data_len);
                
                init_cache_mutex();
                xSemaphoreTake(s_cache_mutex, portMAX_DELAY);
                
                ESP_LOGD(TAG, "当前缓存URL: %s", s_current_cache_url);
                ESP_LOGD(TAG, "新请求URL: %s", raw_url);
                
                if (strcmp(s_current_cache_url, raw_url) != 0) {
                    // 更新缓存URL
                    ESP_LOGD(TAG, "更新缓存URL: %s", raw_url);
                    strncpy(s_current_cache_url, raw_url, sizeof(s_current_cache_url) - 1);
                }

                int out_w, out_h;
                ESP_LOGI(TAG, "开始解码JPEG图片");
                uint16_t* rgb565 = decode_jpg_to_rgb565(raw_data, data_len, &out_w, &out_h);
                heap_caps_free(raw_data);
                
                if (rgb565) {
                    ESP_LOGI(TAG, "JPEG解码成功，尺寸: %dx%d", out_w, out_h);
                    
                    // 保存旧的缓存数据，以便在UI更新后释放
                    uint16_t* old_buffer = s_image_caches[0].buffer;
                    
                    // 更新缓存
                    s_image_caches[0].buffer = rgb565;
                    s_image_caches[0].width = out_w;
                    s_image_caches[0].height = out_h;
                    s_image_caches[0].is_in_use = true; // 标记为正在使用
                    
                    // 发送 UI 更新事件
                    ui_update_t *uu = malloc(sizeof(ui_update_t));
                    if (uu) {
                        uu->type = UI_UPDATE_TYPE_ALBUM_ART;
                        uu->value.ptr_value = rgb565;
                        ESP_LOGI(TAG, "发送UI更新事件，数据指针: %p", rgb565);
                        event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
                        
                        // 等待一小段时间，确保UI线程已经获取到新的图片数据
                        vTaskDelay(pdMS_TO_TICKS(100));
                        
                        // 现在可以安全释放旧的缓存数据
                        if (old_buffer && old_buffer != rgb565) {
                            ESP_LOGD(TAG, "释放旧的缓存数据: %p", old_buffer);
                            heap_caps_free(old_buffer);
                        }
                    } else {
                        ESP_LOGE(TAG, "创建UI更新事件失败");
                        heap_caps_free(rgb565);
                        s_image_caches[0].buffer = old_buffer; // 恢复旧的缓存
                        if (s_image_caches[0].buffer) {
                            s_image_caches[0].is_in_use = true;
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "JPEG解码失败");
                }
                xSemaphoreGive(s_cache_mutex);
            } else {
                ESP_LOGE(TAG, "下载失败，数据为空或长度为0");
                if (raw_data) {
                    heap_caps_free(raw_data);
                }
            }
            
            ESP_LOGI(TAG, "专辑封面请求处理完成");
        }
    }
    vTaskDelete(NULL);
}

void request_album_art_update(const char* raw_url, int width, int height, bool is_background) {
    ESP_LOGI(TAG, "请求更新专辑封面，URL: %s", raw_url);
    
    if (!raw_url || strlen(raw_url) == 0) {
        ESP_LOGE(TAG, "无效的URL");
        return;
    }
    
    // 检查缓存是否命中
    init_cache_mutex();
    xSemaphoreTake(s_cache_mutex, portMAX_DELAY);
    if (strcmp(s_current_cache_url, raw_url) == 0) {
        if (s_image_caches[0].buffer != NULL) {
            ESP_LOGI(TAG, "Cache hit for URL: %s", raw_url);
            s_image_caches[0].is_in_use = true; // 标记为正在使用
            ui_update_t *uu = malloc(sizeof(ui_update_t));
            if (uu) {
                uu->type = UI_UPDATE_TYPE_ALBUM_ART;
                uu->value.ptr_value = s_image_caches[0].buffer;
                event_system_post(EVENT_TYPE_UI_UPDATE, uu, sizeof(ui_update_t));
            } else {
                ESP_LOGE(TAG, "创建UI更新事件失败");
            }
            xSemaphoreGive(s_cache_mutex);
            return;
        }
    }
    xSemaphoreGive(s_cache_mutex);

    // 初始化队列（如果尚未初始化）
    init_album_art_queue();
    
    // 如果任务尚未创建，尝试创建单例任务
    if (s_album_art_task_handle == NULL) {
        int retry_count = 0;
        while (retry_count < 3) {
            if (xTaskCreate(album_art_task, "album_art_dl", 10240, NULL, 2, &s_album_art_task_handle) == pdPASS) {
                ESP_LOGI(TAG, "创建专辑封面单例任务成功");
                break;
            } else {
                retry_count++;
                ESP_LOGE(TAG, "创建专辑封面任务失败，重试次数: %d", retry_count);
                vTaskDelay(pdMS_TO_TICKS(100)); // 短暂延迟后重试
            }
        }
        // 即使任务创建失败，也继续执行，尝试将请求发送到队列中
        // 任务可能在后续请求中成功创建
    }
    
    // 准备请求
    album_art_request_t request;
    strncpy(request.url, raw_url, sizeof(request.url) - 1);
    request.url[sizeof(request.url) - 1] = '\0';
    request.width = width;
    request.height = height;
    request.is_background = is_background;
    
    // 发送请求到队列
    if (xQueueSend(s_album_art_queue, &request, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGE(TAG, "发送专辑封面请求到队列失败");
    } else {
        ESP_LOGI(TAG, "专辑封面请求已加入队列");
    }
}