# CopyCore 真机测试台

不依赖宿主（CopyToolWeb），直接 `LoadLibrary` 加载 `CopyCore.dll`，对复制内核做性能基准与正确性回归。
工具链：MinGW-w64 g++（开发机路径 `C:\mingw64\bin`）。

## 构建

```bat
cd CopyCore\tests
build.bat
```

生成 `bench.exe`、`harness.exe`、`limit_small.exe`（`.exe` 已被仓库 `.gitignore` 忽略，不入库）。
内核 DLL 先按 `CopyCore\build.bat` 构建，默认产物路径为 `..\x64\Release\CopyCore.dll`。

## 工具

### 1. bench.exe — 真机性能基准

```
bench.exe <dll绝对路径> <src目录> <dst根目录(须已存在)> [verify 0|1]
```

- 复制任意真实目录，每 100ms 轮询 `CopyCore_GetStatus`，结束打印耗时 / 文件数 / 失败数 / 平均速度。
- 退出码：0 成功（0 失败），1 有失败，2 调用错误（DLL 加载失败 / 目标根不存在 / Start 返回 FALSE）。

例：

```bat
bench.exe D:\...\CopyCore\x64\Release\CopyCore.dll D:\Steam++ D:\bench_dst 0
```

基准纪律（否则数字不可信）：
1. 同一轮对比要在**全新目标区域交替**跑（R / S / R / S），避免 TRIM 与删除扰动；
2. 冷盘与热缓存数字差可达 2 倍以上（源文件已在系统缓存时偏快），至少跑 3 轮看分布；
3. 不要跨正在跑的其他测试并行跑；
4. 速率单位是 MB/s，小文件负载平均速度没有意义，只看总耗时。

### 2. harness.exe — 正确性回归

```
harness.exe <dll绝对路径> <工作目录(自动创建)> <用例>
```

| 用例 | 覆盖 |
|---|---|
| empty | 空目录 / 空文件树（扫描结束竞态、worker 挂死） |
| mix | 3000 小文件 + 3 个 120MB 大文件 + 空目录，全量 FNV 哈希校验 |
| mix1/mix2/mix3 | 同 mix，分别强制策略 1/2/3 |
| limit | 2 个 300MB 大文件，限速 30MB/s，断言 ~20s（约 30MB/s） |
| skip | 对 mix 结果再跑一次，断点跳过 |
| stop | 复制中终止，200ms 级响应 |
| pause | 暂停 2 秒内字节增量 < 4MB |
| edge / edge3 / edge5 | 边界填充文件，自动 / 强制 HDD-HDD / 强制 SSD-HDD 模式，哈希校验 |

全部用例期望输出 `[xxx] PASS`，退出码 0。

### 3. limit_small.exe — 小文件限速验证

```
limit_small.exe <dll绝对路径> <工作目录> <limitKB>
```

生成 480 个 256KB 文件（共 120MB），限速 30MB/s（参数 30720）时断言 3.0~6.0s、0 失败。
用于覆盖大文件回调限速之外的"小文件复制前预占令牌"路径。

## 已验证的基线（2026-10-18，开发机实测，仅作参考）

- `D:\Steam++`（985 文件 / 338MB）：同盘 D→D 冷盘约 3s，热缓存 1.2~2.5s；legacy 与新版同条件打平。
- 同盘 32 线程比 16 线程慢约 30%；小文件负载 `COPY_FILE_NO_BUFFERING` 慢约 3 倍——勿再开。
- 限速：600MB 大文件 30MB/s 实测 19.1s / 31.4MB/s；120MB 小文件 3.28s / 36.6MB/s。
