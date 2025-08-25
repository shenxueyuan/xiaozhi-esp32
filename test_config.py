#!/usr/bin/env python3
"""
验证Desktop SparkBot配置是否正确集成到ESP-IDF系统中
"""

import os
import re

def check_kconfig():
    """检查Kconfig.projbuild中的配置"""
    kconfig_path = "main/Kconfig.projbuild"

    if not os.path.exists(kconfig_path):
        return False, "Kconfig.projbuild文件不存在"

    with open(kconfig_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 检查BOARD_TYPE_DESKTOP_SPARKBOT配置
    if "BOARD_TYPE_DESKTOP_SPARKBOT" not in content:
        return False, "未找到BOARD_TYPE_DESKTOP_SPARKBOT配置"

    if "Desktop SparkBot 桌面机器人" not in content:
        return False, "未找到Desktop SparkBot描述"

    if "select LV_USE_GIF" not in content:
        return False, "未启用LVGL GIF支持"

    if "select LV_GIF_CACHE_DECODE_DATA" not in content:
        return False, "未启用LVGL GIF缓存支持"

    return True, "Kconfig配置正确"

def check_cmake():
    """检查CMakeLists.txt中的配置"""
    cmake_path = "main/CMakeLists.txt"

    if not os.path.exists(cmake_path):
        return False, "CMakeLists.txt文件不存在"

    with open(cmake_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 检查CONFIG_BOARD_TYPE_DESKTOP_SPARKBOT条件分支
    if "CONFIG_BOARD_TYPE_DESKTOP_SPARKBOT" not in content:
        return False, "未找到CONFIG_BOARD_TYPE_DESKTOP_SPARKBOT分支"

    if 'set(BOARD_TYPE "desktop-sparkbot")' not in content:
        return False, "未设置desktop-sparkbot目录名"

    return True, "CMake配置正确"

def check_board_files():
    """检查板卡文件是否存在"""
    board_dir = "main/boards/desktop-sparkbot"

    if not os.path.exists(board_dir):
        return False, "desktop-sparkbot目录不存在"

    required_files = [
        "config.h",
        "config.json",
        "desktop_sparkbot_board.cc",
        "motor_controller.h",
        "motor_controller.cc",
        "fullscreen_emoji_display.h",
        "fullscreen_emoji_display.cc",
        "emotion_action_controller.h",
        "emotion_action_controller.cc",
        "README.md"
    ]

    missing_files = []
    for file in required_files:
        if not os.path.exists(os.path.join(board_dir, file)):
            missing_files.append(file)

    if missing_files:
        return False, f"缺少文件: {', '.join(missing_files)}"

    return True, "所有板卡文件存在"

def check_includes():
    """检查关键头文件包含"""
    board_file = "main/boards/desktop-sparkbot/desktop_sparkbot_board.cc"

    if not os.path.exists(board_file):
        return False, "主板文件不存在"

    with open(board_file, 'r', encoding='utf-8') as f:
        content = f.read()

    required_includes = [
        "#include <font_emoji.h>",
        "#include \"font_awesome_symbols.h\"",
        "#include \"fullscreen_emoji_display.h\"",
        "#include \"motor_controller.h\"",
        "#include \"emotion_action_controller.h\""
    ]

    missing_includes = []
    for include in required_includes:
        if include not in content:
            missing_includes.append(include)

    if missing_includes:
        return False, f"缺少头文件: {', '.join(missing_includes)}"

    return True, "头文件包含正确"

def main():
    print("🔍 验证Desktop SparkBot配置集成...")
    print("=" * 50)

    tests = [
        ("Kconfig配置", check_kconfig),
        ("CMake配置", check_cmake),
        ("板卡文件", check_board_files),
        ("头文件包含", check_includes)
    ]

    all_passed = True

    for test_name, test_func in tests:
        success, message = test_func()
        status = "✅" if success else "❌"
        print(f"{status} {test_name}: {message}")

        if not success:
            all_passed = False

    print("=" * 50)
    if all_passed:
        print("🎉 所有配置检查通过！Desktop SparkBot已成功集成到ESP-IDF系统中。")
        print("\n📋 下一步操作:")
        print("1. 运行 'idf.py set-target esp32s3' 设置目标芯片")
        print("2. 运行 'idf.py menuconfig' 选择 'Desktop SparkBot 桌面机器人'")
        print("3. 配置其他选项后运行 'idf.py build' 编译")
        print("4. 运行 'idf.py flash monitor' 烧录和监控")
    else:
        print("💥 配置检查失败！请修复上述问题后重试。")

    return all_passed

if __name__ == "__main__":
    success = main()
    exit(0 if success else 1)
