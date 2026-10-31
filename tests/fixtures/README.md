# 图传测试素材

`video.hevc` 是由 FFmpeg `testsrc2` 生成的合成测试图，96×64、10 fps、6 帧。使用 Annex-B HEVC，带 AUD 和重复参数集，每 3 帧一个关键帧、无 B 帧，用于验证真实解码、时间来源和丢帧恢复。无外部视频版权内容。

生成命令（仅维护素材时需要 FFmpeg 与 libx265 编码器）：

```bash
ffmpeg -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=96x64:rate=10' \
  -frames:v 6 -c:v libx265 -pix_fmt yuv420p -preset ultrafast \
  -x265-params 'pools=none:frame-threads=1:keyint=3:min-keyint=3:scenecut=0:bframes=0:repeat-headers=1:aud=1:log-level=error' \
  -f hevc tests/fixtures/video.hevc
```

该小尺寸素材不用于证明现场 1080p60 的性能。
