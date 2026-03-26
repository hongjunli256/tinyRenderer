# TinyRenderer - 软光栅渲染器

独立从零实现的软件光栅化渲染器，基于经典 tinyrenderer 框架，使用纯 C++ 实现。

## 项目亮点
- 已通核心渲染管线，支持**正确 TGA 图像输出**
- 实现**透视校正**、**Z-Buffer** 深度测试
- 已完成 **ImGui 实时参数面板**（支持开关与调参）
- 正在实现 **SSAO**（屏幕空间环境遮蔽），显著提升画面真实感
- 可完整演示渲染流程

## 效果展示
（等你4.20前放上效果图，这里先留占位）1
![render1](images/result1.tga)

## 编译运行
```bash
mkdir build && cd build
cmake ..
make
./tinyrenderer"# tinyrenderer"  
