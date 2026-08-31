#pragma once

#include <string>
#include <vector>

class Instance {
public:
    static constexpr double infeasible_cost = 1.0e12;

    int n = 0;
    int m = 0;
    int T = 0;
    int capacity = 0;
    int total_quan = 0;

    double disru_cost = 0.0;

    std::vector<int> due_date;
    std::vector<int> quantity;
    std::vector<int> ship_mode_num;
    std::vector<int> latest_ship_day;
    std::vector<int> xt_NPD;

    std::vector<double> inven_cost;
    std::vector<double> earli_cost;
    std::vector<double> tardi_cost;

    std::vector<std::vector<int>> ship_time;
    std::vector<std::vector<double>> ship_cost;
    std::vector<std::vector<double>> mo_ship_cost;

    void readFromFile(const std::string& filename);
    void readFromInstanceId(int instance_id, const std::string& directory = "");

    bool hasEligibleShippingMode(int order, int day) const;

private:
    void buildNPDsolution();
    void buildModifiedCost();
};
