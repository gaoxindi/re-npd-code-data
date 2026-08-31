#pragma once

#include "Instance.h"

#include <string>
#include <vector>

struct ProductionSchedule {
    bool feasible = false;
    std::vector<double> x;
    double objective = 0.0;
    double inventory_cost = 0.0;
    double disruption_cost = 0.0;
    std::string message;
};

class MinCostFlowSolver {
public:
    explicit MinCostFlowSolver(double time_limit = 300.0);

    ProductionSchedule solve(
        const Instance& instance,
        const std::vector<int>& shipped_quantity) const;

    double timeLimit() const;
    void setTimeLimit(double time_limit);

private:
    double time_limit_ = 300.0;
};
