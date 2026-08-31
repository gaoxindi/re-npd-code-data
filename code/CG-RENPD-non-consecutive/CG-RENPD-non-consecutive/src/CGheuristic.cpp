#include "CGheuristic.h"

#include <ilcplex/ilocplex.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {
struct Column {
    int order = 0;
    int day = 0;
    double cost = 0.0;
};

struct MasterSolution {
    SolveStatus status = SolveStatus::NotSolved;
    double objective = 0.0;
    double best_bound = 0.0;
    double transport_cost = 0.0;
    double early_tardy_cost = 0.0;
    double inventory_cost = 0.0;
    double disruption_cost = 0.0;
    vector<double> alpha;
    vector<double> beta;
    string message;

    bool hasFeasibleSolution() const {
        return status == SolveStatus::Optimal || status == SolveStatus::Feasible;
    }
};

struct PricingResult {
    double min_reduced_cost = numeric_limits<double>::infinity();
    double best_new_reduced_cost = numeric_limits<double>::infinity();
    int best_new_day = 0;
};

struct ModifiedCostBreakdown {
    double transport = 0.0;
    double early_tardy = 0.0;
};

SolveStatus toSolveStatus(IloAlgorithm::Status status) {
    if (status == IloAlgorithm::Optimal) {
        return SolveStatus::Optimal;
    }
    if (status == IloAlgorithm::Feasible) {
        return SolveStatus::Feasible;
    }
    if (status == IloAlgorithm::Infeasible) {
        return SolveStatus::Infeasible;
    }
    if (status == IloAlgorithm::Unbounded) {
        return SolveStatus::Unbounded;
    }
    return SolveStatus::Unknown;
}

double elapsedSeconds(chrono::steady_clock::time_point start_time) {
    const auto end_time = chrono::steady_clock::now();
    return chrono::duration<double>(end_time - start_time).count();
}

double remainingTime(double time_limit, chrono::steady_clock::time_point start_time) {
    if (time_limit <= 0.0) {
        return 0.0;
    }

    return max(0.0, time_limit - elapsedSeconds(start_time));
}

double columnCost(const Instance& instance, int order, int day) {
    if (!instance.hasEligibleShippingMode(order, day)) {
        throw runtime_error("Attempted to create an infeasible shipping column.");
    }

    return instance.quantity[order] * instance.mo_ship_cost[order][day];
}

ModifiedCostBreakdown modifiedCostBreakdown(const Instance& instance, int order, int day) {
    if (!instance.hasEligibleShippingMode(order, day)) {
        throw runtime_error("Attempted to analyze an infeasible shipping column.");
    }

    double best_modified_cost = Instance::infeasible_cost;
    ModifiedCostBreakdown best;

    for (int j = 1; j <= instance.ship_mode_num[order]; j++) {
        const int ship_time = instance.ship_time[order][j];
        if (day + ship_time > instance.m) {
            continue;
        }

        const double early_tardy = instance.earli_cost[order]
            * max(0, instance.due_date[order] - day - ship_time)
            + instance.tardi_cost[order]
            * max(0, day + ship_time - instance.due_date[order]);
        const double modified_cost = instance.ship_cost[order][j] + early_tardy;

        if (modified_cost < best_modified_cost) {
            best_modified_cost = modified_cost;
            best.transport = instance.ship_cost[order][j];
            best.early_tardy = early_tardy;
        }
    }

    return best;
}

int firstFeasibleDayAtOrAfter(const Instance& instance, int order, int first_day) {
    for (int day = max(instance.T, first_day); day <= instance.latest_ship_day[order]; day++) {
        if (instance.hasEligibleShippingMode(order, day)) {
            return day;
        }
    }

    return 0;
}

void addColumn(
    const Instance& instance,
    int order,
    int day,
    vector<Column>& columns,
    vector<vector<char>>& has_column) {

    if (order <= 0 || order > instance.n || day <= 0 || day > instance.m) {
        throw runtime_error("Invalid column index.");
    }
    if (has_column[order][day]) {
        return;
    }

    columns.push_back(Column{ order, day, columnCost(instance, order, day) });
    has_column[order][day] = true;
}

vector<double> buildInventoryTailCost(const Instance& instance) {
    vector<double> tail_cost(instance.m + 2, 0.0);
    for (int t = instance.m; t >= 1; t--) {
        tail_cost[t] = tail_cost[t + 1] + instance.inven_cost[t];
    }

    return tail_cost;
}

MasterSolution solveRestrictedMaster(
    const Instance& instance,
    const vector<Column>& columns,
    bool integer_model,
    bool collect_duals,
    double time_limit,
    double mip_gap_tolerance) {

    MasterSolution solution;
    IloEnv env;

    try {
        IloModel model(env);
        IloObjective objective = IloAdd(model, IloMinimize(env));

        IloNumVarArray var_x(env);
        IloNumVarArray var_y(env);
        IloNumVarArray var_phi(env);
        for (int t = 1; t <= instance.m; t++) {
            if (integer_model) {
                var_x.add(IloNumVar(env, 0.0, IloInfinity, ILOINT));
                var_y.add(IloNumVar(env, 0.0, IloInfinity, ILOINT));
                var_phi.add(IloNumVar(env, 0.0, IloInfinity, ILOINT));
            }
            else {
                var_x.add(IloNumVar(env, 0.0, IloInfinity, ILOFLOAT));
                var_y.add(IloNumVar(env, 0.0, IloInfinity, ILOFLOAT));
                var_phi.add(IloNumVar(env, 0.0, IloInfinity, ILOFLOAT));
            }
        }

        IloNumVarArray var_theta(env);
        for (size_t col = 0; col < columns.size(); col++) {
            if (integer_model) {
                var_theta.add(IloNumVar(env, 0.0, 1.0, ILOINT));
            }
            else {
                var_theta.add(IloNumVar(env, 0.0, IloInfinity, ILOFLOAT));
            }
        }

        IloRangeArray dual_constraints(env, instance.n + instance.m);
        for (int i = 1; i <= instance.n; i++) {
            IloExpr assigned_once(env);
            for (size_t col = 0; col < columns.size(); col++) {
                if (columns[col].order == i) {
                    assigned_once += var_theta[static_cast<IloInt>(col)];
                }
            }
            dual_constraints[i - 1] = (assigned_once == 1.0);
            assigned_once.end();
        }

        for (int t = 1; t <= instance.m; t++) {
            IloExpr shipped_on_day(env);
            for (size_t col = 0; col < columns.size(); col++) {
                if (columns[col].day == t) {
                    shipped_on_day += instance.quantity[columns[col].order]
                        * var_theta[static_cast<IloInt>(col)];
                }
            }
            dual_constraints[instance.n + t - 1] = (shipped_on_day - var_y[t - 1] == 0.0);
            shipped_on_day.end();
        }
        model.add(dual_constraints);

        for (int t = 1; t <= instance.m; t++) {
            IloExpr shipped_to_date(env);
            IloExpr produced_to_date(env);
            for (int s = 1; s <= t; s++) {
                shipped_to_date += var_y[s - 1];
                produced_to_date += var_x[s - 1];
            }
            model.add(shipped_to_date <= produced_to_date);
            shipped_to_date.end();
            produced_to_date.end();
        }

        for (int t = 1; t <= instance.m; t++) {
            model.add(var_x[t - 1] <= instance.capacity);
            model.add(var_x[t - 1] - instance.xt_NPD[t] <= var_phi[t - 1]);
            model.add(instance.xt_NPD[t] - var_x[t - 1] <= var_phi[t - 1]);
        }

        const vector<double> inventory_tail_cost = buildInventoryTailCost(instance);
        IloExpr obj(env);
        for (size_t col = 0; col < columns.size(); col++) {
            obj += columns[col].cost * var_theta[static_cast<IloInt>(col)];
        }
        for (int t = 1; t <= instance.m; t++) {
            obj += inventory_tail_cost[t] * (var_x[t - 1] - var_y[t - 1]);
            obj += instance.disru_cost * var_phi[t - 1];
        }
        objective.setExpr(obj);
        obj.end();

        IloCplex solver(model);
        solver.setOut(env.getNullStream());
        if (time_limit > 0.0) {
            solver.setParam(IloCplex::TiLim, time_limit);
        }
        if (integer_model) {
            solver.setParam(IloCplex::MIPEmphasis, 4);
            solver.setParam(IloCplex::EpGap, mip_gap_tolerance);
        }

        solver.solve();
        solution.status = toSolveStatus(solver.getStatus());

        if (solution.hasFeasibleSolution()) {
            solution.objective = solver.getObjValue();
            try {
                solution.best_bound = solver.getBestObjValue();
            }
            catch (...) {
                solution.best_bound = solution.objective;
            }

            if (integer_model) {
                const vector<double> inventory_tail_cost = buildInventoryTailCost(instance);

                for (size_t col = 0; col < columns.size(); col++) {
                    const double theta_value = solver.getValue(var_theta[static_cast<IloInt>(col)]);
                    if (theta_value <= 1.0e-7) {
                        continue;
                    }

                    const ModifiedCostBreakdown breakdown = modifiedCostBreakdown(
                        instance,
                        columns[col].order,
                        columns[col].day);
                    const double quantity = static_cast<double>(instance.quantity[columns[col].order]);
                    solution.transport_cost += quantity * breakdown.transport * theta_value;
                    solution.early_tardy_cost += quantity * breakdown.early_tardy * theta_value;
                }

                for (int t = 1; t <= instance.m; t++) {
                    const double produced = solver.getValue(var_x[t - 1]);
                    const double shipped = solver.getValue(var_y[t - 1]);
                    const double disruption = solver.getValue(var_phi[t - 1]);
                    solution.inventory_cost += inventory_tail_cost[t] * (produced - shipped);
                    solution.disruption_cost += instance.disru_cost * disruption;
                }
            }

            if (collect_duals) {
                IloNumArray dual_values(env);
                solver.getDuals(dual_values, dual_constraints);
                solution.alpha.assign(instance.n + 1, 0.0);
                solution.beta.assign(instance.m + 1, 0.0);
                for (int i = 1; i <= instance.n; i++) {
                    solution.alpha[i] = dual_values[i - 1];
                }
                for (int t = 1; t <= instance.m; t++) {
                    solution.beta[t] = dual_values[instance.n + t - 1];
                }
                dual_values.end();
            }
        }

        env.end();
    }
    catch (IloException& e) {
        ostringstream message;
        message << e;
        solution.status = SolveStatus::Error;
        solution.message = message.str();
        env.end();
    }
    catch (exception& e) {
        solution.status = SolveStatus::Error;
        solution.message = e.what();
        env.end();
    }
    catch (...) {
        solution.status = SolveStatus::Error;
        solution.message = "Unknown CPLEX error.";
        env.end();
    }

    return solution;
}

PricingResult solvePricingSubproblem(
    const Instance& instance,
    int order,
    double alpha,
    const vector<double>& beta,
    const vector<vector<char>>& has_column) {

    PricingResult result;

    for (int day = instance.T; day <= instance.latest_ship_day[order]; day++) {
        if (!instance.hasEligibleShippingMode(order, day)) {
            continue;
        }

        const double reduced_cost = instance.quantity[order]
            * (instance.mo_ship_cost[order][day] - beta[day])
            - alpha;

        result.min_reduced_cost = min(result.min_reduced_cost, reduced_cost);

        if (!has_column[order][day] && reduced_cost < result.best_new_reduced_cost) {
            result.best_new_reduced_cost = reduced_cost;
            result.best_new_day = day;
        }
    }

    if (!isfinite(result.min_reduced_cost)) {
        throw runtime_error("Pricing subproblem has no feasible shipping day.");
    }

    return result;
}
}

CGheuristic::CGheuristic(
    double time_limit,
    double cg_gap_tolerance,
    double mip_gap_tolerance,
    double reduced_cost_tolerance,
    int max_iterations)
    : time_limit_(time_limit),
      cg_gap_tolerance_(cg_gap_tolerance),
      mip_gap_tolerance_(mip_gap_tolerance),
      reduced_cost_tolerance_(reduced_cost_tolerance),
      max_iterations_(max_iterations) {
}

SolveResult CGheuristic::solve(
    const Instance& instance,
    int instance_id,
    const string& instance_name) const {

    SolveResult result(instance_id);
    result.instance_name = instance_name;

    const auto start_time = chrono::steady_clock::now();

    try {
        if (instance.n <= 0 || instance.m <= 0) {
            throw runtime_error("Instance is empty.");
        }

        vector<Column> columns;
        columns.reserve(instance.n);
        vector<vector<char>> has_column(
            instance.n + 1,
            vector<char>(instance.m + 1, false));

        vector<int> initial_order_sequence(instance.n);
        for (int i = 1; i <= instance.n; i++) {
            initial_order_sequence[i - 1] = i;
        }
        sort(
            initial_order_sequence.begin(),
            initial_order_sequence.end(),
            [&instance](int lhs, int rhs) {
                if (instance.latest_ship_day[lhs] != instance.latest_ship_day[rhs]) {
                    return instance.latest_ship_day[lhs] < instance.latest_ship_day[rhs];
                }
                return lhs < rhs;
            });

        int cumulative_quantity = 0;
        for (int i : initial_order_sequence) {
            cumulative_quantity += instance.quantity[i];
            const int completion_day = static_cast<int>(
                ceil(static_cast<double>(cumulative_quantity)
                    / static_cast<double>(instance.capacity)));
            const int initial_day = firstFeasibleDayAtOrAfter(instance, i, completion_day);
            if (initial_day == 0) {
                throw runtime_error("Cannot build an initial feasible column set.");
            }

            addColumn(instance, i, initial_day, columns, has_column);
        }

        double lower_bound = 0.0;
        double lp_upper_bound = numeric_limits<double>::infinity();
        int iteration = 0;
        int solved_lp_iterations = 0;

        for (; iteration < max_iterations_; iteration++) {
            const double lp_time_limit = remainingTime(time_limit_, start_time);
            if (time_limit_ > 0.0 && lp_time_limit <= 0.0) {
                break;
            }

            MasterSolution lp_solution = solveRestrictedMaster(
                instance,
                columns,
                false,
                true,
                lp_time_limit,
                mip_gap_tolerance_);

            if (!lp_solution.hasFeasibleSolution()) {
                result.status = lp_solution.status;
                result.message = "RDWLP failed: " + lp_solution.message;
                result.solve_time = elapsedSeconds(start_time);
                return result;
            }

            solved_lp_iterations++;
            lp_upper_bound = lp_solution.objective;

            bool added_column = false;
            double negative_reduced_cost_sum = 0.0;
            for (int i = 1; i <= instance.n; i++) {
                const PricingResult pricing = solvePricingSubproblem(
                    instance,
                    i,
                    lp_solution.alpha[i],
                    lp_solution.beta,
                    has_column);

                negative_reduced_cost_sum += min(0.0, pricing.min_reduced_cost);

                if (pricing.best_new_day != 0
                    && pricing.best_new_reduced_cost < -reduced_cost_tolerance_) {
                    addColumn(instance, i, pricing.best_new_day, columns, has_column);
                    added_column = true;
                }
            }

            lower_bound = lp_upper_bound + negative_reduced_cost_sum;

            if (!added_column) {
                break;
            }

            if (lower_bound > 0.0
                && (lp_upper_bound - lower_bound) / lower_bound <= cg_gap_tolerance_) {
                break;
            }
        }

        const double mip_time_limit = remainingTime(time_limit_, start_time);
        if (time_limit_ > 0.0 && mip_time_limit <= 0.0) {
            result.status = SolveStatus::Unknown;
            result.lower_bound = lower_bound;
            result.message = "Time limit reached before solving RIP.";
            result.solve_time = elapsedSeconds(start_time);
            return result;
        }

        MasterSolution mip_solution = solveRestrictedMaster(
            instance,
            columns,
            true,
            false,
            mip_time_limit,
            mip_gap_tolerance_);

        result.solve_time = elapsedSeconds(start_time);
        result.lower_bound = lower_bound;

        if (mip_solution.hasFeasibleSolution()) {
            result.upper_bound = mip_solution.objective;
            result.transport_cost = mip_solution.transport_cost;
            result.early_tardy_cost = mip_solution.early_tardy_cost;
            result.inventory_cost = mip_solution.inventory_cost;
            result.disruption_cost = mip_solution.disruption_cost;
            result.status = SolveStatus::Feasible;
            ostringstream message;
            message << "CG iterations=" << solved_lp_iterations;
            result.message = message.str();
        }
        else {
            result.status = mip_solution.status;
            result.message = "RIP failed: " + mip_solution.message;
        }
    }
    catch (exception& e) {
        result.status = SolveStatus::Error;
        result.message = e.what();
        result.solve_time = elapsedSeconds(start_time);
    }
    catch (...) {
        result.status = SolveStatus::Error;
        result.message = "Unknown CG heuristic error.";
        result.solve_time = elapsedSeconds(start_time);
    }

    return result;
}

double CGheuristic::timeLimit() const {
    return time_limit_;
}

void CGheuristic::setTimeLimit(double time_limit) {
    time_limit_ = time_limit;
}

double CGheuristic::cgGapTolerance() const {
    return cg_gap_tolerance_;
}

void CGheuristic::setCgGapTolerance(double tolerance) {
    cg_gap_tolerance_ = tolerance;
}

double CGheuristic::mipGapTolerance() const {
    return mip_gap_tolerance_;
}

void CGheuristic::setMipGapTolerance(double tolerance) {
    mip_gap_tolerance_ = tolerance;
}

int CGheuristic::maxIterations() const {
    return max_iterations_;
}

void CGheuristic::setMaxIterations(int max_iterations) {
    max_iterations_ = max_iterations;
}
