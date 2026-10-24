# RTX 4090：BAR1 上限 64 GiB

2026-10-24，八张 48 GiB RTX 4090（`10de:2684`）。

当前实测：八卡最小版本已启动，八次解锁、FLR 前后 capability 检查、
两组桥窗口分配均成功。八张 Linux BAR1 均为 68719476736 字节，全部绑定 NVIDIA。
NVIDIA 八卡均报告 BAR1 total 为 65536 MiB；
TP8 vLLM 的两组 bench 和内外部聊天 API 均已通过；
默认项普通重启后，八卡 64 GiB、驱动、容器自动恢复以及内外部聊天均已再次通过。

## 最小改动

在原有 AD102 SEC2 payload 的四个写入槽中使用以下参数：

| 顺序 | BAR0 偏移 | 写入值 | 用途 |
| --- | --- | --- | --- |
| 1 | `0x88724` | `0x30` | CYA，64 GiB 构造 |
| 2 | `0x88dcc` | `0x8000000a` | CFG，64 GiB 构造 |
| 3 | `0x88ff4` | `0xffffffff` | XVE PLM |
| 4 | `0x823804` | `0xffffffff` | FEAT PLM，保留 tensor 解锁 |

这些是该 AD102 机器的实测构造，不是从其他架构推定的通用寄存器定义。

只执行一次 Booter；原有栈布局、cleanup、SS0/SS1、FLR 和 DMA 清理仍然使用。
新 marker 为 `0x67b00004`。正式代码没有全权限调试、八构造选择器、重复 Booter
或主机侧 CYA/CFG 重试写入。执行前检查 CYA/CFG 的保留位，未知布局直接拒绝。
执行后检查四个目标读回、marker、SEC2 HALT，并在 FLR 前后检查标准 ReBAR
capability 与 MMIO capability 一致且包含 64 GiB。

`gpu_unlock.bar1_64g` 默认为开启，只作用于选中的 `10de:2684`。
模块仍须通过 `gpu_unlock.targets=all` 或明确 PCI 地址选择设备。
`gpu_unlock.bar1_64g=0` 使用原有 tensor-only payload；GA10x 和 AD103 不使用新变体。
原有 GA102、AD102 payload 的内容及 SHA-256 均不变。

生成的新 payload SHA-256：

```text
cf3566d0887d3d31e8cb3611bf0a1b3fafcd9584ea7a73306f75f8d0eafb1284
```

## 为什么只开 PLM 不够

170HX 的参考补丁是 `cmpunlocker/driver/patches/bar1-resize-unlock.patch`：
主机写 CYA/CFG，再调用 NVIDIA 的资源扩容函数。对应源码在相邻研究仓库
`../4090-48g-rebar/sources/`，cmpunlocker 基线为
`2d10f891a58824f0591c3332c4eccf37ff36b086`。

在这台 4090 上，打开一个 XVE PLM、三个 XVE PLM，乃至八组不同权限候选，
主机 MMIO 和 PCI config CYA/CFG 写入仍然不被接受。SEC2 L3 **直接写 CYA/CFG**
的构造有效。以下结果都在未做 PCIe 链路 retrain 的情况下出现：

```text
CYA          0x28       -> 0x30
CFG          0x80000009 -> 0x8000000a
raw cap      0x000ffc00 -> 0x001ffc00
possible     0x0000ffc0 -> 0x0001ffc0
```

因此，修改 capability 不以链路重协商为前提；capability、当前选择的 BAR 大小、
Linux 分配的地址资源和驱动可用性应分别核对。

八卡单次比较全部通过 marker/HALT。单卡直接写入构造随后通过外部 FLR：
CYA/CFG 和 64 GiB 上限保持；WPR2 HI 清零，GSP halted。
FLR 将当前 size control 恢复为 Linux 当时缓存的 32 GiB，这不代表上限丢失。
早期重复 Booter 方案在第二次调用超时，未纳入正式实现。

## 已验证拓扑的桥窗口

仅执行 `echo 16 > resource1_resize` 会返回 `ENOSPC`。每个 PCI root 有 1 TiB
预取地址空间，但原有四卡和空端口窗口占用、嵌套桥对齐使单卡扩容失败。
尝试直接交给内核整组自动重排时，其桥窗口大小也不足以容纳实际对齐后的子窗口。

独立的 [资源布局工具](../tools/)
只用于这台机器，不打包进通用解锁模块。它不修改 NVIDIA 驱动：

- 每个有 GPU 的下游端口分配 128 GiB 窗口，容纳 64 GiB 对齐的 BAR1 和 BAR3。
- 每组 root/upstream 窗口为 576 GiB，给中间空端口及后续对齐留下空间。
- 使用 Linux `pci_release_resource()` / `pci_assign_resource()` 更新资源树，
  同步标准 ReBAR control 和桥预取窗口。BAR0、ROM、音频 BAR、非预取窗口和链路配置不动。
- 仅接受指定的两个 root、Microchip `11f8:4000` 桥、四张未绑定且 BME=0 的
  `10de:2684` GPU，以及它们的 `10de:22ba` 音频函数；要求每卡已支持 64 GiB。

该布局先通过单卡 64 GiB 分配，再通过八卡启动验证。
它针对已验证的四卡交换机拓扑；部署时需按本机拓扑配置资源布局工具。

## 构建与启动

环境：Ubuntu，内核 `7.0.0-38-generic`，NVIDIA open P2P `615.71.09`。
源码基线：`491925c1079435e260137b9f3526066e764de830`。
完整 pinned assets/source 测试通过：23 个 unittest 方法，3 个可选解码测试因缺少
PyCryptodome 跳过；mock 覆盖直接写入失败、marker/HALT、capability 不一致、
FLR 后丢失状态、保留位拒绝和清理边界。两个模块均已在目标内核编译。

```sh
make firmware
make -C kernel/gpu_unlock
layout_dir=$(dirname "$(rg --files tools | rg '/bar1_layout\.c$')")
make -C "$layout_dir"
```

按部署机拓扑配置工具及 `ROOT_BUS_A`、`ROOT_BUS_B` 后，
资源布局模块可在解锁完成、四卡无驱动且 BME=0 时串行加载：

```sh
sudo insmod "$layout_dir/bar1_layout.ko" root_bus="$ROOT_BUS_A"
sudo rmmod bar1_layout
sudo insmod "$layout_dir/bar1_layout.ko" root_bus="$ROOT_BUS_B"
sudo rmmod bar1_layout
```

实际部署使用 init-top 脚本，在 udev 前：
先为八卡设置 driver override，运行解锁，要求八次成功，再串行分配两组资源。
只有八张 Linux BAR1 都恰为 64 GiB 时才清除 override，允许 NVIDIA 绑定。
失败时保留未绑定状态，系统继续启动，便于在线调试。

部署时将两个模块和固定版本固件加入自定义 initrd，并使用独立启动项。
将该启动项配置为 GRUB 默认后，执行 `update-grub`，核对生成配置中的默认 ID。
内核和自定义 initrd 使用已验证的版本；更新内核时应重新构建和验证镜像。
旧实验启动项已清理。回退时选择原系统启动项；需要恢复设备初始状态时完全关机再开机。

## 八卡启动与驱动结果

首次八卡启动：

| GPU | Linux BAR1 | NVIDIA BAR1 total | 驱动 |
| --- | ---: | ---: | --- |
| 0 | 64 GiB | 65536 MiB | nvidia |
| 1 | 64 GiB | 65536 MiB | nvidia |
| 2 | 64 GiB | 65536 MiB | nvidia |
| 3 | 64 GiB | 65536 MiB | nvidia |
| 4 | 64 GiB | 65536 MiB | nvidia |
| 5 | 64 GiB | 65536 MiB | nvidia |
| 6 | 64 GiB | 65536 MiB | nvidia |
| 7 | 64 GiB | 65536 MiB | nvidia |

八张卡的 possible mask 均为 `0x1ffc0`，driver override 均已清除。
NVIDIA 已完成初始化，没有出现 BAR1 回退到 32 GiB；驱动版本仍为原来的 `615.71.09`。
加载 DeepSeek 权重时，八卡 `BAR1 Used` 为 38689–38729 MiB（约 37.8 GiB），
已经超过原 32768 MiB 的上限；这提供了驱动实际分配和使用扩大 aperture 的证据。
bench 完成后，八卡 `BAR1 Used` 进一步达到 47941–47981 MiB（约 46.8 GiB）。
原始内核日志和两份 NVIDIA XML 已归档到本地 `build/bar1-64g/`。

## vLLM 验收方法

保持现有 Compose 配置、镜像和模型不变，使用 TP8、expert parallel、FP8 KV、
Engram CPU offload 和现有 CUDA graph 设置。在项目目录执行 `docker compose up -d`。
先检查 `/v1/models` 和真实 `/v1/chat/completions`，再用容器自带的 CLI：

```sh
export OPENAI_API_KEY="$VLLM_API_KEY"
vllm bench serve --backend openai --base-url http://127.0.0.1:8000 \
  --endpoint /v1/completions --model /model \
  --served-model-name deepseek-v4.1-flash --tokenizer /model \
  --tokenizer-mode deepseek_v41 --trust-remote-code --dataset-name random \
  --num-prompts 32 --max-concurrency 8 --request-rate inf --seed 0 \
  --random-range-ratio 0.0 --random-input-len 512 --random-output-len 128 \
  --ignore-eos --num-warmups 1 --save-result --result-dir /tmp/bar1-64g-bench
```

第二组将输入/输出长度改为 `4096/256`。要求每组完成 32 个请求，无失败，
并检查八卡容量保持及内核没有新增 Xid/PCIe 错误。结果 JSON 和原始日志保存到本地实验目录。

## 应用验收结果

两组均为客户端最大并发 8、32 个请求，输入/输出长度固定，采样沿用服务器默认。
以下吞吐是本次功能验收的结果，没有 32 GiB 对照组。

| 输入/输出 token | 完成 | 失败 | 耗时 s | 输出 token/s | 平均 TTFT ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 512/128 | 32/32 | 0 | 23.07 | 177.56 | 744.78 |
| 4096/256 | 32/32 | 0 | 86.45 | 94.76 | 4524.21 |

本机 `/v1/models`、`/v1/chat/completions` 及既有 HTTPS 外部入口均返回 200；
真实聊天返回“一加一等于二。”及“BAR1 测试通过。”。
容器状态 healthy，重启次数 0；检查内核日志未发现 Xid、fallen-off-bus 或 PCIe 错误。
Compose 镜像和运行参数保持原配置，Docker 为 enabled，容器使用 unless-stopped 重启策略。

八卡 FLR 后的 Tensor Core FP32 累加选择器也全部读回
`SS0=0x88888888`、`SS1=0x8`。本次没有单独重测 tensor 指令吞吐。

## 默认项普通重启

从运行中的 NVIDIA 和 vLLM 服务发起普通 `systemctl reboot --no-block`，
未设置一次性启动项，确认实际进入了配置的默认启动项。

本次再次通过八次解锁、FLR 后 64 GiB capability 和 FP32 累加选择器读回、
两组窗口分配，八张 Linux BAR1 均为 68719476736 字节，NVIDIA 均为 65536 MiB。
GRUB 默认项正确，三个旧实验启动项已不存在。
Docker 自动恢复原 DeepSeek 容器，restart policy 为 unless-stopped，
无需再次执行 Compose。模型就绪后，本机及外部 HTTPS 的
`/v1/models`、`/v1/chat/completions` 均返回 200，聊天返回“一加一等于二。”。
容器 healthy、重启次数为 0，八卡 BAR1 total 保持 65536 MiB，
BAR1 used 为 47861–47901 MiB；内核日志无 Xid 或 PCIe 错误。
本次完成了默认启动、自动恢复及真实推理的整体验收。

本机的专用 initrd 强制要求八卡 64 GiB；如需回退，应选择原 Ubuntu 项，
而不是在这个专用镜像中单独设置 `bar1_64g=0`。

## 实验归档

本地实验目录保留完整代码快照、补丁、payload、模块、原始日志及验收结果。
原始实验记录不随源码发布，审计工具记录命令、输入、输出和退出码。
公开报告只保留硬件型号、软件版本、寄存器构造及功能验证结果。
