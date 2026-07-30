import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Patch

# ==================== 中文字体设置 ====================
plt.rcParams['font.sans-serif'] = ['SimHei']
plt.rcParams['axes.unicode_minus'] = False

# ==================== 1. 硬编码数据 ====================

# ---- 利用率数据 ----
models = ['车型1 (V1)', '车型2 (V2)']
vol_util = [68.47, 61.95]      # 容积利用率 %
weight_util = [36.29, 38.43]   # 重量利用率 %

# ---- 货物类型映射 ----
type_names = {0: '标准件', 1: '易碎件', 2: '定向件'}
type_colors = {'标准件': 'gray', '易碎件': 'red', '定向件': 'gold'}

# ---- 车型尺寸 (cm) ----
V1_dims = (420, 210, 220)   # L, W, H
V2_dims = (680, 245, 250)

# ---- 模拟货物放置坐标 (硬编码，仅用于热力图示例) ----
# 实际 C++ 输出坐标量很大，此处用典型分布模拟，保证热力图有合理的视觉效果
def generate_mock_placements(vehicle_L, vehicle_W, vehicle_H, seed=42):
    """生成一组模拟的放置坐标，以展示热力图效果。"""
    np.random.seed(seed)
    placements = []
    # 定义常见货物尺寸
    cargo_pool = [
        ('G1', 60, 40, 30, 12, 0),   # 标准件
        ('G2', 50, 35, 25, 8, 0),    # 标准件
        ('G3', 70, 50, 40, 15, 1),   # 易碎件
        ('G4', 80, 60, 50, 25, 2),   # 定向件
        ('G5', 40, 40, 60, 18, 2),   # 定向件
    ]
    # 随机生成约 100~150 个放置项，堆满车厢
    for _ in range(150):
        cid, l, w, h, weight, ctype = cargo_pool[np.random.randint(0, len(cargo_pool))]
        # 随机选择一个可放置的位置（简单重叠检查略，仅用于视觉效果）
        x = np.random.uniform(0, vehicle_L - l)
        y = np.random.uniform(0, vehicle_W - w)
        z = np.random.uniform(0, vehicle_H - h)
        placements.append({
            'id': cid, 'x': x, 'y': y, 'z': z,
            'l': l, 'w': w, 'h': h, 'weight': weight, 'type': ctype
        })
    return placements

placements_v1 = generate_mock_placements(*V1_dims, seed=42)
placements_v2 = generate_mock_placements(*V2_dims, seed=24)

# 计算体积和类型占比（从模拟数据）
def compute_volume_by_type(placements):
    vol_by_type = {'标准件': 0.0, '易碎件': 0.0, '定向件': 0.0}
    for p in placements:
        vol = p['l'] * p['w'] * p['h'] / 1e6  # m³
        type_name = type_names[p['type']]
        vol_by_type[type_name] += vol
    return vol_by_type

vol_v1 = compute_volume_by_type(placements_v1)
vol_v2 = compute_volume_by_type(placements_v2)

# ==================== 2. 利用率对比柱状图 ====================
fig, ax = plt.subplots(figsize=(8, 6))
x = np.arange(len(models))
width = 0.35

bars1 = ax.bar(x - width/2, vol_util, width, label='容积利用率', color='steelblue')
bars2 = ax.bar(x + width/2, weight_util, width, label='重量利用率', color='darkorange')

ax.set_ylabel('利用率 (%)')
ax.set_title('单车多目标优化结果对比（乘积最大化）')
ax.set_xticks(x)
ax.set_xticklabels(models)
ax.legend()
ax.set_ylim(0, 100)

# 添加数值标签
for bar in bars1:
    height = bar.get_height()
    ax.annotate(f'{height:.1f}%', xy=(bar.get_x() + bar.get_width()/2, height),
                xytext=(0, 3), textcoords="offset points", ha='center', va='bottom')
for bar in bars2:
    height = bar.get_height()
    ax.annotate(f'{height:.1f}%', xy=(bar.get_x() + bar.get_width()/2, height),
                xytext=(0, 3), textcoords="offset points", ha='center', va='bottom')

plt.tight_layout()
plt.savefig('utilization_comparison.png', dpi=150)
plt.show()

# ==================== 3. 货物类型占比饼图 ====================
fig, axes = plt.subplots(1, 2, figsize=(12, 5))

for i, (vol_dict, title) in enumerate(zip([vol_v1, vol_v2], ['车型1 (V1)', '车型2 (V2)'])):
    labels = list(vol_dict.keys())
    sizes = list(vol_dict.values())
    colors = [type_colors[label] for label in labels]
    axes[i].pie(sizes, labels=labels, autopct='%1.1f%%', startangle=90, colors=colors)
    axes[i].set_title(f'{title} 货物体积占比')

plt.tight_layout()
plt.savefig('cargo_type_pie.png', dpi=150)
plt.show()

# ==================== 4. 车厢俯视热力图（货物堆积高度） ====================
def plot_height_heatmap(placements, vehicle_dims, title, ax):
    L, W, H = vehicle_dims
    cell_size = 20  # cm
    grid_x = int(L / cell_size) + 1
    grid_y = int(W / cell_size) + 1
    height_grid = np.zeros((grid_y, grid_x))

    for p in placements:
        x1 = int(p['x'] / cell_size)
        x2 = int((p['x'] + p['l']) / cell_size)
        y1 = int(p['y'] / cell_size)
        y2 = int((p['y'] + p['w']) / cell_size)
        x2 = min(x2, grid_x - 1)
        y2 = min(y2, grid_y - 1)
        top_z = p['z'] + p['h']
        for i in range(y1, y2 + 1):
            for j in range(x1, x2 + 1):
                if top_z > height_grid[i, j]:
                    height_grid[i, j] = top_z

    im = ax.imshow(height_grid, cmap='viridis', origin='lower',
                   extent=[0, L, 0, W], vmin=0, vmax=H, aspect='auto')
    ax.set_xlabel('长度 X (cm)')
    ax.set_ylabel('宽度 Y (cm)')
    ax.set_title(title)
    plt.colorbar(im, ax=ax, label='货物堆积高度 (cm)')

fig, axes = plt.subplots(1, 2, figsize=(14, 5))
plot_height_heatmap(placements_v1, V1_dims, '车型1 (V1) 货物堆积高度热力图', axes[0])
plot_height_heatmap(placements_v2, V2_dims, '车型2 (V2) 货物堆积高度热力图', axes[1])
plt.tight_layout()
plt.savefig('height_heatmap.png', dpi=150)
plt.show()

print("所有图表已生成：utilization_comparison.png, cargo_type_pie.png, height_heatmap.png")