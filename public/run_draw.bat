@echo off
chcp 65001 >nul
title iSperm Medical - 精子十字形态图绘制工具
echo ======================================================================
echo    iSperm Medical - AllSpermInfor 十字形态位置图生成器
echo ======================================================================
echo.

where python >nul 2>nul
if %errorlevel% neq 0 (
    echo [错误] 系统未检测到 Python 环境！
    echo 请先安装 Python (https://www.python.org/) 并勾选 "Add Python to PATH"。
    echo.
    pause
    exit /b 1
)

echo 正在运行 draw_sperm_crosses.py 绘制 1280x720 图像...
echo.
python "%~dp0draw_sperm_crosses.py" %*

echo.
echo ======================================================================
echo 绘制完毕！可直接用 Windows 照片或画图打开生成的图片：
echo   - sperm_cross_map_black_bg.png (黑色背景)
echo   - sperm_cross_map_white_bg.png (白色背景)
echo ======================================================================
echo.
pause
