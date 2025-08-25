#!/usr/bin/env python3
"""
测试Desktop SparkBot配置和编译
"""

import os
import subprocess
import sys

def run_command(cmd, description):
    """运行命令并显示结果"""
    print(f"\n🔧 {description}")
    print(f"执行: {cmd}")
    print("-" * 50)

    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=300)

        if result.returncode == 0:
            print("✅ 成功")
            if result.stdout.strip():
                print("输出:")
                print(result.stdout[:1000] + ("..." if len(result.stdout) > 1000 else ""))
        else:
            print("❌ 失败")
            print("错误输出:")
            print(result.stderr[:1000] + ("..." if len(result.stderr) > 1000 else ""))
            return False

    except subprocess.TimeoutExpired:
        print("⏰ 超时")
        return False
    except Exception as e:
        print(f"💥 异常: {e}")
        return False

    return True

def create_test_sdkconfig():
    """创建测试用的sdkconfig文件"""
    print("\n📝 创建测试sdkconfig文件...")

    # 读取现有的defaults文件
    defaults_content = ""
    if os.path.exists("sdkconfig.defaults"):
        with open("sdkconfig.defaults", 'r') as f:
            defaults_content = f.read()

    # 添加Desktop SparkBot配置
    test_config = defaults_content + """
# Desktop SparkBot 测试配置
CONFIG_BOARD_TYPE_DESKTOP_SPARKBOT=y

# 确保ESP32-S3目标
CONFIG_IDF_TARGET="esp32s3"

# 启用PSRAM（Desktop SparkBot需要）
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# LVGL相关配置（由Kconfig自动选择）
CONFIG_LV_USE_GIF=y
CONFIG_LV_GIF_CACHE_DECODE_DATA=y
"""

    # 写入测试配置文件
    with open("sdkconfig.test", 'w') as f:
        f.write(test_config)

    print("✅ 测试配置文件已创建: sdkconfig.test")
    return True

def test_config_parsing():
    """测试配置文件是否能被正确解析"""
    print("\n🧪 测试配置解析...")

    # 备份原配置
    if os.path.exists("sdkconfig"):
        os.rename("sdkconfig", "sdkconfig.backup")

    # 使用测试配置
    if os.path.exists("sdkconfig.test"):
        os.rename("sdkconfig.test", "sdkconfig")

    # 测试reconfigure
    success = run_command("idf.py reconfigure", "重新配置项目")

    # 检查生成的配置
    if success and os.path.exists("build/config/sdkconfig.h"):
        with open("build/config/sdkconfig.h", 'r') as f:
            config_content = f.read()

        if "CONFIG_BOARD_TYPE_DESKTOP_SPARKBOT" in config_content:
            print("✅ Desktop SparkBot配置已正确解析")
        else:
            print("❌ 配置未被正确解析")
            success = False

    # 恢复原配置
    if os.path.exists("sdkconfig"):
        os.rename("sdkconfig", "sdkconfig.test")
    if os.path.exists("sdkconfig.backup"):
        os.rename("sdkconfig.backup", "sdkconfig")

    return success

def test_build():
    """测试编译"""
    print("\n🔨 测试编译...")

    # 使用测试配置进行编译测试
    if os.path.exists("sdkconfig.test"):
        os.rename("sdkconfig.test", "sdkconfig")

    # 尝试编译（只编译main组件以节省时间）
    success = run_command("idf.py build --component main", "编译main组件")

    # 检查是否生成了目标文件
    if success:
        desktop_obj = "build/esp-idf/main/CMakeFiles/__idf_main.dir/boards/desktop-sparkbot/"
        if os.path.exists(desktop_obj):
            print("✅ Desktop SparkBot板卡文件已编译")
        else:
            print("⚠️  Desktop SparkBot对象文件未找到，但编译成功")

    return success

def main():
    print("🚀 Desktop SparkBot 配置和编译测试")
    print("=" * 60)

    # 检查环境
    if not os.path.exists("main/boards/desktop-sparkbot"):
        print("❌ Desktop SparkBot目录不存在")
        return False

    if subprocess.run("which idf.py", shell=True, capture_output=True).returncode != 0:
        print("❌ ESP-IDF环境未设置，请先运行: source ~/esp/v5.4.1/esp-idf/export.sh")
        return False

    # 运行测试
    tests = [
        ("创建测试配置", create_test_sdkconfig),
        ("配置解析测试", test_config_parsing),
        ("编译测试", test_build)
    ]

    results = []
    for test_name, test_func in tests:
        print(f"\n{'='*20} {test_name} {'='*20}")
        success = test_func()
        results.append((test_name, success))

    # 总结
    print("\n" + "="*60)
    print("📊 测试结果总结:")

    all_passed = True
    for test_name, success in results:
        status = "✅ 通过" if success else "❌ 失败"
        print(f"  {status} {test_name}")
        if not success:
            all_passed = False

    if all_passed:
        print("\n🎉 所有测试通过！Desktop SparkBot已成功集成到ESP-IDF系统中。")
        print("\n📋 现在您可以:")
        print("1. 运行 'idf.py menuconfig' 在图形界面中选择'Desktop SparkBot 桌面机器人'")
        print("2. 运行 'idf.py build' 进行完整编译")
        print("3. 运行 'idf.py flash monitor' 烧录到硬件")
    else:
        print("\n💥 部分测试失败，请检查配置。")

    # 清理测试文件
    for test_file in ["sdkconfig.test", "sdkconfig.backup"]:
        if os.path.exists(test_file):
            os.remove(test_file)

    return all_passed

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
