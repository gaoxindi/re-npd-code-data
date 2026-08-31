#include "Instance.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace std;

namespace {
template <typename T>
void readValue(ifstream& input, T& value, const string& filename) {
    if (!(input >> value)) {
        throw runtime_error("Invalid or incomplete instance data in file: " + filename);
    }
}
}

void Instance::readFromFile(const string& filename) {
    ifstream instuf(filename);
    if (!instuf.is_open()) {
        throw runtime_error("Cannot open instance file: " + filename);
    }

    readValue(instuf, n, filename);
    readValue(instuf, m, filename);
    readValue(instuf, T, filename);
    readValue(instuf, capacity, filename);
    readValue(instuf, disru_cost, filename);

    if (n <= 0 || m <= 0 || T <= 0 || T > m || capacity <= 0) {
        throw runtime_error("Invalid instance dimensions in file: " + filename);
    }
    if (disru_cost < 0.0) {
        throw runtime_error("Invalid disruption cost in file: " + filename);
    }

    ship_mode_num.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        readValue(instuf, ship_mode_num[i], filename);
        if (ship_mode_num[i] <= 0 || ship_mode_num[i] > m) {
            throw runtime_error("Invalid shipping mode count in file: " + filename);
        }
    }

    ship_time.assign(n + 1, vector<int>());
    for (int i = 1; i <= n; i++) {
        ship_time[i].assign(ship_mode_num[i] + 1, 0);
        for (int j = 1; j <= ship_mode_num[i]; j++) {
            readValue(instuf, ship_time[i][j], filename);
            if (ship_time[i][j] < 0 || ship_time[i][j] >= m) {
                throw runtime_error("Invalid shipping time in file: " + filename);
            }
            if (j > 1 && ship_time[i][j] < ship_time[i][j - 1]) {
                throw runtime_error("Shipping times must be increasing in file: " + filename);
            }
        }
    }

    ship_cost.assign(n + 1, vector<double>());
    for (int i = 1; i <= n; i++) {
        ship_cost[i].assign(ship_mode_num[i] + 1, 0.0);
        for (int j = 1; j <= ship_mode_num[i]; j++) {
            readValue(instuf, ship_cost[i][j], filename);
            if (ship_cost[i][j] < 0.0) {
                throw runtime_error("Invalid shipping cost in file: " + filename);
            }
        }
    }

    earli_cost.assign(n + 1, 0.0);
    for (int i = 1; i <= n; i++) {
        readValue(instuf, earli_cost[i], filename);
        if (earli_cost[i] < 0.0) {
            throw runtime_error("Invalid earliness cost in file: " + filename);
        }
    }

    tardi_cost.assign(n + 1, 0.0);
    for (int i = 1; i <= n; i++) {
        readValue(instuf, tardi_cost[i], filename);
        if (tardi_cost[i] < 0.0) {
            throw runtime_error("Invalid tardiness cost in file: " + filename);
        }
    }

    inven_cost.assign(m + 1, 0.0);
    for (int t = 1; t <= m; t++) {
        readValue(instuf, inven_cost[t], filename);
        if (inven_cost[t] < 0.0) {
            throw runtime_error("Invalid inventory cost in file: " + filename);
        }
    }

    total_quan = 0;
    quantity.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        readValue(instuf, quantity[i], filename);
        if (quantity[i] <= 0) {
            throw runtime_error("Invalid order quantity in file: " + filename);
        }
        total_quan += quantity[i];
    }

    due_date.assign(n + 1, 0);
    for (int i = 1; i <= n; i++) {
        readValue(instuf, due_date[i], filename);
        if (due_date[i] <= 0 || due_date[i] > m) {
            throw runtime_error("Invalid due date in file: " + filename);
        }
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

bool Instance::hasEligibleShippingMode(int order, int day) const {
    if (order <= 0 || order > n || day <= 0 || day > m) {
        return false;
    }
    if (order >= static_cast<int>(mo_ship_cost.size())
        || day >= static_cast<int>(mo_ship_cost[order].size())) {
        return false;
    }

    return mo_ship_cost[order][day] < infeasible_cost / 2.0;
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
    mo_ship_cost.assign(n + 1, vector<double>(m + 1, infeasible_cost));
    latest_ship_day.assign(n + 1, 0);

    for (int i = 1; i <= n; i++) {
        int min_ship_time = numeric_limits<int>::max();
        for (int j = 1; j <= ship_mode_num[i]; j++) {
            min_ship_time = min(min_ship_time, ship_time[i][j]);
        }

        latest_ship_day[i] = m - min_ship_time;
        if (latest_ship_day[i] < T) {
            throw runtime_error("Disruption duration leaves an order with no feasible shipping day.");
        }

        for (int t = 1; t <= m; t++) {
            double min_modified_cost = infeasible_cost;

            for (int j = 1; j <= ship_mode_num[i]; j++) {
                const int s = ship_time[i][j];
                if (t + s > m) {
                    continue;
                }

                const double candidate = ship_cost[i][j]
                    + earli_cost[i] * max(0, due_date[i] - t - s)
                    + tardi_cost[i] * max(0, t + s - due_date[i]);

                min_modified_cost = min(min_modified_cost, candidate);
            }

            mo_ship_cost[i][t] = min_modified_cost;
        }
    }

    for (int i = 1; i <= n; i++) {
        for (int t = 1; t <= T - 1; t++) {
            mo_ship_cost[i][t] = mo_ship_cost[i][T];
        }
    }
}
