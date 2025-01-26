#include "model_logger.hpp"

#include "BSlogger.hpp"

void ModelLogger::logWitnessPrefix(const z3::model& m, const Event& e1,
                                   const Event& e2) {
    LOG_INIT_COUT();
    std::vector<Event> events = trace_.getAllEvents();
    std::vector<std::pair<std::string, int>> event_order;
    int e1Idx, e2Idx;

    std::unordered_map<uint32_t, uint32_t> firstInfeasibleEventInThread;

    for (auto thread : trace_.getThreads()) {
        firstInfeasibleEventInThread[thread.getThreadId()] = 0;
    }

    for (unsigned i = 0; i < m.size(); ++i) {
        z3::func_decl v = m[i];

        if (v.name().str().substr(0, 3) == "phi") {
            z3::expr value = m.get_const_interp(v);
            assert(value.is_bool());

            std::string name = v.name().str().substr(4);
            uint32_t eid = static_cast<uint32_t>(std::stoul(name));
            Event e = trace_.getEvent(eid);

            if (value.bool_value() == 1)
                continue;

            if (e.getThreadId() == -1)
                log(LOG_INFO) << v.name() << ", " << eid << ": " << value.bool_value() << "\n";

            if (firstInfeasibleEventInThread[e.getThreadId()] == 0) 
                firstInfeasibleEventInThread[e.getThreadId()] = eid;

            if (!Event::isNullEvent(trace_.getThread(e.getThreadId()).getPrevAcq(e)))
                eid = trace_.getThread(e.getThreadId()).getPrevAcq(e).getEventId();
            
            firstInfeasibleEventInThread[e.getThreadId()] = std::min(eid, firstInfeasibleEventInThread[e.getThreadId()]);
        }

        // log(LOG_INFO) << firstInfeasibleEventInThread[1] << "\n";

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
    std::vector<uint32_t> witness;
    for (const auto& [name, order] : event_order) {
        uint32_t eid = static_cast<uint32_t>(std::stoul(name));
        Event e = trace_.getEvent(eid);

        assert(firstInfeasibleEventInThread.find(e.getThreadId()) != firstInfeasibleEventInThread.end());

        if (firstInfeasibleEventInThread[e.getThreadId()] != 0 && firstInfeasibleEventInThread[e.getThreadId()] <= eid)
            continue;

        int i = std::stoi(name) - 1;
        if (order > e1Idx || order > e2Idx) break;
        if (i + 1 == e1.getEventId() || i + 1 == e2.getEventId()) continue;

        if (log_binary_witness_)
            witness.push_back(std::stoul(name));
        log_file_ << j++ << ": e" << name << " - " << events[i].prettyString()
                  << "\n";
    }

    if (e1Idx < e2Idx) {
        if (log_binary_witness_) {
            witness.push_back(e1.getEventId());
            witness.push_back(e2.getEventId());
        }
        
        log_file_ << j++ << ": e" << e1.getEventId() << " - "
                  << e1.prettyString() << "\n";
        log_file_ << j++ << ": e" << e2.getEventId() << " - "
                  << e2.prettyString() << "\n";
    } else {
        if (log_binary_witness_) {
            witness.push_back(e2.getEventId());
            witness.push_back(e1.getEventId());
        }
        
        log_file_ << j++ << ": e" << e2.getEventId() << " - "
                  << e2.prettyString() << "\n";
        log_file_ << j++ << ": e" << e1.getEventId() << " - "
                  << e1.prettyString() << "\n";
    }

    if (log_binary_witness_) {
        uint32_t size = witness.size();
        binary_log_file_.write(reinterpret_cast<const char*>(&size),
                            sizeof(uint32_t));
        binary_log_file_.write(reinterpret_cast<const char*>(witness.data()),
                            size * sizeof(uint32_t));
    }

    log_file_ << "------------------------------------------------------\n";
}

void ModelLogger::logBinaryWitnessPrefix(const z3::model& m, const Event& e1,
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

    std::vector<uint32_t> witness;

    for (const auto& [name, order] : event_order) {
        uint32_t i = std::stoul(name) - 1;
        if (order > e1Idx || order > e2Idx) break;
        if (i + 1 == e1.getEventId() || i + 1 == e2.getEventId()) continue;

        witness.push_back(std::stoul(name));
    }
    if (e1Idx < e2Idx) {
        witness.push_back(e1.getEventId());
        witness.push_back(e2.getEventId());
    } else {
        witness.push_back(e2.getEventId());
        witness.push_back(e1.getEventId());
    }

    uint32_t size = witness.size();
    binary_log_file_.write(reinterpret_cast<const char*>(&size),
                           sizeof(uint32_t));
    binary_log_file_.write(reinterpret_cast<const char*>(witness.data()),
                           size * sizeof(uint32_t));
}

std::vector<std::vector<uint32_t>> ModelLogger::readBinaryWitness(
    const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open binary witness file");
    }

    std::vector<std::vector<uint32_t>> witnesses;
    while (file.peek() != EOF) {
        uint32_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        std::vector<uint32_t> witness(size);
        file.read(reinterpret_cast<char*>(witness.data()),
                  size * sizeof(uint32_t));
        witnesses.push_back(witness);
    }

    return witnesses;
}