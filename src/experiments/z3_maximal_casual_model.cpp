#include <z3++.h>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "event.cpp"
#include "model.cpp"
#include "model_logger.cpp"
#include "trace.cpp"

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

class Z3V2Model : public Model {
   private:
    Trace& trace;
    ModelLogger& logger;

    bool logWitness;

    z3::context c;
    z3::solver s;
    z3::expr_vector varMap;
    z3::expr_vector mhbs;
    z3::expr_vector lockConstraints;
    z3::expr_vector readCFConstraints;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const((std::to_string(e.getEventId())).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const((std::to_string(eid)).c_str());
    }

   public:
    Z3V2Model(Trace& trace_, ModelLogger& logger_, bool logWitness = false)
        : trace(trace_),
          logger(logger_),
          logWitness(logWitness),
          c(),
          s(c, "QF_IDL"),
          varMap(c),
          mhbs(c),
          lockConstraints(c),
          readCFConstraints(c) {
        z3::params p(c);
        p.set("auto_config", false);
        p.set("smt.arith.solver", (unsigned)1);
        s.set(p);
        generateVarMap();
        generateMHBConstraints();
        generateLockConstraints();
        generateReadCFConstraints();
    }

    void generateVarMap() {
        for (const auto& event : trace.getAllEvents()) {
            varMap.push_back(getZ3ExprFromEvent(event, c));
        }
    }

    void generateMHBConstraints() {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                if (i == 0) {
                    // mhbs.push_back(varMap[events[i].getEventId() - 1] > 0);
                } else {
                    mhbs.push_back(varMap[events[i - 1].getEventId() - 1] <
                                   varMap[events[i].getEventId() - 1]);
                }
            }
        }

        for (const auto [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            mhbs.push_back(varMap[forkEvent - 1] < varMap[beginEvent - 1]);
        }

        for (const auto [endEvent, joinEvent] : trace.getJoinEndPairs()) {
            mhbs.push_back(varMap[endEvent - 1] < varMap[joinEvent - 1]);
        }
    }

    void generateLockConstraints() {
        for (const auto& [lockId, acq_rel_pairs] : trace.getLockPairs()) {
            z3::expr_vector lockC(c);
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
                    lockConstraints.push_back((rel1_lt_acq2 || rel2_lt_acq1));
                }
            }
        }
    }

    void generateReadCFConstraints() {
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
                    if (tmp.size() > 0) r_cf.push_back(z3::mk_and(tmp));
                }

                if (same_initial_val) {
                    z3::expr_vector tmp(c);
                    for (Event& ifw : inFeasibleWrites) {
                        tmp.push_back(varMap[r.getEventId() - 1] <
                                      varMap[ifw.getEventId() - 1]);
                    }
                    if (tmp.size() > 0) r_cf.push_back(z3::mk_and(tmp));
                }

                if (r_cf.size() > 0)
                    readCFConstraints.push_back(z3::mk_or(r_cf));
            }
        }
    }

    uint32_t solveForRace() {
        uint32_t race_count = 0;
        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);
        for (const auto& [e1, e2] : trace.getCOPEvents()) {
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            z3::expr_vector test(c);
            test.push_back((e1_expr == e2_expr || e1_expr - e2_expr == 1 || e2_expr - e1_expr == 1));

            if (s.check(test) == z3::sat) race_count++;
        }
        return race_count;
    }

    uint32_t solveForRace(uint32_t maxCOP) {
        uint32_t race_count = 0;

        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);
        for (int i = 0; i < maxCOP; ++i) {
            if (i > trace.getCOPEvents().size()) break;
            auto [e1, e2] = trace.getCOPEvents()[i];
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            z3::expr_vector test(c);
            test.push_back((e1_expr == e2_expr || e1_expr - e2_expr == 1 || e2_expr - e1_expr == 1));

            if (s.check(test) == z3::sat) race_count++;
        }
        return race_count;
    }

    void getStatistics() {}
};

class Z3MaximalCasualModel : public Model {
   private:
    Trace& trace;
    ModelLogger& logger;

    bool logWitness;

    z3::context c;
    z3::expr_vector varMap;
    z3::expr_vector mhbs;
    z3::expr_vector lockConstraints;
    z3::expr_vector readCFConstraints;

    // For debugging
    uint64_t totalFeasibleWrites = 0;
    uint64_t totalInFeasibleWrites = 0;
    uint64_t totalReads = 0;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const((std::to_string(e.getEventId())).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const((std::to_string(eid)).c_str());
    }

   public:
    Z3MaximalCasualModel(Trace& trace_, ModelLogger& logger_,
                         bool logWitness = false)
        : trace(trace_),
          logger(logger_),
          logWitness(logWitness),
          c(),
          varMap(c),
          mhbs(c),
          lockConstraints(c),
          readCFConstraints(c) {
        generateVarMap();
        generateMHBConstraints();
        generateLockConstraints();
        generateReadCFConstraints();
    }

    void generateVarMap() {
        for (const auto& event : trace.getAllEvents()) {
            varMap.push_back(getZ3ExprFromEvent(event, c));
        }
    }

    void generateMHBConstraints() {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                if (i == 0) {
                    // mhbs.push_back(varMap[events[i].getEventId() - 1] > 0);
                } else {
                    mhbs.push_back(varMap[events[i - 1].getEventId() - 1] <
                                   varMap[events[i].getEventId() - 1]);
                }
            }
        }

        for (const auto [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            mhbs.push_back(varMap[forkEvent - 1] < varMap[beginEvent - 1]);
        }

        for (const auto [endEvent, joinEvent] : trace.getJoinEndPairs()) {
            mhbs.push_back(varMap[endEvent - 1] < varMap[joinEvent - 1]);
        }
    }

    void generateLockConstraints() {
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
                    lockConstraints.push_back((rel1_lt_acq2 || rel2_lt_acq1));
                }
            }
        }
    }

    void generateReadCFConstraints() {
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

                if (r_cf.size() > 0)
                    readCFConstraints.push_back(z3::mk_or(r_cf));
            }
        }
    }

    uint32_t solveForRace() {
        uint32_t race_count = 0;
        z3::solver s(c);
        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);

        std::cout << "No of COP events: " << trace.getCOPEvents().size() << "\n";

        for (int i = 0; i < trace.getCOPEvents().size(); ++i) {
            auto cop = trace.getCOPEvents()[i];

            z3::expr e1 = varMap[cop.first.getEventId() - 1];
            z3::expr e2 = varMap[cop.second.getEventId() - 1];

            z3::expr_vector test(c);

            test.push_back((e1 - e2 == 1) || (e2 - e1 == 1 || e1 == e2));

            if (s.check(test) == z3::sat) {
                if (logWitness) {
                    z3::model m = s.get_model();
                    logger.logWitnessPrefix(m, cop.first, cop.second);
                }

                race_count++;
            }
        }
        return race_count;
    }

    uint32_t solveForRace(uint32_t maxCOP) {
        uint32_t race_count = 0;
        z3::solver s(c);
        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);
        for (int i = 0; i < maxCOP; ++i) {
            if (i > trace.getCOPEvents().size()) break;
            auto [e1, e2] = trace.getCOPEvents()[i];
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            z3::expr_vector test(c);

            test.push_back((e1_expr - e2_expr == 1) || (e2_expr - e1_expr == 1 || e1_expr == e2_expr));
            if (s.check(test) == z3::sat) {
                race_count++;
            }
        }
        return race_count;
    }

    void getStatistics() {
        DEBUG_PRINT("No of COP events: " << trace.getCOPEvents().size());
        for (int i = 0; i < 1; ++i) {
            if (i > trace.getCOPEvents().size()) break;
            auto [e1, e2] = trace.getCOPEvents()[i];
            z3::solver s(c);

            auto start = std::chrono::high_resolution_clock::now();

            s.add(mhbs);
            auto end = std::chrono::high_resolution_clock::now();
            DEBUG_PRINT(
                "added MHB constraints, took: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                         start)
                       .count()
                << "ms");

            s.add(lockConstraints);
            end = std::chrono::high_resolution_clock::now();
            DEBUG_PRINT(
                "added lock constraints, took: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                         start)
                       .count()
                << "ms");

            s.add(readCFConstraints);
            end = std::chrono::high_resolution_clock::now();
            DEBUG_PRINT(
                "added read cf constraints, took: "
                << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                         start)
                       .count()
                << "ms");
        }
    }
};
