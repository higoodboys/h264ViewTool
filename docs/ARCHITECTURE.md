# 架构与核心算法

## 1. 模块分层

```
┌─────────────────────────────────────────────────────────┐
│  UI 层 (Qt)                                              │
│  MainControlWindow  PreviewWindow  FrameBarChart         │
│  FrameInfoPanel     StatsPanel     NavigationBar         │
│              SyncController（帧选中/播放同步）              │
└───────────────────────────┬─────────────────────────────┘
                            │ FrameInfo / 选中索引
┌───────────────────────────▼─────────────────────────────┐
│  分析层                                                   │
│  FileLoader → (Raw / Mp4 / Avi) → NAL 列表               │
│  StreamAnalyzer → FrameInfo[] + StreamStats              │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────┐
│  解析层                                                   │
│  NalParser  SpsPpsParser  SliceHeaderParser  BitReader   │
└───────────────────────────┬─────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────┐
│  解码层 (可选)                                            │
│  IDecoder ← FFmpegDecoder / NullDecoder                  │
│  setStreamIndex：按已解析 NAL/关键帧从最近 I 帧解码预览     │
└─────────────────────────────────────────────────────────┘
```

## 2. 核心类职责

| 类 | 职责 |
|----|------|
| `BitReader` | 按位读取；实现 `ue(v)` / `se(v)` Exp-Golomb |
| `NalUnit` | 单个 NAL：类型、偏移、载荷、是否起始码 |
| `NalParser` | Annex-B 起始码扫描；AVCC 长度前缀分割 |
| `SpsPpsParser` | 解析 SPS/PPS → 宽高、POC 类型、参考帧数等 |
| `SliceHeaderParser` | 解析 slice_type / frame_num / POC / QP |
| `FrameInfo` | 一帧聚合信息（可含多 NAL） |
| `StreamAnalyzer` | 遍历 NAL，组装帧列表与全局统计 |
| `RawH264Reader` | 读裸流并分割 NAL |
| `Mp4Demuxer` | 解析 ftyp/moov/trak/stsd(avcC)；progressive 走 stsz/stco，fMP4 走 moof/trun，输出 AVCC NAL |
| `AviDemuxer` | 解析 RIFF/hdrl/movi，提取 H264 块 |
| `FileLoader` | 按扩展名/魔数选择读取器 |
| `IDecoder` | 解码接口：seek/decodeFrame → QImage |
| `SyncController` | 控制窗与预览窗共享当前帧索引 |

## 3. NAL 解析流程

```
文件字节流
    │
    ├─ 裸流 / Annex-B：扫描 00 00 01 或 00 00 00 01
    │       → 切分 NAL，去掉起始码，读 nal_unit_type (低 5 bit)
    │
    └─ MP4 AVCC：avcC 提供 lengthSizeMinusOne（通常 3 → 4 字节长度）
            → 读 length，再读 length 字节为 NAL
```

关键：`NalParser::parseAnnexB` / `parseAvcc`。

## 4. 帧类型判定

依据 H.264 (ISO/IEC 14496-10)：

1. `nal_unit_type == 5` → **IDR**（计为 I 帧）
2. `nal_unit_type == 1`（非 IDR 编码片）→ 解析 `slice_header`：
   - `slice_type % 5`：0=P，1=B，2=I，3=SP，4=SI
3. 同一接入单元（AU）内多个 slice 合并为一帧；以主片类型为准。
4. Access Unit 分界启发式：新的主键片 + `first_mb_in_slice==0`，或遇到 AUD，或 SPS/PPS 后新片。

颜色映射（条带图）：

- I / IDR → `#E53935` 红
- P → `#1E88E5` 蓝
- B → `#43A047` 绿
- 其他 / 未知 → 灰

## 5. POC 与 QP

- `pic_order_cnt_type` 来自 SPS；type 0 时从 slice header 读 `pic_order_cnt_lsb`。
- `QP = 26 + pic_init_qp_minus26(PPS) + slice_qp_delta`。

## 6. 图形绘制逻辑（FrameBarChart）

1. 分析完成后持有 `QVector<FrameInfo>`。
2. `paintEvent`：按可见窗口计算 `firstVisible`～`lastVisible`。
3. 每帧画矩形：`x = (index - first) * barWidth`，`h = size / maxSize * plotHeight`。
4. 点击：`index = first + x / barWidth`，发射 `frameClicked(index)`。
5. 目标：可见区重绘 &lt; 50ms（仅绘制可见条，避免全量路径）。

## 7. 双窗口同步

```
用户点击条带 / 导航按钮 / 预览快捷键
        → SyncController::setCurrentFrame(i)
        → 控制窗：高亮条带 + 刷新 FrameInfoPanel
        → 预览窗：IDecoder::decodeFrame(i) → 显示图像
```

## 8. 扩展点

- 新增容器：实现读取器并在 `FileLoader` 注册。
- 新增解码器：实现 `IDecoder`，在 `DecoderFactory` 选择。
- SEI / 宏块级分析：在 `StreamAnalyzer` 后挂接二级解析器。
