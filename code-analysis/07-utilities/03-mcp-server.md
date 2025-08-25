# MCP服务器系统 (McpServer) 详解

## 📁 模块概览
- **功能**: Model Context Protocol (MCP) 工具服务器实现
- **设计模式**: 单例模式 + 工厂模式 + 策略模式
- **核心特性**: 动态工具注册、JSON-RPC协议、异步执行
- **扩展性**: 支持自定义工具开发和热插拔

## 🏗️ 架构设计

### 核心组件层次
```
McpServer (单例服务器)
├── McpTool (工具定义)
│   ├── PropertyList (参数列表)
│   └── Property (单个参数)
└── Tool Execution Engine (执行引擎)
```

## 🎯 核心类详细分析

### Property 参数定义类
```cpp
enum PropertyType {
    kPropertyTypeBoolean,        // 布尔类型
    kPropertyTypeInteger,        // 整数类型
    kPropertyTypeString          // 字符串类型
};

class Property {
private:
    std::string name_;                           // 参数名称
    PropertyType type_;                          // 参数类型
    std::variant<bool, int, std::string> value_; // 参数值
    bool has_default_value_;                     // 是否有默认值
    std::optional<int> min_value_;               // 整数最小值
    std::optional<int> max_value_;               // 整数最大值

public:
    // 必需参数构造函数
    Property(const std::string& name, PropertyType type);

    // 可选参数构造函数 (带默认值)
    template<typename T>
    Property(const std::string& name, PropertyType type, const T& default_value);

    // 整数范围限制构造函数
    Property(const std::string& name, PropertyType type, int min_value, int max_value);
    Property(const std::string& name, PropertyType type, int default_value,
             int min_value, int max_value);

    // 访问器方法
    inline const std::string& name() const { return name_; }
    inline PropertyType type() const { return type_; }
    inline bool has_default_value() const { return has_default_value_; }
    inline bool has_range() const { return min_value_.has_value() && max_value_.has_value(); }

    // 值操作
    template<typename T> T value() const { return std::get<T>(value_); }
    template<typename T> void set_value(const T& value);

    // JSON序列化
    std::string to_json() const;
};
```

#### Property使用示例
```cpp
// 必需的字符串参数
Property name_prop("name", kPropertyTypeString);

// 可选的布尔参数 (默认false)
Property enable_prop("enable", kPropertyTypeBoolean, false);

// 整数参数带范围限制 (1-100)
Property volume_prop("volume", kPropertyTypeInteger, 50, 1, 100);

// 参数JSON输出示例
{
    "type": "integer",
    "default": 50,
    "minimum": 1,
    "maximum": 100
}
```

### PropertyList 参数列表管理
```cpp
class PropertyList {
private:
    std::vector<Property> properties_;

public:
    PropertyList() = default;
    PropertyList(const std::vector<Property>& properties);

    // 参数管理
    void AddProperty(const Property& property);
    const Property& operator[](const std::string& name) const;

    // 迭代器支持
    auto begin() { return properties_.begin(); }
    auto end() { return properties_.end(); }

    // 必需参数列表
    std::vector<std::string> GetRequired() const;

    // JSON Schema生成
    std::string to_json() const;
};
```

#### PropertyList使用示例
```cpp
PropertyList properties({
    Property("message", kPropertyTypeString),                    // 必需参数
    Property("urgent", kPropertyTypeBoolean, false),             // 可选参数
    Property("retry_count", kPropertyTypeInteger, 3, 1, 10)      // 范围参数
});

// 生成的JSON Schema
{
    "message": { "type": "string" },
    "urgent": { "type": "boolean", "default": false },
    "retry_count": { "type": "integer", "default": 3, "minimum": 1, "maximum": 10 }
}
```

### McpTool 工具定义类
```cpp
using ReturnValue = std::variant<bool, int, std::string>;

class McpTool {
private:
    std::string name_;                                           // 工具名称
    std::string description_;                                    // 工具描述
    PropertyList properties_;                                    // 参数定义
    std::function<ReturnValue(const PropertyList&)> callback_;  // 回调函数

public:
    McpTool(const std::string& name,
            const std::string& description,
            const PropertyList& properties,
            std::function<ReturnValue(const PropertyList&)> callback);

    // 访问器
    inline const std::string& name() const { return name_; }
    inline const std::string& description() const { return description_; }
    inline const PropertyList& properties() const { return properties_; }

    // 工具调用
    std::string Call(const PropertyList& properties);

    // JSON Schema生成
    std::string to_json() const;
};
```

#### McpTool JSON Schema示例
```json
{
    "name": "send_message",
    "description": "发送消息到指定用户",
    "inputSchema": {
        "type": "object",
        "properties": {
            "message": { "type": "string" },
            "urgent": { "type": "boolean", "default": false },
            "retry_count": { "type": "integer", "default": 3, "minimum": 1, "maximum": 10 }
        },
        "required": ["message"]
    }
}
```

### McpServer 服务器核心
```cpp
class McpServer {
public:
    // 单例模式
    static McpServer& GetInstance() {
        static McpServer instance;
        return instance;
    }

    // 工具管理
    void AddCommonTools();                                       // 添加通用工具
    void AddTool(McpTool* tool);                                // 添加工具对象
    void AddTool(const std::string& name,
                 const std::string& description,
                 const PropertyList& properties,
                 std::function<ReturnValue(const PropertyList&)> callback);

    // 消息处理
    void ParseMessage(const cJSON* json);                       // 解析JSON消息
    void ParseMessage(const std::string& message);             // 解析字符串消息

private:
    McpServer();
    ~McpServer();

    // 协议处理
    void ParseCapabilities(const cJSON* capabilities);
    void ReplyResult(int id, const std::string& result);
    void ReplyError(int id, const std::string& message);

    // 工具操作
    void GetToolsList(int id, const std::string& cursor);
    void DoToolCall(int id, const std::string& tool_name,
                    const cJSON* tool_arguments, int stack_size);

    // 成员变量
    std::vector<McpTool*> tools_;                               // 工具列表
    std::thread tool_call_thread_;                              // 工具执行线程
};
```

## 🔧 工具注册和管理

### 通用工具注册
```cpp
void McpServer::AddCommonTools() {
    // 设备信息工具
    AddTool("get_device_info", "获取设备信息",
        PropertyList(), [](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            cJSON* info = cJSON_CreateObject();

            cJSON_AddStringToObject(info, "board_type", board.GetBoardType().c_str());
            cJSON_AddStringToObject(info, "uuid", board.GetUuid().c_str());
            cJSON_AddStringToObject(info, "mac_address", SystemInfo::GetMacAddress().c_str());

            float temperature;
            if (board.GetTemperature(temperature)) {
                cJSON_AddNumberToObject(info, "temperature", temperature);
            }

            int battery_level;
            bool charging, discharging;
            if (board.GetBatteryLevel(battery_level, charging, discharging)) {
                cJSON_AddNumberToObject(info, "battery_level", battery_level);
                cJSON_AddBoolToObject(info, "charging", charging);
                cJSON_AddBoolToObject(info, "discharging", discharging);
            }

            char* json_str = cJSON_PrintUnformatted(info);
            std::string result(json_str);
            cJSON_free(json_str);
            cJSON_Delete(info);

            return result;
        });

    // 网络状态工具
    AddTool("get_network_status", "获取网络连接状态",
        PropertyList(), [](const PropertyList& properties) -> ReturnValue {
            auto* network = Board::GetInstance().GetNetwork();
            if (network && network->IsConnected()) {
                return std::string("网络已连接: " + network->GetConnectionInfo());
            } else {
                return std::string("网络未连接");
            }
        });

    // 系统重启工具
    AddTool("reboot_device", "重启设备",
        PropertyList({
            Property("confirm", kPropertyTypeBoolean, false)
        }), [](const PropertyList& properties) -> ReturnValue {
            bool confirm = properties["confirm"].value<bool>();
            if (confirm) {
                esp_restart();
                return std::string("设备正在重启...");
            } else {
                return std::string("请设置confirm=true确认重启");
            }
        });
}
```

### 自定义工具示例

#### 音频控制工具
```cpp
void AudioService::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    // 音量控制工具
    mcp_server.AddTool("set_volume", "设置音量大小",
        PropertyList({
            Property("volume", kPropertyTypeInteger, 50, 0, 100)
        }), [this](const PropertyList& properties) -> ReturnValue {
            int volume = properties["volume"].value<int>();
            if (audio_codec_) {
                audio_codec_->SetOutputVolume(volume);
                return std::string("音量已设置为: " + std::to_string(volume));
            }
            return std::string("音频编解码器不可用");
        });

    // 音频格式查询工具
    mcp_server.AddTool("get_audio_format", "获取音频格式信息",
        PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            if (audio_codec_) {
                cJSON* format = cJSON_CreateObject();
                cJSON_AddNumberToObject(format, "input_sample_rate",
                                       audio_codec_->input_sample_rate());
                cJSON_AddNumberToObject(format, "output_sample_rate",
                                       audio_codec_->output_sample_rate());
                cJSON_AddNumberToObject(format, "input_channels",
                                       audio_codec_->input_channels());
                cJSON_AddNumberToObject(format, "output_channels",
                                       audio_codec_->output_channels());
                cJSON_AddBoolToObject(format, "duplex", audio_codec_->duplex());

                char* json_str = cJSON_PrintUnformatted(format);
                std::string result(json_str);
                cJSON_free(json_str);
                cJSON_Delete(format);

                return result;
            }
            return std::string("音频编解码器不可用");
        });
}
```

#### 显示控制工具
```cpp
void Display::RegisterMcpTools() {
    auto& mcp_server = McpServer::GetInstance();

    // 显示通知工具
    mcp_server.AddTool("show_notification", "显示通知消息",
        PropertyList({
            Property("message", kPropertyTypeString),
            Property("duration", kPropertyTypeInteger, 3000, 1000, 10000)
        }), [this](const PropertyList& properties) -> ReturnValue {
            std::string message = properties["message"].value<std::string>();
            int duration = properties["duration"].value<int>();

            ShowNotification(message.c_str(), duration);
            return std::string("通知已显示: " + message);
        });

    // 主题切换工具
    mcp_server.AddTool("set_theme", "切换显示主题",
        PropertyList({
            Property("theme", kPropertyTypeString, "light")
        }), [this](const PropertyList& properties) -> ReturnValue {
            std::string theme = properties["theme"].value<std::string>();

            if (theme == "light" || theme == "dark") {
                SetTheme(theme);
                return std::string("主题已切换为: " + theme);
            } else {
                return std::string("不支持的主题: " + theme);
            }
        });
}
```

## 📡 JSON-RPC协议处理

### 消息解析流程
```cpp
void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Invalid JSON message");
        return;
    }

    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseMessage(const cJSON* json) {
    // 获取请求ID
    cJSON* id_json = cJSON_GetObjectItem(json, "id");
    int id = id_json ? id_json->valueint : -1;

    // 获取方法名
    cJSON* method_json = cJSON_GetObjectItem(json, "method");
    if (method_json == nullptr || !cJSON_IsString(method_json)) {
        ReplyError(id, "Missing or invalid method");
        return;
    }

    std::string method = method_json->valuestring;
    cJSON* params = cJSON_GetObjectItem(json, "params");

    // 路由到相应的处理方法
    if (method == "initialize") {
        HandleInitialize(id, params);
    } else if (method == "tools/list") {
        HandleToolsList(id, params);
    } else if (method == "tools/call") {
        HandleToolCall(id, params);
    } else {
        ReplyError(id, "Unknown method: " + method);
    }
}
```

### 工具调用处理
```cpp
void McpServer::HandleToolCall(int id, const cJSON* params) {
    // 解析工具名称
    cJSON* name_json = cJSON_GetObjectItem(params, "name");
    if (!name_json || !cJSON_IsString(name_json)) {
        ReplyError(id, "Missing tool name");
        return;
    }

    std::string tool_name = name_json->valuestring;

    // 查找工具
    McpTool* tool = nullptr;
    for (auto* t : tools_) {
        if (t->name() == tool_name) {
            tool = t;
            break;
        }
    }

    if (tool == nullptr) {
        ReplyError(id, "Tool not found: " + tool_name);
        return;
    }

    // 解析参数
    PropertyList call_properties;
    cJSON* arguments = cJSON_GetObjectItem(params, "arguments");
    if (arguments) {
        ParseToolArguments(tool->properties(), arguments, call_properties);
    }

    // 异步执行工具
    DoToolCall(id, tool_name, arguments, 8192);  // 8KB栈大小
}

void McpServer::DoToolCall(int id, const std::string& tool_name,
                          const cJSON* tool_arguments, int stack_size) {
    // 等待前一个工具执行完成
    if (tool_call_thread_.joinable()) {
        tool_call_thread_.join();
    }

    // 创建新的执行线程
    tool_call_thread_ = std::thread([this, id, tool_name, tool_arguments]() {
        try {
            // 查找并执行工具
            for (auto* tool : tools_) {
                if (tool->name() == tool_name) {
                    PropertyList properties;
                    ParseToolArguments(tool->properties(), tool_arguments, properties);

                    std::string result = tool->Call(properties);
                    ReplyResult(id, result);
                    return;
                }
            }

            ReplyError(id, "Tool not found: " + tool_name);
        } catch (const std::exception& e) {
            ReplyError(id, std::string("Tool execution error: ") + e.what());
        }
    });
}
```

### 响应格式化
```cpp
void McpServer::ReplyResult(int id, const std::string& result) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);

    cJSON* result_json = cJSON_Parse(result.c_str());
    if (result_json) {
        cJSON_AddItemToObject(response, "result", result_json);
    } else {
        // 如果不是JSON，作为字符串处理
        cJSON_AddStringToObject(response, "result", result.c_str());
    }

    char* response_str = cJSON_PrintUnformatted(response);

    // 发送到协议层
    Application::GetInstance().SendMcpResponse(response_str);

    cJSON_free(response_str);
    cJSON_Delete(response);
}

void McpServer::ReplyError(int id, const std::string& message) {
    cJSON* response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "id", id);

    cJSON* error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", -1);
    cJSON_AddStringToObject(error, "message", message.c_str());
    cJSON_AddItemToObject(response, "error", error);

    char* response_str = cJSON_PrintUnformatted(response);
    Application::GetInstance().SendMcpResponse(response_str);

    cJSON_free(response_str);
    cJSON_Delete(response);
}
```

## ⚡ 性能优化

### 异步执行机制
```cpp
// 工具在独立线程中执行，避免阻塞主线程
std::thread tool_call_thread_;

// 大栈空间支持复杂工具
DoToolCall(id, tool_name, arguments, 8192);  // 8KB栈

// 线程安全的结果返回
void SafeReplyResult(int id, const std::string& result) {
    std::lock_guard<std::mutex> lock(reply_mutex_);
    ReplyResult(id, result);
}
```

### 内存管理优化
```cpp
// 智能指针管理工具生命周期
std::vector<std::unique_ptr<McpTool>> managed_tools_;

// JSON对象自动释放
class JsonGuard {
    cJSON* json_;
public:
    JsonGuard(cJSON* json) : json_(json) {}
    ~JsonGuard() { if (json_) cJSON_Delete(json_); }
    cJSON* get() { return json_; }
};

// 使用示例
JsonGuard json_guard(cJSON_Parse(message.c_str()));
if (json_guard.get()) {
    ParseMessage(json_guard.get());
}
```

### 缓存优化
```cpp
// 工具列表缓存
std::string cached_tools_list_;
bool tools_list_dirty_ = true;

std::string GetToolsListJson() {
    if (tools_list_dirty_) {
        cJSON* tools_array = cJSON_CreateArray();
        for (auto* tool : tools_) {
            cJSON* tool_json = cJSON_Parse(tool->to_json().c_str());
            cJSON_AddItemToArray(tools_array, tool_json);
        }

        char* json_str = cJSON_PrintUnformatted(tools_array);
        cached_tools_list_ = json_str;
        cJSON_free(json_str);
        cJSON_Delete(tools_array);

        tools_list_dirty_ = false;
    }

    return cached_tools_list_;
}
```

## 🔗 与其他模块集成

### Application集成
```cpp
void Application::InitializeMcpServer() {
    auto& mcp_server = McpServer::GetInstance();

    // 添加通用工具
    mcp_server.AddCommonTools();

    // 各模块注册自己的工具
    audio_service_->RegisterMcpTools();
    Board::GetInstance().GetDisplay()->RegisterMcpTools();
    Board::GetInstance().GetCamera()->RegisterMcpTools();

    ESP_LOGI(TAG, "MCP Server initialized with %zu tools", mcp_server.GetToolsCount());
}

void Application::OnMcpMessage(const std::string& message) {
    auto& mcp_server = McpServer::GetInstance();
    mcp_server.ParseMessage(message);
}
```

### Protocol集成
```cpp
void Protocol::SendMcpMessage(const std::string& message) {
    // 构建MCP消息包
    cJSON* mcp_message = cJSON_CreateObject();
    cJSON_AddStringToObject(mcp_message, "type", "mcp");
    cJSON_AddStringToObject(mcp_message, "content", message.c_str());

    char* json_str = cJSON_PrintUnformatted(mcp_message);
    SendText(json_str);

    cJSON_free(json_str);
    cJSON_Delete(mcp_message);
}
```

---

**相关文档**:
- [Application核心控制](../02-main-core/02-application-class.md)
- [通信协议系统](../06-protocols/01-protocol-overview.md)
- [摄像头系统集成](./02-camera-system.md)
