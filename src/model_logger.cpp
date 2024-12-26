#include "model_logger.h"

void ModelLogger::logWitnessPrefix(const z3::model& m, const Event& e1,
                                   const Event& e2) {
    std::vector<Event> events = trace_.getAllEvents();
    std::vector<std::pair<std::string, int>> event_order;
    int e1Idx, e2Idx;

    for (unsigned i = 0; i < m.size(); ++i) {
        z3::func_decl v = m[i];
        if (!v.range().is_int()) continue;

        z3::expr value = m.get_const_interp(v);
        assert(value.is_int());

        std::string name =
            v.name().str().substr(2);  // "order variables start with e_"

        if (name == std::to_string(e1.getEventId())) {
            e1Idx = value.get_numeral_int();
        } else if (name == std::to_string(e2.getEventId())) {
            e2Idx = value.get_numeral_int();
        }

        event_order.push_back({name, value.get_numeral_int()});
    }

    std::sort(event_order.begin(), event_order.end(),
              [](const std::pair<std::string, int>& a,
                 const std::pair<std::string, int>& b) {
                  return a.second < b.second;
              });

    log_file_ << "Witness for: e" << e1.getEventId() << " - e"
              << e2.getEventId() << "\n";

    int j = 1;
    for (const auto& [name, order] : event_order) {
        int i = std::stoi(name) - 1;
        if (order > e1Idx || order > e2Idx) break;
        if (i + 1 == e1.getEventId() || i + 1 == e2.getEventId()) continue;

        log_file_ << j++ << ": e" << name << " - " << events[i].prettyString()
                  << "\n";
    }

    if (e1Idx < e2Idx) {
        log_file_ << j++ << ": e" << e1.getEventId() << " - "
                  << e1.prettyString() << "\n";
        log_file_ << j++ << ": e" << e2.getEventId() << " - "
                  << e2.prettyString() << "\n";
    } else {
        log_file_ << j++ << ": e" << e2.getEventId() << " - "
                  << e2.prettyString() << "\n";
        log_file_ << j++ << ": e" << e1.getEventId() << " - "
                  << e1.prettyString() << "\n";
    }

    log_file_ << "------------------------------------------------------\n";
}