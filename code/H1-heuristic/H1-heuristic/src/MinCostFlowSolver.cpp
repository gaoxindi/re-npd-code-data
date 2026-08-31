#include "MinCostFlowSolver.h"

#include "G3network.h"

#include <ilcplex/ilocplex.h>

#include <cmath>
#include <exception>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

MinCostFlowSolver::MinCostFlowSolver(double time_limit)
    : time_limit_(time_limit) {
}

ProductionSchedule MinCostFlowSolver::solve(
    const Instance& instance,
    const vector<int>& shipped_quantity) const {

    ProductionSchedule schedule;
    schedule.x.assign(instance.m + 1, 0.0);

    if (static_cast<int>(shipped_quantity.size()) <= instance.m) {
        schedule.message = "Invalid shipping schedule length.";
        return schedule;
    }

    int total_shipped = 0;
    for (int t = 1; t <= instance.m; t++) {
        if (shipped_quantity[t] < 0) {
            schedule.message = "Invalid negative shipping quantity.";
            return schedule;
        }
        total_shipped += shipped_quantity[t];
    }

    if (total_shipped != instance.total_quan) {
        schedule.message = "Shipping schedule does not ship all products.";
        return schedule;
    }

    IloEnv env;
    try {
        G3network network(instance);
        IloModel model(env);
        IloObjective objective = IloAdd(model, IloMinimize(env));

        IloNumVarArray flow(env, static_cast<IloInt>(network.arcs().size()));
        IloExpr obj(env);
        for (const G3Arc& arc : network.arcs()) {
            flow[arc.id] = IloNumVar(env, 0.0, arc.capacity, ILOFLOAT);
            obj += arc.cost * flow[arc.id];
        }
        objective.setExpr(obj);
        obj.end();

        vector<double> supply(network.nodeCount() + 1, 0.0);
        for (int t = 1; t <= instance.m; t++) {
            supply[network.d7(t)] = static_cast<double>(instance.xt_NPD[t]);
            supply[network.d10(t)] = -static_cast<double>(shipped_quantity[t]);
        }

        IloExprArray balance(env, network.nodeCount() + 1);
        for (int node = 1; node <= network.nodeCount(); node++) {
            balance[node] = IloExpr(env);
        }

        for (const G3Arc& arc : network.arcs()) {
            balance[arc.from] += flow[arc.id];
            balance[arc.to] -= flow[arc.id];
        }

        for (int node = 1; node <= network.nodeCount(); node++) {
            model.add(balance[node] == supply[node]);
            balance[node].end();
        }
        balance.end();

        IloCplex solver(model);
        solver.setOut(env.getNullStream());
        solver.setParam(IloCplex::TiLim, time_limit_);
        solver.setParam(IloCplex::RootAlg, IloCplex::Network);

        if (!solver.solve() || solver.getStatus() != IloAlgorithm::Optimal) {
            schedule.message = "CPLEX did not solve G3 to optimality.";
            env.end();
            return schedule;
        }

        schedule.objective = solver.getObjValue();
        schedule.inventory_cost = 0.0;
        schedule.disruption_cost = 0.0;

        for (const G3Arc& arc : network.arcs()) {
            const double value = solver.getValue(flow[arc.id]);
            if (value <= 1.0e-7) {
                continue;
            }

            if (arc.type == G3ArcType::Production) {
                schedule.x[arc.from_day] = value;
            }
            else if (arc.type == G3ArcType::Inventory) {
                schedule.inventory_cost += arc.cost * value;
            }
            else if (arc.type == G3ArcType::DecreaseProduction
                || arc.type == G3ArcType::IncreaseProduction) {
                schedule.disruption_cost += arc.cost * value;
            }
        }

        for (int t = 1; t <= instance.m; t++) {
            const double rounded = round(schedule.x[t]);
            if (fabs(schedule.x[t] - rounded) <= 1.0e-6) {
                schedule.x[t] = rounded;
            }
        }

        schedule.feasible = true;
        env.end();
    }
    catch (IloException& e) {
        ostringstream message;
        message << e;
        schedule.message = message.str();
        env.end();
    }
    catch (exception& e) {
        schedule.message = e.what();
        env.end();
    }
    catch (...) {
        schedule.message = "Unknown CPLEX error while solving G3.";
        env.end();
    }

    return schedule;
}

double MinCostFlowSolver::timeLimit() const {
    return time_limit_;
}

void MinCostFlowSolver::setTimeLimit(double time_limit) {
    time_limit_ = time_limit;
}
