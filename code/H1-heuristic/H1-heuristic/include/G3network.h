#pragma once

#include "Instance.h"

#include <string>
#include <vector>

enum class G3ArcType {
    KeepProduction,
    DecreaseProduction,
    IncreaseProduction,
    Production,
    Inventory
};

struct G3Arc {
    int id = 0;
    int from = 0;
    int to = 0;
    int from_day = 0;
    int to_day = 0;
    double cost = 0.0;
    double capacity = 0.0;
    G3ArcType type = G3ArcType::KeepProduction;
};

class G3network {
public:
    G3network() = default;
    explicit G3network(const Instance& instance);

    int nodeCount() const;
    const std::vector<G3Arc>& arcs() const;

    int d7(int day) const;
    int d8(int day) const;
    int d9(int day) const;
    int d10(int day) const;
    int r3() const;

    std::string nodeName(int node) const;
    std::string arcTypeName(G3ArcType type) const;

private:
    int m_ = 0;
    std::vector<G3Arc> arcs_;

    void addArc(
        int from,
        int to,
        double cost,
        double capacity,
        G3ArcType type,
        int from_day,
        int to_day);
};
