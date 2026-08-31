#pragma once

#include <string>

enum class SolveStatus {
    NotSolved,
    Optimal,
    Feasible,
    Infeasible,
    Error,
    Unknown
};

class SolveResult {
public:
    int instance_id = 0;
    std::string instance_name;

    double upper_bound = 0.0;
    double lower_bound = 0.0;
    double solve_time = 0.0;
    double shipping_cost = 0.0;
    double production_cost = 0.0;
    double inventory_cost = 0.0;
    double disruption_cost = 0.0;

    SolveStatus status = SolveStatus::NotSolved;
    std::string message;

    SolveResult() = default;
    explicit SolveResult(int instance_id);

    bool hasFeasibleSolution() const;
    std::string statusName() const;

    static std::string statusName(SolveStatus status);
};
