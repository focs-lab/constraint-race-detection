#include <z3++.h>

#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#include "event.cpp"
#include "maximal_casual_model.cpp"
#include "trace.cpp"

z3::expr addWriteCfConstraints(z3::context& c, z3::solver& s,
                               std::vector<Event>& events, Event& r);
z3::expr addReadFromConstraints(z3::context& c, z3::solver& s,
                                std::vector<Event>& events, Event& r);

std::vector<Event> parseEventsFromLog(const std::string& filename) {
    std::ifstream inputFile(filename);
    std::vector<Event> events;

    if (!inputFile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return events;
    }

    std::string line;
    uint32_t event_no = 1;
    while (std::getline(inputFile, line)) {
        try {
            uint64_t rawEvent = std::stoull(line);
            events.push_back(Event(rawEvent, event_no++));
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what()
                      << std::endl;
        }
    }

    inputFile.close();

    return events;
}

void addMHBConstraints(z3::context& c, z3::solver& s,
                       std::vector<Event>& events) {
    z3::expr_vector mhbs(c);
    z3::expr_vector allEvents(c);
    std::unordered_map<uint32_t, std::string> last_event_in_thread;
    uint32_t threadId;

    for (const Event& event : events) {
        threadId = event.getThreadId();
        z3::expr eventExpr =
            c.int_const(std::to_string(event.getEventId()).c_str());
        allEvents.push_back(eventExpr);
        if (last_event_in_thread.find(threadId) == last_event_in_thread.end()) {
            mhbs.push_back(eventExpr > 0);
        } else {
            z3::expr prevEventExpr =
                c.int_const(last_event_in_thread[threadId].c_str());
            mhbs.push_back(eventExpr > prevEventExpr);
        }
        last_event_in_thread[threadId] = std::to_string(event.getEventId());
    }

    s.add(z3::distinct(allEvents));
    s.add(z3::mk_and(mhbs));
}

void addLockConstraints(z3::context& c, z3::solver& s,
                        std::vector<Event>& events) {
    z3::expr_vector lock_constrains(c);
    std::unordered_map<uint32_t, std::vector<std::pair<Event, Event>>> locks;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, Event>>
        last_acquire_event;

    for (const Event& event : events) {
        if (event.getEventType() == Event::EventType::Acquire) {
            last_acquire_event[event.getVarId()][event.getThreadId()] = event;
        } else if (event.getEventType() == Event::EventType::Release) {
            locks[event.getVarId()].push_back(
                {last_acquire_event[event.getVarId()][event.getThreadId()],
                 event});
        }
    }

    for (const auto& [lockId, acq_rel_pairs] : locks) {
        for (int i = 0; i < acq_rel_pairs.size(); ++i) {
            z3::expr acq1 = c.int_const(
                std::to_string(acq_rel_pairs[i].first.getEventId()).c_str());
            z3::expr rel1 = c.int_const(
                std::to_string(acq_rel_pairs[i].second.getEventId()).c_str());
            for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                z3::expr acq2 = c.int_const(
                    std::to_string(acq_rel_pairs[j].first.getEventId())
                        .c_str());
                z3::expr rel2 = c.int_const(
                    std::to_string(acq_rel_pairs[j].second.getEventId())
                        .c_str());
                lock_constrains.push_back((rel1 < acq2 || rel2 < acq1));
            }
        }
    }

    s.add(z3::mk_and(lock_constrains));
}

std::vector<std::pair<Event, Event>> getCOPEvents(std::vector<Event>& events) {
    std::vector<std::pair<Event, Event>> cops;

    for (int i = 0; i < events.size(); ++i) {
        if (events[i].getEventType() != Event::EventType::Read &&
            events[i].getEventType() != Event::EventType::Write)
            continue;
        for (int j = i + 1; j < events.size(); ++j) {
            if (events[j].getThreadId() == events[i].getThreadId()) continue;
            if (events[j].getEventType() != Event::EventType::Read &&
                events[j].getEventType() != Event::EventType::Write)
                continue;
            if (events[i].getVarId() != events[j].getVarId()) continue;
            cops.push_back(std::make_pair(events[i], events[j]));
        }
    }

    return cops;
}

z3::expr addReadFromConstraints(z3::context& c, z3::solver& s,
                                std::vector<Event>& events, Event& r) {
    std::vector<Event> feasibleWrites;
    std::vector<Event> inFeasibleWrites;
    z3::expr_vector res(c);

    uint32_t inital_val = 0;
    bool initial_val_found = false;
    bool initial_op_is_write = false;

    z3::expr r_expr = c.int_const(std::to_string(r.getEventId()).c_str());

    for (Event& e : events) {
        if (e.getVarId() == r.getVarId() && !initial_val_found) {
            inital_val = e.getVarValue();
            initial_val_found = true;
            initial_op_is_write = e.getEventType() == Event::EventType::Write;
        }
        if (e.getEventType() != Event::EventType::Write ||
            e.getVarId() != r.getVarId())
            continue;
        if (e.getThreadId() == r.getThreadId() &&
            e.getEventId() > r.getEventId())
            continue;
        if (e.getVarValue() == r.getVarValue()) {
            feasibleWrites.push_back(e);
        } else {
            inFeasibleWrites.push_back(e);
        }
    }

    if (inital_val == r.getVarValue() && !initial_op_is_write) {
        z3::expr_vector tmp(c);
        for (Event& ifw : inFeasibleWrites) {
            z3::expr ifw_expr =
                c.int_const(std::to_string(ifw.getEventId()).c_str());
            tmp.push_back((r_expr < ifw_expr));
        }
        res.push_back(z3::mk_and(tmp));
    }

    for (Event& fw : feasibleWrites) {
        z3::expr_vector tmp(c);
        z3::expr fw_expr = c.int_const(std::to_string(fw.getEventId()).c_str());
        if (fw.getThreadId() == r.getThreadId() &&
            fw.getEventId() > r.getEventId())
            continue;

        for (Event& ifw : inFeasibleWrites) {
            z3::expr ifw_expr =
                c.int_const(std::to_string(ifw.getEventId()).c_str());
            tmp.push_back((ifw_expr < fw_expr || r_expr < ifw_expr));
        }
        if (fw.getThreadId() == r.getThreadId()) {
            res.push_back(z3::mk_and(tmp));
        } else {
            res.push_back((fw_expr < r_expr && z3::mk_and(tmp)));
        }
    }

    return z3::mk_or(res);
}

void addRaceConstraints(z3::context& c, z3::solver& s,
                        std::pair<Event, Event> cop,
                        std::vector<Event>& events) {
    z3::expr e1 = c.int_const(std::to_string(cop.first.getEventId()).c_str());
    z3::expr e2 = c.int_const(std::to_string(cop.second.getEventId()).c_str());

    for (Event& e : events) {
        if (e.getThreadId() != cop.first.getThreadId() &&
            e.getThreadId() != cop.second.getThreadId())
            continue;
        if (e.getEventType() != Event::EventType::Read) continue;
        if (e.getEventId() > cop.first.getEventId() &&
            e.getEventId() > cop.second.getEventId())
            continue;
        s.add(addReadFromConstraints(c, s, events, e));
    }

    s.add((e1 - e2 == 1) || (e2 - e1 == 1));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Please provide an input file\n";
        return 0;
    }

    std::string filename = argv[1];

    Trace* trace = Trace::fromLog(filename);
    if (trace == nullptr) {
        std::cerr << "Error parsing log file" << std::endl;
        return 1;
    }

    MaximalCasualModel mcm(*trace);

    mcm.generateMHBConstraints();
    mcm.generateLockConstraints();
    mcm.solveForRace();

    // std::vector<Event> events = parseEventsFromLog(filename);

    // z3::context c;
    // z3::solver s(c);

    // addMHBConstraints(c, s, events);
    // addLockConstraints(c, s, events);

    // std::vector<std::pair<Event, Event>> cops = getCOPEvents(events);

    // for (const auto& cop : cops) {
    //     std::cout << "Solving for cop " << cop.first.getEventId() << ", " <<
    //     cop.second.getEventId() << "\n"; z3::context c_tmp; z3::solver
    //     s_tmp(c_tmp); addMHBConstraints(c_tmp, s_tmp, events);
    //     addLockConstraints(c_tmp, s_tmp, events);
    //     addRaceConstraints(c_tmp, s_tmp, cop, events);

    //     std::cout << "constraints are:\n";
    //     std::cout << s_tmp << std::endl;

    //     if (s_tmp.check() == z3::sat) {
    //         z3::model m = s_tmp.get_model();
    //         std::cout << "Race found" << "\n";
    //         std::cout << m << std::endl;
    //     } else {
    //         std::cout << "no race found" << "\n";
    //     }
    //     std::cout <<
    //     "-------------------------------------------------------\n";
    // }

    return 0;
}