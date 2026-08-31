#include "G3network.h"

#include <sstream>
#include <stdexcept>

using namespace std;

G3network::G3network(const Instance& instance)
    : m_(instance.m) {
    if (m_ <= 0) {
        throw runtime_error("Cannot build G3 network for an empty instance.");
    }

    const double total_quantity = static_cast<double>(instance.total_quan);

    for (int t = 1; t <= m_; t++) {
        addArc(d7(t), d8(t), 0.0, total_quantity, G3ArcType::KeepProduction, t, t);
        addArc(d7(t), r3(), instance.disru_cost, total_quantity, G3ArcType::DecreaseProduction, t, 0);
        addArc(r3(), d8(t), instance.disru_cost, total_quantity, G3ArcType::IncreaseProduction, 0, t);
        addArc(d8(t), d9(t), 0.0, instance.capacity, G3ArcType::Production, t, t);
    }

    for (int u = 1; u <= m_; u++) {
        double inventory_cost = 0.0;
        for (int v = u; v <= m_; v++) {
            if (v > u) {
                inventory_cost += instance.inven_cost[v - 1];
            }
            addArc(d9(u), d10(v), inventory_cost, total_quantity, G3ArcType::Inventory, u, v);
        }
    }
}

int G3network::nodeCount() const {
    return 4 * m_ + 1;
}

const vector<G3Arc>& G3network::arcs() const {
    return arcs_;
}

int G3network::d7(int day) const {
    return day;
}

int G3network::d8(int day) const {
    return m_ + day;
}

int G3network::d9(int day) const {
    return 2 * m_ + day;
}

int G3network::d10(int day) const {
    return 3 * m_ + day;
}

int G3network::r3() const {
    return 4 * m_ + 1;
}

string G3network::nodeName(int node) const {
    if (node == r3()) {
        return "R3";
    }
    if (node >= d7(1) && node <= d7(m_)) {
        return "D7(" + to_string(node) + ")";
    }
    if (node >= d8(1) && node <= d8(m_)) {
        return "D8(" + to_string(node - m_) + ")";
    }
    if (node >= d9(1) && node <= d9(m_)) {
        return "D9(" + to_string(node - 2 * m_) + ")";
    }
    if (node >= d10(1) && node <= d10(m_)) {
        return "D10(" + to_string(node - 3 * m_) + ")";
    }

    ostringstream name;
    name << "Unknown(" << node << ")";
    return name.str();
}

string G3network::arcTypeName(G3ArcType type) const {
    switch (type) {
    case G3ArcType::KeepProduction:
        return "KeepProduction";
    case G3ArcType::DecreaseProduction:
        return "DecreaseProduction";
    case G3ArcType::IncreaseProduction:
        return "IncreaseProduction";
    case G3ArcType::Production:
        return "Production";
    case G3ArcType::Inventory:
        return "Inventory";
    default:
        return "Unknown";
    }
}

void G3network::addArc(
    int from,
    int to,
    double cost,
    double capacity,
    G3ArcType type,
    int from_day,
    int to_day) {

    G3Arc arc;
    arc.id = static_cast<int>(arcs_.size());
    arc.from = from;
    arc.to = to;
    arc.cost = cost;
    arc.capacity = capacity;
    arc.type = type;
    arc.from_day = from_day;
    arc.to_day = to_day;
    arcs_.push_back(arc);
}
