#include "ShipPlanGenerate.h"

#include <cmath>
#include <limits>
#include <sstream>

using namespace std;

namespace {
constexpr double kTolerance = 1.0e-9;
}

ShippingSchedule ShipPlanGenerate::generate(const Instance& instance) const {
    ShippingSchedule schedule;
    schedule.ship_day_by_order.assign(instance.n + 1, 0);
    schedule.y.assign(instance.m + 1, 0);

    for (int i = 1; i <= instance.n; i++) {
        const int initial_day = selectInitialShippingDay(instance, i);
        if (initial_day == 0) {
            ostringstream message;
            message << "Order " << i << " has no feasible shipping day.";
            schedule.message = message.str();
            return schedule;
        }

        schedule.ship_day_by_order[i] = initial_day;
        schedule.y[initial_day] += instance.quantity[i];
    }

    if (!repairCumulativeFeasibility(instance, schedule)) {
        return schedule;
    }

    schedule.shipping_cost = calculateShippingCost(instance, schedule.ship_day_by_order);
    schedule.feasible = true;
    return schedule;
}

int ShipPlanGenerate::selectInitialShippingDay(const Instance& instance, int order) const {
    int selected_day = 0;
    double best_cost = Instance::infeasible_cost;

    for (int t = instance.T; t <= instance.m; t++) {
        if (!instance.hasEligibleShippingMode(order, t)) {
            continue;
        }

        const double cost = instance.mo_ship_cost[order][t];
        if (cost < best_cost - kTolerance
            || (fabs(cost - best_cost) <= kTolerance && t > selected_day)) {
            best_cost = cost;
            selected_day = t;
        }
    }

    return selected_day;
}

bool ShipPlanGenerate::repairCumulativeFeasibility(
    const Instance& instance,
    ShippingSchedule& schedule) const {

    for (int t = instance.T; t <= instance.m; t++) {
        while (prefixQuantity(schedule.y, t) > t * instance.capacity) {
            int best_order = 0;
            int best_new_day = 0;
            double best_delta = numeric_limits<double>::infinity();

            for (int i = 1; i <= instance.n; i++) {
                const int current_day = schedule.ship_day_by_order[i];
                if (current_day <= 0 || current_day > t) {
                    continue;
                }

                for (int new_day = t + 1; new_day <= instance.m; new_day++) {
                    if (!instance.hasEligibleShippingMode(i, new_day)) {
                        continue;
                    }

                    const double delta = instance.quantity[i]
                        * (instance.mo_ship_cost[i][new_day]
                            - instance.mo_ship_cost[i][current_day]);
                    if (delta < best_delta - kTolerance
                        || (fabs(delta - best_delta) <= kTolerance && new_day < best_new_day)) {
                        best_delta = delta;
                        best_order = i;
                        best_new_day = new_day;
                    }
                }
            }

            if (best_order == 0) {
                ostringstream message;
                message << "Cannot repair cumulative shipping feasibility at day " << t << ".";
                schedule.message = message.str();
                return false;
            }

            const int old_day = schedule.ship_day_by_order[best_order];
            schedule.y[old_day] -= instance.quantity[best_order];
            schedule.y[best_new_day] += instance.quantity[best_order];
            schedule.ship_day_by_order[best_order] = best_new_day;
        }
    }

    return true;
}

int ShipPlanGenerate::prefixQuantity(const vector<int>& y, int day) const {
    int prefix = 0;
    for (int t = 1; t <= day; t++) {
        prefix += y[t];
    }

    return prefix;
}

double ShipPlanGenerate::calculateShippingCost(
    const Instance& instance,
    const vector<int>& ship_day_by_order) const {

    double cost = 0.0;
    for (int i = 1; i <= instance.n; i++) {
        const int day = ship_day_by_order[i];
        if (!instance.hasEligibleShippingMode(i, day)) {
            return Instance::infeasible_cost;
        }

        cost += instance.quantity[i] * instance.mo_ship_cost[i][day];
    }

    return cost;
}
