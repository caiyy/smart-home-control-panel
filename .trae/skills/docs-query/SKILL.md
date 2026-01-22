---
name: "docs-query"
description: "查询官方文档，获取LVGL、ESP-IDF等框架的最新信息和最佳实践。当用户需要了解特定API、功能或解决方案时调用。"
---

# 官方文档查询器

## 功能

该技能用于查询官方文档，获取有关LVGL、ESP-IDF等框架的最新信息、API参考和最佳实践。

## 使用场景

- 当用户需要了解特定LVGL API的用法时
- 当用户需要ESP-IDF开发的最佳实践时
- 当用户需要解决特定框架问题时
- 当用户需要了解框架最新特性时

## 查询方法

1. **直接提问**：描述你需要查询的内容，例如"如何在LVGL中创建按钮？"
2. **API查询**：指定API名称，例如"查询lv_obj_get_child_by_name函数"
3. **功能查询**：描述所需功能，例如"如何实现LVGL按钮按下效果？"
4. **问题解决**：描述遇到的问题，例如"为什么lv_obj_get_child_by_name返回NULL？"

## 支持的框架

- LVGL 9
- ESP-IDF 5.x
- 其他相关嵌入式框架

## 示例

### 查询API

**用户**：查询lv_obj_get_child_by_name函数

**技能**：
`lv_obj_get_child_by_name`函数用于通过名称查找子对象，语法为：
```c
lv_obj_t * lv_obj_get_child_by_name(const lv_obj_t * parent, const char * name_path);
```
- `parent`：父对象指针
- `name_path`：对象名称路径，例如"child_name"或"parent/child/grandchild"
- 返回值：找到的对象指针，未找到返回NULL

### 查询功能

**用户**：如何实现LVGL按钮按下效果？

**技能**：
在LVGL中实现按钮按下效果有两种方式：
1. **XML样式定义**：在XML中添加按下状态样式
   ```xml
   <style name="btn_pressed" bg_color="#005a9e" selector="pressed" />
   ```
2. **C代码实现**：在回调函数中手动切换样式
   ```c
   lv_obj_set_style_bg_color(btn, lv_color_hex(0x005a9e), LV_PART_MAIN);
   ```

### 问题解决

**用户**：为什么lv_obj_get_child_by_name返回NULL？

**技能**：
可能的原因：
1. 对象名称不匹配
2. 对象还未创建
3. 使用了错误的父对象
4. XML中使用了id属性而不是name属性
5. 标签名错误，例如使用lv_btn而不是lv_button

## 注意事项

- 查询结果基于官方文档和最新框架版本
- 对于特定版本的问题，请注明框架版本
- 复杂问题可能需要多个查询步骤

## 最佳实践

- 提供清晰、具体的查询内容
- 包含相关代码片段或上下文
- 说明使用的框架版本
- 描述预期的行为和实际遇到的问题