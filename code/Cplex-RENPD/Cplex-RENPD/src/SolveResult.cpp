#include "SolveResult.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace std;

SolveResult::SolveResult(int instance_id)
    : instance_id(instance_id) {
}

SolveResult::SolveResult(
    int instance_id,
    double upper_bound,
    double lower_bound,
    double solve_time,
    SolveStatus status)
    : instance_id(instance_id),
      upper_bound(upper_bound),
      lower_bound(lower_bound),
      solve_time(solve_time),
      status(status) {
}

bool SolveResult::hasFeasibleSolution() const {
    return status == SolveStatus::Optimal || status == SolveStatus::Feasible;
}

double SolveResult::absoluteGap() const {
    if (!hasFeasibleSolution()) {
        return numeric_limits<double>::infinity();
    }

    return fabs(upper_bound - lower_bound);
}

double SolveResult::relativeGap() const {
    if (!hasFeasibleSolution()) {
        return numeric_limits<double>::infinity();
    }

    const double denominator = max(1.0, fabs(upper_bound));
    return absoluteGap() / denominator;
}

string SolveResult::statusName() const {
    return statusName(status);
}

string SolveResult::statusName(SolveStatus status) {
    switch (status) {
    case SolveStatus::NotSolved:
        return "NotSolved";
    case SolveStatus::Optimal:
        return "Optimal";
    case SolveStatus::Feasible:
        return "Feasible";
    case SolveStatus::Infeasible:
        return "Infeasible";
    case SolveStatus::Unbounded:
        return "Unbounded";
    case SolveStatus::Error:
        return "Error";
    case SolveStatus::Unknown:
        return "Unknown";
    default:
        return "Unknown";
    }
}
