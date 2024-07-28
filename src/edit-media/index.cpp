#include "./edit-media.cpp"
#include <node_api.h>
#include <assert.h>
#include <cstring>
#include <vector>
#include <iostream>

// Node.js Addon 函数，用于接收 JavaScript 数组并调用 C++ 主函数
napi_value editMedia(napi_env env, napi_callback_info info) {
    napi_status status;
    
    size_t argc = 1;
    napi_value js_array = nullptr;
    napi_get_cb_info(env, info, &argc, &js_array, nullptr, nullptr);

    // 将 JavaScript 数组转换为 C++ 标准库中的 vector
    std::vector<std::string> cpp_strings;
    
    napi_valuetype valuetype;
    status = napi_typeof(env, js_array, &valuetype);
    bool isArray;
    napi_is_array(env, js_array, &isArray);

    if (status != napi_ok || !isArray) {
        napi_throw_type_error(env, nullptr, "Expected an array");
        return nullptr;
    }

    uint32_t length;
    status = napi_get_array_length(env, js_array, &length);
    if (status != napi_ok) {
        napi_throw_type_error(env, nullptr, "Failed to get array length");
        return nullptr;
    }

    for (size_t i = 0; i < length; ++i) {
        napi_value element;
        status = napi_get_element(env, js_array, i, &element);

        if (status != napi_ok) {
            napi_throw_type_error(env, nullptr, "Failed to get element from array");
            return nullptr;
        }

        napi_value str_value;
        status = napi_coerce_to_string(env, element, &str_value);

        if (status != napi_ok) {
            napi_throw_type_error(env, nullptr, "Expected a string");
            return nullptr;
        }

        size_t str_len;
        char *str;
        status = napi_get_value_string_utf8(env, str_value, nullptr, 0, &str_len);
        if (status != napi_ok) {
            napi_throw_type_error(env, nullptr, "Failed to get string length");
            return nullptr;
        }

        str = new char[str_len + 1];
        status = napi_get_value_string_utf8(env, str_value, str, str_len + 1, &str_len);
        if (status != napi_ok) {
            napi_throw_type_error(env, nullptr, "Failed to get string value");
            return nullptr;
        }

        str[str_len] = '\0'; // 确保字符串以空字符结尾
        cpp_strings.push_back(str);
    }

    // 创建 char *argv[] 数组
    std::vector<char *> argv(cpp_strings.size() + 1);
    for (size_t i = 0; i < cpp_strings.size(); ++i) {
        argv[i] = const_cast<char *>(cpp_strings[i].c_str());
    }
    argv.back() = nullptr; // 结束标志

    // 调用 C++ 主函数
    int ret = main(static_cast<int>(cpp_strings.size()), argv.data());

    napi_value result;
    const char *str = (ret == 0 ? "success" : "error");

    napi_create_string_utf8(env, str, strlen(str), &result);
    return result;
}

#define DECLARE_NAPI_METHOD(name, func)                                        \
  { name, 0, func, 0, 0, 0, napi_default, 0 }

// 初始化模块
napi_value Init(napi_env env, napi_value exports) {
    napi_status status;
    napi_property_descriptor desc = DECLARE_NAPI_METHOD("editMedia", editMedia);
    status = napi_define_properties(env, exports, 1, &desc);
    assert(status == napi_ok);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)