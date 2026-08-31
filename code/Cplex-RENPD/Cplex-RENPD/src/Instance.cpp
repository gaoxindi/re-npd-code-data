#include "Instance.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;

void Instance::readFromFile(const string& filename) {
    ifstream instuf(filename);
    if (!instuf.is_open()) {
        throw runtime_error("Cannot open instance file: " + filename);
    }

    instuf >> n;
    instuf >> m;
    instuf >> T;
    instuf >> capacity;
    instuf >> disru_cost;

    if (n <= 0 || m <= 0 || T <= 0 || T > m || capacity <= 0) {
        throw runtime_error("Invalid instance dimensions in file: " + filename);
    }

    ship_A.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        instuf >> ship_A[i];
    }

    ship_B.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        instuf >> ship_B[i];
    }

    earli_cost.assign(n + 1, 0.0);
    for (int i = 1; i <= n; i++) {
        instuf >> earli_cost[i];
    }

    tardi_cost.assign(n + 1, 0.0);
    for (int i = 1; i <= n; i++) {
        instuf >> tardi_cost[i];
    }

    inven_cost.assign(m + 1, 0.0);
    for (int t = 1; t <= m; t++) {
        instuf >> inven_cost[t];
    }

    total_quan = 0;
    quantity.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        instuf >> quantity[i];
        total_quan += quantity[i];
    }

    due_date.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        instuf >> due_date[i];
    }

    if (!instuf) {
        throw runtime_error("Invalid or incomplete instance data in file: " + filename);
    }

    buildNPDsolution();
    buildModifiedCost();
}

void Instance::readFromInstanceId(int instance_id, const string& directory) {
    filesystem::path filename = "instance" + to_string(instance_id) + ".txt";
    if (!directory.empty()) {
        filename = filesystem::path(directory) / filename;
    }

    readFromFile(filename.string());
}

void Instance::buildNPDsolution() {
    xt_NPD.assign(m + 1, 0);

    if (total_quan == 0) {
        return;
    }

    const int active_days = static_cast<int>(
        ceil(static_cast<double>(total_quan) / static_cast<double>(capacity)));

    if (active_days > m) {
        throw runtime_error("Total quantity exceeds available production capacity.");
    }

    for (int t = 1; t < active_days; t++) {
        xt_NPD[t] = capacity;
    }
    xt_NPD[active_days] = total_quan - capacity * (active_days - 1);
}

void Instance::buildModifiedCost() {
    mo_ship_cost.assign(n + 1, vector<double>(m + 1, 0.0));

    for (int i = 1; i <= n; i++) {
        vector<double> func_g(m, 0.0);
        for (int s = 0; s < m; s++) {
            func_g[s] = static_cast<double>(ship_B[i]) / static_cast<double>(1 + s)
                + static_cast<double>(ship_A[i]);
        }

        for (int t = 1; t <= m; t++) {
            double min_mo_cost = 1000000.0;

            for (int s = 0; s < m; s++) {
                double temp_mo_cost = 1000000.0;
                if (t + s <= m) {
                    temp_mo_cost = func_g[s]
                        + earli_cost[i] * max(0, due_date[i] - t - s)
                        + tardi_cost[i] * max(0, t + s - due_date[i]);
                }
                else {
                    // Preserve the legacy out-of-horizon penalty. In CPLEX1204.cpp,
                    // the global variable d is zero-initialized and never changed here.
                    temp_mo_cost = func_g[s] + tardi_cost[i] * m;
                }

                if (temp_mo_cost < min_mo_cost) {
                    min_mo_cost = temp_mo_cost;
                }
            }

            mo_ship_cost[i][t] = min_mo_cost;
        }
    }

    for (int i = 1; i <= n; i++) {
        for (int t = 1; t <= T - 1; t++) {
            mo_ship_cost[i][t] = mo_ship_cost[i][T];
        }
    }
}
