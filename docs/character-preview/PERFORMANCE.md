# Character Preview 性能实测

Each 8s CSV, omit first60/final3 rows. Means and P95 are per rendered frame, not per capture. RenderThreadTime near zero in this Editor setup; also sum exclusive RenderThread scopes excluding EventWait. Editor/Slate/Python overhead and pre-existing world rendering are included.

环境与限制见 [VALIDATION.md](VALIDATION.md)。单位 ms，均值；Editor PIE 1280×720、RTX5070/Ryzen9600X。

| 场景 | Game Thread | Render Thread exclusive，无等待 | GPU |
|---|---:|---:|---:|
| 关闭，世界运行 | 4.975 | 2.171 | 1.836 |
| 暂停菜单，无预览 | 4.533 | 2.254 | 1.711 |
| 静态无动画，按需刷新 | 4.809 | 2.407 | 1.735 |
| 打开，待机动画 | 4.900 | 2.584 | 2.037 |
| 连续旋转 | 4.958 | 2.744 | 1.824 |
| 连续切换角色 | 5.379 | 2.629 | 1.950 |
| 关闭后，世界运行 | 5.389 | 2.382 | 1.716 |

`RenderThreadTime` 原始列异常接近零，完整保留在 JSON 中；上表使用 CSV 独占 RenderThread scope 之和并排除 EventWait，避免把无效指标解释成没有渲染成本。不是额外 Capture 的独立耗时。

待机/旋转实测 8 秒 240 次 Capture（30 Hz），关闭样本 0；静态无动画首次捕获后停止重复渲染，交互触发一次更新。

单轮短样本、Editor/Slate/Python、整个 World 与视口共同开销；并非 packaged/Shipping 基准。现有暂停策略不同，所以优先用“暂停菜单”比较“打开待机”，不能把相对运行世界的较低 Game Thread 值归因为预览优化。

原始 CSV：`Saved/Profiling/CSV/CharacterPreview_*.csv`；统计：`Saved/CharacterPreview/performance-summary.json`；元数据：`profile_scenarios.json`。静态样本来自最终构建的无动画注入检查；其余六场景来自最终构建复验。旧 R3 轮次单独保留在 `Saved/CharacterPreview/PerformanceR3/`。

资源切换的首次 Capture 在 Ready 通知前立即提交，不受后续定时器间隔约束；最终切换样本 8 秒 250 次（包含 20 次左右资源切换首帧），与定时待机/旋转的 30 Hz 区分。切换复测元数据为 `profile_switching_delivery.json`。
