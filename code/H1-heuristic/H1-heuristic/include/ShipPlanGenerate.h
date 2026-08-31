#pragma once

#include "Instance.h"

#include <string>
#include <vector>

struct ShippingSchedule {
    bool feasible = false;
    std::vector<int> ship_day_by_order;
    std::vector<int> y;
    double shipping_cost = 0.0;
    std::string message;
};

class ShipPlanGenerate {
public:
    ShippingSchedule generate(const Instance& instance) const;

private:
    int selectInitialShippingDay(const Instance& instance, int order) const;
    bool repairCumulativeFeasibility(const Instance& instance, ShippingSchedule& schedule) const;
    int prefixQuantity(const std::vector<int>& y, int day) const;
    double calculateShippingCost(const Instance& instance, const std::vector<int>& ship_day_by_order) const;
};
