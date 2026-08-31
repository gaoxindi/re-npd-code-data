#pragma once

#include <string>

enum class SolveStatus {
    NotSolved,
    Optimal,
    Feasible,
    Infeasible,
    Unbounded,
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
    SolveStatus status = SolveStatus::NotSolved;
    std::string message;

    SolveResult() = default;
    explicit SolveResult(int instance_id);
    SolveResult(
        int instance_id,
        double upper_bound,
        double lower_bound,
        double solve_time,
        SolveStatus status);

    bool hasFeasibleSolution() const;
    double absoluteGap() const;
    double relativeGap() const;
    std::string statusName() const;

    static std::string statusName(SolveStatus status);
};
