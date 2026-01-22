# 实现MQTT服务功能计划

## 1. 项目配置更新

* 更新`idf_component.yml`，添加ESP-IDF MQTT组件依赖

## 2. 创建MQTT相关文件

* 创建`include/mqtt_client.h`：定义MQTT客户端配置、API和事件类型

* 创建`src/mqtt_client.c`：实现MQTT客户端功能

## 3. 事件系统扩展

* 在`event_system.h`中添加MQTT相关事件类型

  * `EVENT_TYPE_MQTT_CONNECTED`：MQTT连接成功事件

  * `EVENT_TYPE_MQTT_DISCONNECTED`：MQTT断开连接事件

  * `EVENT_TYPE_MQTT_MESSAGE_RECEIVED`：MQTT消息接收事件

## 4. MQTT客户端实现

* **MQTT配置**：使用提供的服务器参数（192.158.1.115:1883, caiyy, yangyong1229）

* **初始化函数**：`mqtt_client_init()`，配置MQTT客户端参数

* **连接管理**：

  * `mqtt_client_start()`：启动MQTT客户端

  * `mqtt_client_stop()`：停止MQTT客户端

  * 自动重连机制

* **订阅功能**：订阅主题`homeassistant/sensor/esp32_music_player/lyrics/state`

* **消息处理**：实现MQTT消息回调函数，将收到的消息通过事件队列传递

## 5. 系统集成

* 在`smart_control_panel_main.c`的WiFi连接成功事件中启动MQTT客户端

* 在事件处理任务中添加MQTT事件处理逻辑

* 确保MQTT客户端与事件系统无缝集成

## 6. 代码结构

```
main/
├── include/
│   ├── event_system.h       # 更新：添加MQTT事件类型
│   └── mqtt_client.h        # 新增：MQTT客户端头文件
└── src/
    ├── event_system.c       # 更新：处理MQTT事件
    ├── mqtt_client.c        # 新增：MQTT客户端实现
    └── smart_control_panel_main.c  # 更新：集成MQTT客户端
```

## 7. 关键实现要点

* 严格遵循项目规则，使用全局事件队列处理所有MQTT事件

* 确保代码仅包含必要功能，不添加额外操作

* 使用中文日志输出

* 处理内存泄漏问题，确保资源正确释放

* 与现有WiFi功能良好集成

## 8. 测试验证

* 编译项目，确保无编译错误

* 验证MQTT客户端能正确连接到服务器

* 验证MQTT客户端能正确订阅指定主题

* 验证收到的MQTT消息能正确通过事件队列传递

