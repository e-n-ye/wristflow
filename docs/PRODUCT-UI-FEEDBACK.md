# 2026-09-26 真机 UI 反馈与修复

用户确认已烧录最新产品固件，并提供扩散表盘、蜂窝菜单实拍和 Redmi Watch 4 列表/三列参考。调查基于 `codex/product-runtime` / `feabc1d268b63b92e14bc4f097c0f32f1e9370b8`。实施位于独立工作树 `C:/Users/13984/.codex/worktrees/product-ui-feedback/wristflow`、分支 `codex/product-ui-feedback`；已保留上一任务未提交的三份烧录交接文档，原工作树不改动。

## 用户已确认的修复方向

- 分页点随实际组件页数量与当前页变化，为后续 1–6 页增删共用同一来源；点组保持固定位置。
- 未校时时，在扩散小时位置显示白灰扩散风格大号 `--`，下方分钟显示 `--`，保留 USB；校时后自动恢复真实时间。
- 产品取消蜂窝菜单，采用 Redmi Watch 4 式列表及固定三列图标。默认列表；列表为左侧圆图标和右侧名称，三列仅图标，均纵向滚动。
- 从 `设置 → 应用布局` 切换列表/三列并保存，重启恢复；所有入口继续使用同一应用注册表。
- 最新 Redmi Watch 4 实拍补充：选择页两块大圆角选项，文案“多列布局”和“列表布局”；选中项右侧为蓝色底＋浅色实心点，未选中不显示标记。
- 先修复本轮反馈，再接组件配置保存、长按编辑、同尺寸替换、左右新增和整页删除；[既有共识](PRODUCT-UI-SCOPE.md) 中默认三页、1–6 页、两色、重复、立即保存/新页点勾等规则不重开访谈。

## 源码原因

对照版本中 `ui/xml/components/page_dots.xml:4` 起固定四个点；`ui/runtime/ui_shell.c:351` 只移动既有点组，没有根据配置生成数量。扩散表盘 `ui/runtime/default_watchface.c:14` 在缺时间时隐藏整张小时图片，下一行将分钟改为默认小字体，因此只剩很小的 `--:--`。

蜂窝 `screen_product_launcher.xml:3` 的菜单高度只有 402，底部名称占用剩余空间；`ui/runtime/apps.c:84` 根据距离调整圆背景，`:85` 把尺寸下限固定为 64，子图标仍使用固定 44 号字体。用户指出 SiFli 官方边缘会缩小至完全消失，此点已核实：固定 SDK `app_mainmenu.c:604` 的 `layout_icon_transform` 会将边缘半径减至零，`:1323` 起把不可见项宽高置零，`:745` 起隐藏对象。当前产品只是借用排列思路，没有复现完整官方行为。

## 开源候选与性能边界

- [SiFli v2.5.1 Watch 蜂窝](https://gitee.com/SiFli/sifli-sdk/blob/v2.5.1/example/multimedia/lvgl/watch/src/gui_apps/main/app_mainmenu.c)：文件 Apache-2.0；基于图像对象与 LVGL v8 API，并直接操作部分内部字段，不能原样搬入产品 LVGL 9.4。
- [LVGL 列表](https://github.com/lvgl/lvgl/blob/v9.3.0/examples/widgets/list/lv_example_list_1.c)、[三列网格](https://github.com/lvgl/lvgl/blob/v9.3.0/examples/layouts/grid/lv_example_grid_1.c)：LVGL MIT 许可。复用原生布局/滚动方法，实施继续使用锁定 SDK，不升级依赖。

此前 UI-TOUCH 只有静态首页启动一次 render 18ms、handler max 18ms 与亮度 2ms；没有蜂窝连续拖动样本。`ui_demo` 的 `[ui_perf]` 为毫秒级墙钟统计，render 含等待，flush 回调不等于 DMA 完成，frames 不等于面板帧率。当前 Product 尚无等效埋点。`BSP_USING_EPIC` 不能证明具体绘制任务已使用 GPU。

固定尺寸列表/网格移除了 Product 蜂窝逐帧尺寸与几何更新，但不能据此宣称真机已流畅或芯片瓶颈已解决。用户反馈不是全套交互通过。列表与三列使用 LVGL 原生滚动，固定大小；UI Demo 保留旧蜂窝演示。

## 实施与验证

页数由运行中组件配置决定，点组在屏幕底部固定，实际点数不含表盘；1–6 页以及新增/删除后的数量、选中颜色、居中位置均有主机指针测试。无时间时保留小时图片并使用 `hour_unknown` RGB565 资源，分钟为原 56 号字的 `--`；有效 RTC 自动恢复。设置记录仍为 24 B，v2 使用原表盘 ID 字段末尾空余字节保存菜单类型；v1 保留亮度/表盘并默认为列表。

官方 Editor 2.0.1 GUI 完成语义校验、C/字体/图片导出及 LVGL 9.4.0 预览。初次失败来自未支持的 XML `scroll_dir` 属性，改为运行层 `LV_DIR_VER`。主机菜单遍历用过大的固定拖动会反复越过目标，改用短拖动后两种布局的 28 个应用入口/返回与滚动位置保留全部通过。未降低断言或以点击事件注入代替菜单指针路径。

反馈阶段先通过 8/8 和两目标构建；随后同分支继续完成 [组件编辑保存](COMPONENT-EDITOR.md)，最终为 10/10，Product/UI Demo 产物与当前源码哈希一致。最新构建版本、完整 BIN 哈希、命令及原始日志目录见 [精简证据](evidence/2026-09-26/product-ui-feedback.json) 和 [构建入口](BUILD.md)。本机实际渲染位于当前工作树 `artifacts/host-feedback/renders/`，合图为 `artifacts/feedback-preview.png`，不是网页重画。

本轮未烧录。下一有界实验为板上列表/三列连续拖动、菜单开关与返回、缺时间表盘和 1–6 页编辑；记录最大 handler/render 时间、内存及布局 `write_ms`。设置记录迁移和布局字节截断的主机结果不能替代 FlashDB 擦除/GC 或物理断电实验。
