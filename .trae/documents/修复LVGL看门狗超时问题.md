# 问题分析

任务看门狗（Task Watchdog）被触发，IDLE1任务（CPU 1）没有及时重置看门狗，当前运行的任务是CPU 1上的lvgl_task。

## 根本原因
1. **LVGL任务中存在长时间阻塞操作**：`main_screen.c`中的`load_xml_from_network()`函数是一个阻塞操作，它会执行网络请求
2. **XML刷新回调在LVGL任务中执行**：`refresh_xml_cb()`函数在LVGL任务中被调用，会执行网络请求、清除屏幕、重新加载XML等耗时操作
3. **LVGL任务占用CPU 1所有时间**：导致IDLE1任务无法运行，看门狗定时器超时

## 解决方案

### 1. 将网络请求移出LVGL任务
- 创建专门的网络任务处理XML加载
- 使用事件队列在任务间传递消息

### 2. 优化XML刷新流程
- 将`load_xml_from_network()`函数移到后台执行
- 在LVGL任务中只处理UI更新，避免长时间阻塞

### 3. 修改文件

#### 3.1 main_screen.c
- 将`load_xml_from_network()`函数的调用移到事件处理中
- 修改`refresh_xml_cb()`函数，只发送事件而不直接执行网络请求
- 添加事件处理函数处理XML加载完成事件

#### 3.2 event_system.h/event_system.c
- 添加新的事件类型：`EVENT_TYPE_XML_REFRESH_REQUEST`和`EVENT_TYPE_XML_LOAD_COMPLETE`

#### 3.3 主任务文件
- 确保事件系统正确处理新的事件类型

### 4. 实现步骤
1. 定义新的事件类型
2. 修改`refresh_xml_cb()`函数，使其发送事件而非直接执行网络请求
3. 创建XML加载事件处理函数
4. 在事件系统中注册新的事件处理
5. 测试修复后的代码，确保看门狗不再触发

## 预期效果
- LVGL任务不再被长时间阻塞
- IDLE1任务能够正常运行
- 看门狗定时器不再超时
- 系统稳定性提高