#include "model_logger.hpp"

void ModelLogger::logWitnessPrefix(const z3::model& m, const Event& e1,
                                   const Event& e2) {
    std::vector<Event> events = trace_.getAllEvents();
    std::vector<std::pair<std::string, int>> event_order;
    int e1Idx, e2Idx;

    std::unordered_map<uint32_t, uint32_t> firstInfeasibleEventInThread;

    for (auto thread : trace_.getThreads()) {
        firstInfeasibleEventInThread[thread.getThreadId()] = 0;
    }

    for (unsigned i = 0; i < m.size(); ++i) {
        z3::func_decl v = m[static_cast<int>(i)];

        if (v.name().str().substr(0, 3) == "phi") {
            z3::expr value = m.get_const_interp(v);
            assert(value.is_bool());

            std::string name = v.name().str().substr(4);
            uint32_t eid = static_cast<uint32_t>(std::stoul(name));
            Event e = trace_.getEvent(eid);

            if (value.bool_value() == 1) continue;

            if (firstInfeasibleEventInThread[e.getThreadId()] == 0)
                firstInfeasibleEventInThread[e.getThreadId()] = eid;

            if (!Event::isNullEvent(
                    trace_.getThread(e.getThreadId()).getPrevAcq(e)))
                eid = trace_.getThread(e.getThreadId())
                          .getPrevAcq(e)
                          .getEventId();

            firstInfeasibleEventInThread[e.getThreadId()] =
                std::min(eid, firstInfeasibleEventInThread[e.getThreadId()]);
        }

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

    if (log_readable_witness_)
        log_file_ << "Witness for: e" << e1.getEventId() << " - e"
                  << e2.getEventId() << "\n";

    int j = 1;
    std::vector<uint32_t> witness;
    for (const auto& [name, order] : event_order) {
        uint32_t eid = static_cast<uint32_t>(std::stoul(name));
        Event e = trace_.getEvent(eid);

        assert(firstInfeasibleEventInThread.find(e.getThreadId()) !=
               firstInfeasibleEventInThread.end());

        if (firstInfeasibleEventInThread[e.getThreadId()] != 0 &&
            firstInfeasibleEventInThread[e.getThreadId()] <= eid)
            continue;

        if (order > e1Idx || order > e2Idx) break;
        if (eid == e1.getEventId() || eid == e2.getEventId()) continue;

        if (log_binary_witness_) witness.push_back(eid);

        if (log_readable_witness_)
            log_file_ << j++ << ": e" << name << " - "
                      << events[eid - 1].prettyString() << "\n";
    }

    if (e1Idx < e2Idx) {
        if (log_binary_witness_) {
            witness.push_back(e1.getEventId());
            witness.push_back(e2.getEventId());
        }

        if (log_readable_witness_) {
            log_file_ << j++ << ": e" << e1.getEventId() << " - "
                      << e1.prettyString() << "\n";
            log_file_ << j++ << ": e" << e2.getEventId() << " - "
                      << e2.prettyString() << "\n";
        }
    } else {
        if (log_binary_witness_) {
            witness.push_back(e2.getEventId());
            witness.push_back(e1.getEventId());
        }

        if (log_readable_witness_) {
            log_file_ << j++ << ": e" << e2.getEventId() << " - "
                      << e2.prettyString() << "\n";
            log_file_ << j++ << ": e" << e1.getEventId() << " - "
                      << e1.prettyString() << "\n";
        }
    }

    if (log_binary_witness_) 
        writeBinaryWitness(witness);

    if (log_readable_witness_)
        log_file_ << "------------------------------------------------------\n";
}

void ModelLogger::writeBinaryWitness(const std::vector<uint32_t>& witness) {
    if (!binary_log_file_.is_open())
        throw std::runtime_error("Binary witness file not open");
        
    uint32_t witness_size = static_cast<uint32_t>(witness.size());
    binary_log_file_.write(reinterpret_cast<const char*>(&witness_size), sizeof(uint32_t));
    binary_log_file_.write(reinterpret_cast<const char*>(witness.data()), witness_size * sizeof(uint32_t));
}

std::vector<std::vector<uint32_t>> ModelLogger::readBinaryWitness(
    const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open binary witness file");

    std::vector<std::vector<uint32_t>> witnesses;

    while (true) {
        uint32_t witness_size;
        
        if (!file.read(reinterpret_cast<char*>(&witness_size), sizeof(uint32_t)))
            break;

        std::vector<uint32_t> witness(witness_size);

        if (!file.read(reinterpret_cast<char*>(witness.data()), witness_size * sizeof(uint32_t)))
            throw std::runtime_error("File is corrupted or incomplete.");

        witnesses.push_back(std::move(witness));
    }

    return witnesses;
}