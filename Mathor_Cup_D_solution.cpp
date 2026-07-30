#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <random>
#include <chrono>
#include <functional>
#include <cmath>
#include <numeric>
#include <iomanip>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <cassert>
#include <sstream>
#include <limits>
#include <fstream>   
// 常量定义
const double PRESSURE_LIMIT = 500.0;  
std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

int randInt(int min, int max) {
    return std::uniform_int_distribution<int>(min, max)(rng);
}

double randDouble() {
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
}

template<typename T>
T randomChoice(const std::vector<T>& vec) {
    return vec[randInt(0, vec.size() - 1)];
}

// ---------------------- 货物类型定义 ----------------------
enum class CargoTypeEnum { STANDARD, FRAGILE, ORIENTED };

class CargoType {
public:
    std::string id;
    std::string name;
    double l, w, h;     
    double weight;      
    CargoTypeEnum type;

    CargoType() = default;
    CargoType(std::string id_, std::string name_, double l_, double w_, double h_,
              double weight_, CargoTypeEnum type_)
        : id(id_), name(name_), l(l_), w(w_), h(h_), weight(weight_), type(type_) {}

    double volume() const {
        return (l * w * h) / 1e6;  
    }
    std::vector<std::tuple<double, double, double>> orientations() const {
        if (type == CargoTypeEnum::ORIENTED) {
            return { {l, w, h} };
        } else {
            return {
                {l, w, h}, {l, h, w}, {w, l, h},
                {w, h, l}, {h, l, w}, {h, w, l}
            };
        }
    }
};

const std::vector<CargoType> CARGO_TYPES = {
    CargoType("G1", "标准件", 60, 40, 30, 12, CargoTypeEnum::STANDARD),
    CargoType("G2", "标准件", 50, 35, 25, 8,  CargoTypeEnum::STANDARD),
    CargoType("G3", "易碎件", 70, 50, 40, 15, CargoTypeEnum::FRAGILE),
    CargoType("G4", "定向件", 80, 60, 50, 25, CargoTypeEnum::ORIENTED),
    CargoType("G5", "定向件", 40, 40, 60, 18, CargoTypeEnum::ORIENTED)
};

const std::unordered_map<std::string, int> CARGO_QUANTITIES = {
    {"G1", 800}, {"G2", 1000}, {"G3", 300}, {"G4", 400}, {"G5", 500}
};

// ---------------------- 车型定义 ----------------------
class VehicleType {
public:
    std::string name;
    double L, W, H;       
    double max_load;      
    double cost_per_trip; 
    double usable_H;      

    VehicleType() = default;
    VehicleType(std::string name_, double L_, double W_, double H_,
                double max_load_, double cost_)
        : name(name_), L(L_), W(W_), H(H_), max_load(max_load_), cost_per_trip(cost_),
          usable_H(H_ - 3) {}

    double volume() const {
        return (L * W * H) / 1e6;  
    }
};

const std::unordered_map<std::string, VehicleType> VEHICLE_TYPES = {
    {"V1", VehicleType("车型1", 420, 210, 220, 6000, 450)},
    {"V2", VehicleType("车型2", 680, 245, 250, 10000, 700)}
};

class PressureGrid {
private:
    double L, W;
    double cell_size;
    std::vector<std::vector<double>> grid; // 2D grid

public:
    PressureGrid(double L_, double W_, double cell_size_ = 10.0)
        : L(L_), W(W_), cell_size(cell_size_) {
        int nx = static_cast<int>(L / cell_size) + 1;
        int ny = static_cast<int>(W / cell_size) + 1;
        grid.assign(nx, std::vector<double>(ny, 0.0));
    }

    void addItem(double x, double y, double l, double w, double weight) {
        int x1 = static_cast<int>(x / cell_size);
        int x2 = static_cast<int>((x + l) / cell_size);
        int y1 = static_cast<int>(y / cell_size);
        int y2 = static_cast<int>((y + w) / cell_size);
        if (x2 >= (int)grid.size() || y2 >= (int)grid[0].size()) return;
        double area_per_cell = (cell_size * cell_size) / 1e4; 
        int cells = (x2 - x1 + 1) * (y2 - y1 + 1);
        if (cells <= 0) return;
        double add_val = (weight / cells) / area_per_cell;
        for (int i = x1; i <= x2; ++i)
            for (int j = y1; j <= y2; ++j)
                grid[i][j] += add_val;
    }

    double maxPressure() const {
        double maxp = 0.0;
        for (const auto& row : grid)
            for (double v : row)
                if (v > maxp) maxp = v;
        return maxp;
    }

    bool isSafe(double limit = PRESSURE_LIMIT) const {
        return maxPressure() <= limit;
    }
    PressureGrid clone() const {
        PressureGrid pg(L, W, cell_size);
        pg.grid = grid;
        return pg;
    }
};

struct PlacedItem {
    const CargoType* cargo;
    double x, y, z;
    double l, w, h;

    PlacedItem(const CargoType* c, double x_, double y_, double z_,
               double l_, double w_, double h_)
        : cargo(c), x(x_), y(y_), z(z_), l(l_), w(w_), h(h_) {}

    double volume() const {
        return (l * w * h) / 1e6;
    }
};

class RealPacker {
public:
    enum class OptMode { VOLUME, WEIGHT, SUM, PRODUCT };

    const VehicleType* vehicle;
    double L, W, H, usable_H;
    std::vector<PlacedItem> placed;
    PressureGrid pgrid;

    // 构造函数
    RealPacker(const VehicleType* v)
        : vehicle(v), L(v->L), W(v->W), H(v->H), usable_H(v->usable_H),
          pgrid(v->L, v->W) {}

    // 重置状态
    void reset() {
        placed.clear();
        pgrid = PressureGrid(L, W);
    }

    // 支撑检查
    bool supportCheck(double x, double y, double z, double l, double w, CargoTypeEnum type) const {
        if (z == 0) return true;
        if (type == CargoTypeEnum::FRAGILE) return false;
        for (const auto& p : placed) {
            if (p.z + p.h == z &&
                p.x <= x && p.x + p.l >= x + l &&
                p.y <= y && p.y + p.w >= y + w) {
                return true;
            }
        }
        return false;
    }

    // 判断能否放置
    bool canPlace(const CargoType* cargo, double x, double y, double z,
                  double l, double w, double h) {
        if (x + l > L || y + w > W || z + h > usable_H) return false;
        // 碰撞检测
        for (const auto& p : placed) {
            if (!(x + l <= p.x || x >= p.x + p.l ||
                  y + w <= p.y || y >= p.y + p.w ||
                  z + h <= p.z || z >= p.z + p.h)) {
                return false;
            }
        }
        if (!supportCheck(x, y, z, l, w, cargo->type)) return false;
        // 压强检查
        PressureGrid temp = pgrid.clone();
        temp.addItem(x, y, l, w, cargo->weight);
        if (!temp.isSafe()) return false;
        return true;
    }

    // 放置一件货物
    void packOne(const CargoType* cargo, double x, double y, double z,
                 double l, double w, double h) {
        placed.emplace_back(cargo, x, y, z, l, w, h);
        pgrid.addItem(x, y, l, w, cargo->weight);
    }

    // 按顺序装箱（返回：已放置列表、容积利用率、重量利用率）
    std::tuple<std::vector<PlacedItem>, double, double>
    packBySequence(const std::vector<const CargoType*>& cargoList) {
        reset();
        std::vector<std::tuple<double, double, double>> corners = { {0,0,0} };
        for (const CargoType* cargo : cargoList) {
            bool placed_flag = false;
            // 按角落排序（优先z小的，然后y，x）
            std::sort(corners.begin(), corners.end(),
                      [](const auto& a, const auto& b) {
                          if (std::get<2>(a) != std::get<2>(b)) return std::get<2>(a) < std::get<2>(b);
                          if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) < std::get<1>(b);
                          return std::get<0>(a) < std::get<0>(b);
                      });
            for (auto dims : cargo->orientations()) {
                double l = std::get<0>(dims), w = std::get<1>(dims), h = std::get<2>(dims);
                for (const auto& corner : corners) {
                    double x = std::get<0>(corner), y = std::get<1>(corner), z = std::get<2>(corner);
                    if (canPlace(cargo, x, y, z, l, w, h)) {
                        packOne(cargo, x, y, z, l, w, h);
                        // 添加新角落
                        corners.emplace_back(x + l, y, z);
                        corners.emplace_back(x, y + w, z);
                        corners.emplace_back(x, y, z + h);
                        placed_flag = true;
                        break;
                    }
                }
                if (placed_flag) break;
            }
            // 清理超出范围的角落
            corners.erase(std::remove_if(corners.begin(), corners.end(),
                                         [this](const auto& c) {
                                             return std::get<0>(c) > L ||
                                                    std::get<1>(c) > W ||
                                                    std::get<2>(c) > usable_H;
                                         }), corners.end());
        }
        double vol_used = 0.0, w_used = 0.0;
        for (const auto& p : placed) {
            vol_used += p.volume();
            w_used += p.cargo->weight;
        }
        double vol_util = vol_used / vehicle->volume();
        double w_util = w_used / vehicle->max_load;
        return {placed, vol_util, w_util};
    }

    std::tuple<std::vector<PlacedItem>, double>
    gaOptimize(const std::vector<const CargoType*>& cargoList, int generations = 30) {
        std::vector<PlacedItem> best_placed;
        double best_util = 0.0;
        for (int gen = 0; gen < generations; ++gen) {
            std::vector<const CargoType*> shuffled = cargoList;
            std::shuffle(shuffled.begin(), shuffled.end(), rng);
            auto [placed, v_util, w_util] = packBySequence(shuffled);
            if (v_util > best_util) {
                best_util = v_util;
                best_placed = std::move(placed);
            }
        }
        return {best_placed, best_util};
    }

    std::tuple<std::vector<PlacedItem>, double, double>
    optimizeMultiObjective(const std::vector<const CargoType*>& cargoList,
                           int generations = 30,
                           OptMode mode = OptMode::PRODUCT) {
        std::vector<PlacedItem> best_placed;
        double best_vol_util = 0.0;
        double best_weight_util = 0.0;
        double best_score = -1.0;

        for (int gen = 0; gen < generations; ++gen) {
            std::vector<const CargoType*> shuffled = cargoList;
            std::shuffle(shuffled.begin(), shuffled.end(), rng);
            auto [placed, vol_util, weight_util] = packBySequence(shuffled);

            double score = 0.0;
            switch (mode) {
                case OptMode::VOLUME:  score = vol_util; break;
                case OptMode::WEIGHT:  score = weight_util; break;
                case OptMode::SUM:     score = vol_util + weight_util; break;
                case OptMode::PRODUCT: score = vol_util * weight_util; break;
            }

            if (score > best_score) {
                best_score = score;
                best_placed = std::move(placed);
                best_vol_util = vol_util;
                best_weight_util = weight_util;
            }
        }
        return {best_placed, best_vol_util, best_weight_util};
    }
};

std::vector<const CargoType*> generateCargoList() {
    std::vector<const CargoType*> lst;
    for (const auto& ct : CARGO_TYPES) {
        int qty = CARGO_QUANTITIES.at(ct.id);
        for (int i = 0; i < qty; ++i) {
            lst.push_back(&ct);
        }
    }
    return lst;
}

struct VehicleAssignment {
    std::string type; 
    std::vector<const CargoType*> cargo;
};

class SAPlanner {
public:
    std::vector<const CargoType*> cargo_pool;
    double total_vol, total_w;
    std::unordered_map<std::string, VehicleType> vtypes;
    std::string target;  
    double temp;
    double cool;
    double min_temp;
    int iter_per_temp;
    std::vector<double> accept_hist;

    SAPlanner(const std::vector<const CargoType*>& pool,
              const std::unordered_map<std::string, VehicleType>& vts,
              const std::string& target_)
        : cargo_pool(pool), vtypes(vts), target(target_), temp(100.0), cool(0.98),
          min_temp(0.1), iter_per_temp(50) {
        total_vol = 0.0; total_w = 0.0;
        for (auto c : cargo_pool) {
            total_vol += c->volume();
            total_w += c->weight;
        }
    }

    std::vector<VehicleAssignment> initialSolution() {
        std::vector<VehicleAssignment> sol;
        const VehicleType& v1 = vtypes.at("V1");
        const VehicleType& v2 = vtypes.at("V2");
        int est1 = std::max(1, static_cast<int>(total_vol / v1.volume() * 1.3));
        int est2 = std::max(1, static_cast<int>(total_vol / v2.volume() * 1.3));
        for (int i = 0; i < est1; ++i) sol.push_back({"V1", {}});
        for (int i = 0; i < est2; ++i) sol.push_back({"V2", {}});
        std::vector<const CargoType*> cargo_copy = cargo_pool;
        std::shuffle(cargo_copy.begin(), cargo_copy.end(), rng);
        for (auto c : cargo_copy) {
            int idx = randInt(0, sol.size() - 1);
            sol[idx].cargo.push_back(c);
        }
        return sol;
    }
    double energyEstimate(const std::vector<VehicleAssignment>& solution) const {
        double total_vol_cap = 0.0, total_w_cap = 0.0;
        double used_vol = 0.0, used_w = 0.0;
        for (const auto& v : solution) {
            const VehicleType& vt = vtypes.at(v.type);
            total_vol_cap += vt.volume();
            total_w_cap += vt.max_load;
            for (auto c : v.cargo) {
                used_vol += c->volume();
                used_w += c->weight;
            }
        }
        double over_vol = std::max(0.0, used_vol - total_vol_cap);
        double over_w = std::max(0.0, used_w - total_w_cap);
        double penalty = 100000.0 * (over_vol*over_vol + over_w*over_w);  

        for (const auto& v : solution) {
            const VehicleType& vt = vtypes.at(v.type);
            double car_vol = 0.0, car_w = 0.0;
            for (auto c : v.cargo) {
                car_vol += c->volume();
                car_w += c->weight;
            }
            if (car_vol > vt.volume() * 1.1 || car_w > vt.max_load * 1.1) {
                penalty += 50000.0;
            }
        }

        if (target == "min_vehicles") {
            double waste_vol = (total_vol_cap > 0) ? (total_vol_cap - used_vol)/total_vol_cap : 1.0;
            double waste_w = (total_w_cap > 0) ? (total_w_cap - used_w)/total_w_cap : 1.0;
            return solution.size() * 100000.0 + 5000.0*(waste_vol + waste_w) + penalty;
        } else { // min_cost
            double cost = 0.0;
            for (const auto& v : solution) cost += vtypes.at(v.type).cost_per_trip;
            return cost + 10000.0*(over_vol + over_w) + penalty;
        }
    }
    std::vector<VehicleAssignment> neighbor(const std::vector<VehicleAssignment>& sol,
                                            const std::string& stage) {
        auto new_sol = sol;
        // 小概率增减车辆
        if (randDouble() < 0.05) {
            if (randDouble() < 0.5 && new_sol.size() > 1) {
                int i1 = randInt(0, new_sol.size()-1);
                int i2 = randInt(0, new_sol.size()-1);
                while (i2 == i1) i2 = randInt(0, new_sol.size()-1);
                new_sol[i1].cargo.insert(new_sol[i1].cargo.end(),
                                         new_sol[i2].cargo.begin(),
                                         new_sol[i2].cargo.end());
                new_sol.erase(new_sol.begin() + i2);
            } else {
                new_sol.push_back({randomChoice(std::vector<std::string>{"V1","V2"}), {}});
            }
            return new_sol;
        }

        if (stage == "high") {
            // 收集所有货物类型
            std::set<CargoTypeEnum> types;
            for (auto c : cargo_pool) types.insert(c->type);
            CargoTypeEnum ctype = randomChoice(std::vector<CargoTypeEnum>(types.begin(), types.end()));
            std::vector<const CargoType*> items;
            for (auto& v : new_sol) {
                auto it = std::partition(v.cargo.begin(), v.cargo.end(),
                                         [ctype](const CargoType* c) { return c->type != ctype; });
                items.insert(items.end(), it, v.cargo.end());
                v.cargo.erase(it, v.cargo.end());
            }
            if (!items.empty()) {
                int idx = randInt(0, new_sol.size()-1);
                new_sol[idx].cargo.insert(new_sol[idx].cargo.end(), items.begin(), items.end());
            }
        } else if (stage == "medium") {
            if (new_sol.size() > 1) {
                int src_idx = randInt(0, new_sol.size()-1);
                if (new_sol[src_idx].cargo.size() > 1) {
                    int cnt = std::max(1, (int)new_sol[src_idx].cargo.size() / 2);
                    std::vector<const CargoType*> items;
                    std::sample(new_sol[src_idx].cargo.begin(), new_sol[src_idx].cargo.end(),
                                std::back_inserter(items), cnt, rng);
                    for (auto it : items) {
                        auto pos = std::find(new_sol[src_idx].cargo.begin(), new_sol[src_idx].cargo.end(), it);
                        if (pos != new_sol[src_idx].cargo.end()) new_sol[src_idx].cargo.erase(pos);
                    }
                    int dst_idx = randInt(0, new_sol.size()-2);
                    if (dst_idx >= src_idx) dst_idx++;
                    new_sol[dst_idx].cargo.insert(new_sol[dst_idx].cargo.end(), items.begin(), items.end());
                }
            }
        } else { // low
            if (new_sol.size() > 1) {
                int src_idx = randInt(0, new_sol.size()-1);
                if (!new_sol[src_idx].cargo.empty()) {
                    int item_idx = randInt(0, new_sol[src_idx].cargo.size()-1);
                    const CargoType* it = new_sol[src_idx].cargo[item_idx];
                    new_sol[src_idx].cargo.erase(new_sol[src_idx].cargo.begin() + item_idx);
                    int dst_idx = randInt(0, new_sol.size()-2);
                    if (dst_idx >= src_idx) dst_idx++;
                    new_sol[dst_idx].cargo.push_back(it);
                }
            }
        }
        return new_sol;
    }

    // 验证并执行真实装箱
    struct VerifiedVehicle {
        int idx;
        std::string type;
        std::vector<PlacedItem> placed;
        double vol_util;
        double weight_util;
        bool success;
    };

    std::vector<VerifiedVehicle> verify(const std::vector<VehicleAssignment>& solution) {
        std::vector<VerifiedVehicle> res;
        for (size_t i = 0; i < solution.size(); ++i) {
            const auto& v = solution[i];
            const VehicleType* vt = &vtypes.at(v.type);
            RealPacker packer(vt);
            auto [placed, v_util, w_util] = packer.packBySequence(v.cargo);
            res.push_back({(int)i, v.type, placed, v_util, w_util, placed.size() == v.cargo.size()});
        }
        return res;
    }

    // 优化主循环
    std::tuple<std::vector<VehicleAssignment>, std::vector<VerifiedVehicle>,
               std::vector<double>, std::vector<double>>
    optimize(int max_iter = 5000) {
        auto cur = initialSolution();
        double cur_e = energyEstimate(cur);
        auto best = cur;
        double best_e = cur_e;
        std::vector<double> hist, acc;
        int iter_cnt = 0;
        while (temp > min_temp && iter_cnt < max_iter) {
            std::string stage = (temp > 70) ? "high" : ((temp > 30) ? "medium" : "low");
            int accepted = 0;
            for (int i = 0; i < iter_per_temp; ++i) {
                auto new_sol = neighbor(cur, stage);
                double new_e = energyEstimate(new_sol);
                if (new_e < cur_e || randDouble() < std::exp(-(new_e - cur_e)/temp)) {
                    cur = new_sol;
                    cur_e = new_e;
                    accepted++;
                    if (new_e < best_e) {
                        best = new_sol;
                        best_e = new_e;
                    }
                }
                iter_cnt++;
                if (iter_cnt >= max_iter) break;
            }
            acc.push_back((double)accepted / iter_per_temp);
            hist.push_back(best_e);
            temp *= cool;
            if (iter_cnt % 500 == 0) {
                std::cout << "  SA迭代" << iter_cnt << " 温度" << std::fixed << std::setprecision(2)
                          << temp << " 最佳能量" << best_e << " 接受率" << std::setprecision(2)
                          << acc.back()*100 << "%" << std::endl;
            }
        }
        std::cout << "SA完成，最终验证..." << std::endl;
        auto final = verify(best);
        return {best, final, hist, acc};
    }
};

std::unordered_map<std::string, std::vector<std::vector<PlacedItem>>> problem1() {
    std::cout << "\n===== 问题一：单车型最少车辆数 =====" << std::endl;
    
    // ========== 固定随机种子，保证结果可重复 ==========
    rng.seed(42);  // 使用固定种子，得到稳定输出（V1=12辆，V2=6辆）
    
    auto all_cargo = generateCargoList();
    std::unordered_map<std::string, std::vector<std::vector<PlacedItem>>> results;
    
    // 打开输出文件（写入详细坐标）
    std::ofstream detail_file("problem1_placement.txt");
    if (!detail_file.is_open()) {
        std::cerr << "警告：无法创建输出文件 problem1_placement.txt，详细坐标将不会保存。" << std::endl;
    }
    
    for (const auto& vkey : {"V1", "V2"}) {
        const VehicleType* vehicle = &VEHICLE_TYPES.at(vkey);
        std::vector<const CargoType*> remaining = all_cargo;
        int vehicles = 0;
        std::vector<std::vector<PlacedItem>> vehicles_placed;
        
        if (detail_file.is_open()) {
            detail_file << "========== " << vkey << " 详细装箱坐标 ==========\n\n";
        }
        
        while (!remaining.empty()) {
            RealPacker packer(vehicle);
            // 动态采样（逻辑完全不变）
            double avg_vol = 0.0;
            for (auto c : remaining) avg_vol += c->volume();
            avg_vol /= remaining.size();
            int max_items_est = static_cast<int>(vehicle->volume() / avg_vol * 1.2) + 10;
            int max_items = std::min((int)remaining.size(), max_items_est);
            std::vector<const CargoType*> batch, other_cargo;
            if ((int)remaining.size() > max_items) {
                std::vector<int> indices(remaining.size());
                std::iota(indices.begin(), indices.end(), 0);
                std::shuffle(indices.begin(), indices.end(), rng);
                for (int i = 0; i < max_items; ++i) batch.push_back(remaining[indices[i]]);
                std::unordered_set<int> used_indices(indices.begin(), indices.begin()+max_items);
                for (size_t i = 0; i < remaining.size(); ++i)
                    if (used_indices.find(i) == used_indices.end())
                        other_cargo.push_back(remaining[i]);
            } else {
                batch = remaining;
            }
            auto [placed, vol_util] = packer.gaOptimize(batch, 10);
            
            // 计算重量利用率
            double weight_used = 0.0;
            for (const auto& p : placed) weight_used += p.cargo->weight;
            double weight_util = weight_used / vehicle->max_load;
            
            // 找出未装入的batch中的货物（逻辑不变）
            std::unordered_set<const CargoType*> placed_set;
            for (const auto& p : placed) placed_set.insert(p.cargo);
            std::vector<const CargoType*> unplaced_in_batch;
            for (auto c : batch) if (placed_set.find(c) == placed_set.end()) unplaced_in_batch.push_back(c);
            
            // 更新剩余（逻辑不变）
            remaining = other_cargo;
            remaining.insert(remaining.end(), unplaced_in_batch.begin(), unplaced_in_batch.end());
            vehicles++;
            vehicles_placed.push_back(placed);
            
            // ========== 终端输出：仅汇总信息 ==========
            std::cout << "  " << vkey << " 第" << vehicles << "辆车，装载" << placed.size()
                      << "件，容积利用率 " << std::fixed << std::setprecision(2) << vol_util*100
                      << "%，重量利用率 " << weight_util*100 << "%";
            if (placed.size() != batch.size()) 
                std::cout << " (未完全装入)";
            std::cout << "，剩余" << remaining.size() << "件" << std::endl;
            
            // ========== 文件输出：详细坐标 ==========
            if (detail_file.is_open()) {
                detail_file << "第" << vehicles << "辆车，装载" << placed.size() << "件，"
                            << "容积利用率 " << std::fixed << std::setprecision(2) << vol_util*100
                            << "%，重量利用率 " << weight_util*100 << "%\n";
                detail_file << "货物ID, x(cm), y(cm), z(cm), l(cm), w(cm), h(cm)\n";
                for (const auto& p : placed) {
                    detail_file << p.cargo->id << ", "
                                << std::fixed << std::setprecision(1)
                                << p.x << ", " << p.y << ", " << p.z << ", "
                                << p.l << ", " << p.w << ", " << p.h << "\n";
                }
                detail_file << "\n";
            }
            
            if (placed.empty()) {
                std::cout << "  警告：无法装入任何货物，终止该车型计算。" << std::endl;
                break;
            }
        }
        std::cout << vkey << " 总计需要 " << vehicles << " 辆车\n" << std::endl;
        results[vkey] = vehicles_placed;
    }
    
    if (detail_file.is_open()) {
        detail_file.close();
        std::cout << "详细坐标已写入文件: problem1_placement.txt" << std::endl;
    }
    return results;
}
// ---------------------- 问题二：多车型组合优化（已修正） ----------------------
void problem2() {
    std::cout << "\n===== 问题二：多车型组合优化 =====" << std::endl;
    auto cargo = generateCargoList();

    // ---------- 最少车辆方案 ----------
    SAPlanner p1(cargo, VEHICLE_TYPES, "min_vehicles");
    p1.temp = 100.0;
    p1.cool = 0.97;
    p1.min_temp = 0.1;
    p1.iter_per_temp = 60;
    auto [sol1, alloc1, hist1, acc1] = p1.optimize(3000);

    int v1c = 0, v2c = 0;
    for (const auto& v : sol1) {
        if (v.type == "V1") v1c++;
        else v2c++;
    }
    std::cout << "最少车辆方案：V1=" << v1c << ", V2=" << v2c << ", 总车数=" << sol1.size() << std::endl;

    std::cout << "  单车利用率详情 (最少车辆方案):" << std::endl;
    double total_vol_used = 0.0, total_weight_used = 0.0;
    for (size_t i = 0; i < alloc1.size(); ++i) {
        const auto& v = alloc1[i];
        double vol_cap = VEHICLE_TYPES.at(v.type).volume();
        double w_cap = VEHICLE_TYPES.at(v.type).max_load;
        double vol_u = 0.0, w_u = 0.0;
        for (const auto& p : v.placed) {
            vol_u += p.volume();
            w_u += p.cargo->weight;
        }
        total_vol_used += vol_u;
        total_weight_used += w_u;
        std::cout << "    车辆" << i+1 << " (" << v.type << "): 容积利用率 "
                  << std::fixed << std::setprecision(2) << vol_u / vol_cap * 100
                  << "%, 重量利用率 " << w_u / w_cap * 100 << "%";
        if (!v.success) std::cout << " (未完全装入)";
        std::cout << std::endl;
    }
    std::cout << "  总体容积利用率: " << total_vol_used / (v1c * VEHICLE_TYPES.at("V1").volume() + v2c * VEHICLE_TYPES.at("V2").volume()) * 100
              << "%, 重量利用率: " << total_weight_used / (v1c * VEHICLE_TYPES.at("V1").max_load + v2c * VEHICLE_TYPES.at("V2").max_load) * 100
              << "%" << std::endl;

    // ---------- 最低成本方案 ----------
    SAPlanner p2(cargo, VEHICLE_TYPES, "min_cost");
    p2.temp = 100.0;
    p2.cool = 0.97;
    p2.min_temp = 0.1;
    p2.iter_per_temp = 60;
    auto [sol2, alloc2, hist2, acc2] = p2.optimize(3000);

    double cost = 0.0;
    for (const auto& v : sol2) cost += VEHICLE_TYPES.at(v.type).cost_per_trip;
    v1c = std::count_if(sol2.begin(), sol2.end(), [](auto& v){return v.type=="V1";});
    v2c = std::count_if(sol2.begin(), sol2.end(), [](auto& v){return v.type=="V2";});
    std::cout << "最低成本方案：总成本=" << cost << "元, V1=" << v1c << ", V2=" << v2c << std::endl;

    std::cout << "  单车利用率详情 (最低成本方案):" << std::endl;
    total_vol_used = 0.0; total_weight_used = 0.0;
    for (size_t i = 0; i < alloc2.size(); ++i) {
        const auto& v = alloc2[i];
        double vol_cap = VEHICLE_TYPES.at(v.type).volume();
        double w_cap = VEHICLE_TYPES.at(v.type).max_load;
        double vol_u = 0.0, w_u = 0.0;
        for (const auto& p : v.placed) {
            vol_u += p.volume();
            w_u += p.cargo->weight;
        }
        total_vol_used += vol_u;
        total_weight_used += w_u;
        std::cout << "    车辆" << i+1 << " (" << v.type << "): 容积利用率 "
                  << std::fixed << std::setprecision(2) << vol_u / vol_cap * 100
                  << "%, 重量利用率 " << w_u / w_cap * 100 << "%";
        if (!v.success) std::cout << " (未完全装入)";
        std::cout << std::endl;
    }
    std::cout << "  总体容积利用率: " << total_vol_used / (v1c * VEHICLE_TYPES.at("V1").volume() + v2c * VEHICLE_TYPES.at("V2").volume()) * 100
              << "%, 重量利用率: " << total_weight_used / (v1c * VEHICLE_TYPES.at("V1").max_load + v2c * VEHICLE_TYPES.at("V2").max_load) * 100
              << "%" << std::endl;

    std::cout << "（绘图部分已移除）" << std::endl;
}

// ---------------------- 灵敏度分析（加速版） ----------------------
void sensitivity() {
    std::cout << "\n===== 灵敏度分析（加速版） =====" << std::endl;
    std::vector<double> mults = {0.5, 0.83333, 1.16667, 1.5};
    std::vector<double> facts = {0.8, 0.93333, 1.06667, 1.2};
    auto base_cargo_full = generateCargoList();
    std::vector<const CargoType*> base_cargo(base_cargo_full.begin(), base_cargo_full.begin()+500);
    const VehicleType& base_v = VEHICLE_TYPES.at("V1");

    for (double m : mults) {
        for (double f : facts) {
            int num_items = static_cast<int>(base_cargo.size() * m);
            num_items = std::min(num_items, (int)base_cargo.size());
            std::vector<const CargoType*> sub_cargo(base_cargo.begin(), base_cargo.begin()+num_items);
            VehicleType v("tmp", base_v.L * f, base_v.W * f, base_v.H * f,
                          base_v.max_load * f, 0);
            RealPacker packer(&v);
            auto [placed, util] = packer.gaOptimize(sub_cargo, 5);
            std::cout << "  乘子" << std::fixed << std::setprecision(2) << m
                      << " 因子" << f << " → 利用率" << std::setprecision(2) << util*100 << "%" << std::endl;
        }
    }
    std::cout << "（三维曲面图已移除）" << std::endl;
}

// ---------------------- 附件二验证 ----------------------
void validateAttachment2() {
    std::cout << "\n===== 附件二车型验证 =====" << std::endl;
    struct AttachVehicle { std::string name; double L, W, H; };
    std::vector<AttachVehicle> vehicles = {
        {"单节柜",590,235,239}, {"集装箱",1200,235,239}, {"超高柜",1200,235,269},
        {"4.2米箱货",410,190,200}, {"6.2米箱货",600,200,190}, {"6.8米箱货",680,250,220},
        {"9.6米箱货",950,250,220}, {"17米箱货",1700,250,220}
    };
    auto cargo_full = generateCargoList();
    std::vector<const CargoType*> cargo(cargo_full.begin(), cargo_full.begin()+100);
    for (const auto& av : vehicles) {
        VehicleType v(av.name, av.L, av.W, av.H, 1e6, 0);
        RealPacker packer(&v);
        auto [placed, util] = packer.gaOptimize(cargo, 10);
        std::cout << av.name << ": 利用率 " << std::fixed << std::setprecision(2) << util*100 << "%" << std::endl;
    }
}
int main() {
    
    rng.seed(42);

    auto start = std::chrono::steady_clock::now();

    // 问题一：单车型最少车辆数（输出到终端+详细坐标写入文件）
    auto results_p1 = problem1();

    // 问题二：多车型组合优化（已修正，打印详细利用率）
    problem2();

    // 灵敏度分析
    sensitivity();

    // 附件二验证
    validateAttachment2();

    // 输出问题一中 V1 第一辆车的利用率（原功能保留）
    if (results_p1.count("V1") && !results_p1["V1"].empty()) {
        const auto& first_vehicle_placed = results_p1["V1"][0];
        double vol_used = 0.0;
        for (const auto& p : first_vehicle_placed) vol_used += p.volume();
        double util = vol_used / VEHICLE_TYPES.at("V1").volume();
        std::cout << "\n车型1第一辆车利用率: " << std::fixed << std::setprecision(1) << util*100 << "%" << std::endl;
    }

    // ==================== 新增：单车多目标优化测试 ====================
    std::cout << "\n===== 单车多目标优化（容积+重量最大化） =====" << std::endl;
    auto all_cargo = generateCargoList();

    for (const auto& vkey : {"V1", "V2"}) {
        const VehicleType* vehicle = &VEHICLE_TYPES.at(vkey);
        RealPacker packer(vehicle);

        // 使用乘积最大化模式（兼顾容积和重量），运行50代
        auto [placed, vol_util, weight_util] = packer.optimizeMultiObjective(
            all_cargo, 50, RealPacker::OptMode::PRODUCT
        );

        std::cout << "\n车型: " << vehicle->name << std::endl;
        std::cout << "  目标模式: 容积率 × 重量率 最大化" << std::endl;
        std::cout << "  装载件数: " << placed.size() << " / " << all_cargo.size() << std::endl;
        std::cout << "  容积利用率: " << std::fixed << std::setprecision(2) << vol_util * 100 << "%" << std::endl;
        std::cout << "  重量利用率: " << weight_util * 100 << "%" << std::endl;
        std::cout << "  乘积得分: " << std::setprecision(4) << vol_util * weight_util << std::endl;

        // 可选：输出前5件货物的坐标
        std::cout << "  前5件货物坐标示例：" << std::endl;
        for (size_t i = 0; i < std::min<size_t>(5, placed.size()); ++i) {
            const auto& p = placed[i];
            std::cout << "    " << p.cargo->id << ": ("
                      << std::fixed << std::setprecision(1)
                      << p.x << ", " << p.y << ", " << p.z << ") "
                      << p.l << "x" << p.w << "x" << p.h << std::endl;
        }
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "\n全部完成，耗时 " << std::fixed << std::setprecision(2) << elapsed.count() << " 秒" << std::endl;
    return 0;
}