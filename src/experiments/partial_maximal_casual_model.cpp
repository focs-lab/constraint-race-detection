#include <z3++.h>

#include <memory>
#include <utility>
#include <vector>

#include "event.cpp"
#include "model.cpp"
#include "trace.cpp"

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

class PartialMaximalCasualModel : public Model {
   private:
    Trace& trace;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const(std::to_string(e.getEventId()).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const(std::to_string(eid).c_str());
    }

   public:
    PartialMaximalCasualModel(Trace& trace_) : trace(trace_) {}

    void generateMHBConstraints(z3::context& c, z3::solver& s,
                                z3::expr_vector& varMap) {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                if (i == 0) {
                    s.add(varMap[events[i].getEventId() - 1] > 0);
                } else {
                    s.add(varMap[events[i - 1].getEventId() - 1] <
                          varMap[events[i].getEventId() - 1]);
                }
            }
        }

        for (const auto [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            s.add(varMap[forkEvent - 1] < varMap[beginEvent - 1]);
        }

        for (const auto [joinEvent, endEvent] : trace.getJoinEndPairs()) {
            s.add(varMap[joinEvent - 1] < varMap[endEvent - 1]);
        }
    }

    void generateLockConstraints(z3::context& c, z3::solver& s,
                                 z3::expr_vector& varMap) {
        for (const auto& [lockId, acq_rel_pairs] : trace.getLockPairs()) {
            for (int i = 0; i < acq_rel_pairs.size(); ++i) {
                for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                    if (acq_rel_pairs[i].first.getThreadId() ==
                        acq_rel_pairs[j].first.getThreadId())
                        continue;
                    z3::expr rel1_lt_acq2 =
                        varMap[acq_rel_pairs[i].second.getEventId() - 1] <
                        varMap[acq_rel_pairs[j].first.getEventId() - 1];
                    z3::expr rel2_lt_acq1 =
                        varMap[acq_rel_pairs[j].second.getEventId() - 1] <
                        varMap[acq_rel_pairs[i].first.getEventId() - 1];
                    s.add(rel1_lt_acq2 || rel2_lt_acq1);
                }
            }
        }
    }

    void generateReadCFConstraints(z3::context& c, z3::solver& s,
                                   z3::expr_vector& varMap) {
        for (auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (Event& r : events) {
                if (r.getEventType() != Event::EventType::Read) continue;

                std::vector<Event> feasibleWrites;
                std::vector<Event> inFeasibleWrites;

                bool has_write_events =
                    trace.getWriteEvents(r.getVarId()).size() > 0;
                bool same_initial_val =
                    r.getVarValue() ==
                        trace.getReadEvents(r.getVarId())[0].getVarValue() &&
                    (!has_write_events ||
                     trace.getReadEvents(r.getVarId())[0].getEventId() <
                         trace.getWriteEvents(r.getVarId())[0].getEventId());

                for (Event& e : trace.getWriteEvents(r.getVarId())) {
                    if (e.getThreadId() == r.getThreadId() &&
                        e.getEventId() > r.getEventId())
                        continue;
                    if (e.getVarValue() == r.getVarValue()) {
                        feasibleWrites.push_back(e);
                    } else {
                        inFeasibleWrites.push_back(e);
                    }
                }

                z3::expr_vector r_cf(c);

                for (Event& fw : feasibleWrites) {
                    z3::expr_vector tmp(c);
                    tmp.push_back(varMap[fw.getEventId() - 1] <
                                  varMap[r.getEventId() - 1]);
                    for (Event& ifw : inFeasibleWrites) {
                        tmp.push_back(varMap[ifw.getEventId() - 1] <
                                          varMap[fw.getEventId() - 1] ||
                                      varMap[r.getEventId() - 1] <
                                          varMap[ifw.getEventId() - 1]);
                    }
                    r_cf.push_back(z3::mk_and(tmp));
                }

                if (same_initial_val) {
                    z3::expr_vector tmp(c);
                    for (Event& ifw : inFeasibleWrites) {
                        tmp.push_back(varMap[r.getEventId() - 1] <
                                      varMap[ifw.getEventId() - 1]);
                    }
                    if (tmp.size() > 0) r_cf.push_back(z3::mk_and(tmp));
                }

                if (r_cf.size() > 0) s.add(z3::mk_or(r_cf));
            }
        }
    }

    uint32_t solveForRace(uint32_t maxNoOfCOP) {
        DEBUG_PRINT("No of COP events: " << trace.getCOPEvents().size());
        uint32_t race_count = 0;
        for (int i = 0; i < maxNoOfCOP; ++i) {
            if (i >= trace.getCOPEvents().size()) {
                break;
            }

            z3::context c;
            z3::solver s(c);
            z3::expr_vector varMap(c);
            for (const auto& ev : trace.getAllEvents()) {
                varMap.push_back(getZ3ExprFromEvent(ev, c));
            }

            generateMHBConstraints(c, s, varMap);
            generateLockConstraints(c, s, varMap);
            generateReadCFConstraints(c, s, varMap);

            auto [e1, e2] = trace.getCOPEvents()[i];
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            s.add((e1_expr - e2_expr == 1) || (e2_expr - e1_expr == 1));

            if (s.check() == z3::sat) {
                race_count++;
            }
        }

        return race_count;
    }

    uint32_t solveForRace() {
        DEBUG_PRINT("No of COP events: " << trace.getCOPEvents().size());
        uint32_t race_count = 0;
        for (int i = 0; i < trace.getCOPEvents().size(); ++i) {
            z3::context c;
            z3::solver s(c);
            z3::expr_vector varMap(c);
            for (const auto& ev : trace.getAllEvents()) {
                varMap.push_back(getZ3ExprFromEvent(ev, c));
            }

            generateMHBConstraints(c, s, varMap);
            generateLockConstraints(c, s, varMap);
            generateReadCFConstraints(c, s, varMap);

            auto [e1, e2] = trace.getCOPEvents()[i];
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            s.add((e1_expr - e2_expr == 1) || (e2_expr - e1_expr == 1));

            if (s.check() == z3::sat) {
                race_count++;
            }
        }

        return race_count;
    }

    void getStatistics() {}
};