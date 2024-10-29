#include <z3++.h>

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "event.cpp"
#include "lock_set_engine.cpp"
#include "model.cpp"
#include "model_logger.cpp"
#include "trace.cpp"
#include "transitive_closure.cpp"

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

class Z3V3Model : public Model {
   private:
    Trace& trace;
    ModelLogger& logger;

    TransitiveClosure mhbClosure;

    bool logWitness;

    LockSetEngine lockSetEngine;
    std::vector<std::pair<Event, Event>> filteredCopEvents;

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
    Z3V3Model(Trace& trace_, ModelLogger& logger_, bool logWitness = false)
        : trace(trace_),
          logger(logger_),
          logWitness(logWitness),
          c(),
          s(c, "QF_IDL"),
          varMap(c),
          mhbs(c),
          lockConstraints(c),
          readCFConstraints(c),
          lockSetEngine(trace.getLockRegions()) {
        z3::params p(c);
        p.set("auto_config", false);
        p.set("smt.arith.solver", (unsigned)1);
        s.set(p);
        generateVarMap();
        generateMHBConstraints();
        generateLockConstraints();
        generateReadCFConstraints();
        filterCop();
    }

    void generateVarMap() {
        for (const auto& event : trace.getAllEvents()) {
            varMap.push_back(getZ3ExprFromEvent(event, c));
        }
    }

    void generateMHBConstraints() {
        TransitiveClosure::Builder builder(trace.getAllEvents().size());
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                if (i == 0) {
                    // mhbs.push_back(varMap[events[i].getEventId() - 1] > 0);
                    builder.createNewGroup(events[i].getEventId());
                    continue;
                };
                Event e1 = events[i - 1];
                Event e2 = events[i];
                mhbs.push_back(varMap[e1.getEventId() - 1] <
                               varMap[e2.getEventId() - 1]);

                if (e1.getEventType() == Event::EventType::Fork ||
                    e2.getEventType() == Event::EventType::Join) {
                    builder.createNewGroup(e2.getEventId());
                    builder.addRelation(e1.getEventId(), e2.getEventId());
                } else {
                    builder.addToGroup(e2.getEventId(), e1.getEventId());
                }
            }
        }

        for (const auto [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            mhbs.push_back(varMap[forkEvent - 1] < varMap[beginEvent - 1]);
            builder.addRelation(forkEvent, beginEvent);
        }

        for (const auto [endEvent, joinEvent] : trace.getJoinEndPairs()) {
            mhbs.push_back(varMap[endEvent - 1] < varMap[joinEvent - 1]);
            builder.addRelation(endEvent, joinEvent);
        }

        mhbClosure = builder.build();
    }

    void generateLockConstraints() {
        for (const auto& [lockId, acq_rel_pairs] : trace.getLockPairs()) {
            for (int i = 0; i < acq_rel_pairs.size(); ++i) {
                for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                    if (acq_rel_pairs[i].first.getThreadId() ==
                        acq_rel_pairs[j].first.getThreadId())
                        continue;
                    if (mhbClosure.happensBefore(
                            acq_rel_pairs[i].first.getEventId(),
                            acq_rel_pairs[j].first.getEventId()) ||
                        mhbClosure.happensBefore(
                            acq_rel_pairs[j].first.getEventId(),
                            acq_rel_pairs[i].first.getEventId()))
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
        uint32_t readCount = 0;
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

                Event sameThreadPrevWrite;
                bool hasSameThreadPrevWrite = false;

                for (Event& e : trace.getWriteEvents(r.getVarId())) {
                    if (e.getThreadId() == r.getThreadId() &&
                        e.getEventId() > r.getEventId())
                        continue;

                    if (e.getThreadId() == r.getThreadId()) {
                        sameThreadPrevWrite = e;
                        hasSameThreadPrevWrite = true;
                        continue;
                    }

                    if (e.getVarValue() == r.getVarValue() &&
                        !mhbClosure.happensBefore(r.getEventId(),
                                                  e.getEventId())) {
                        feasibleWrites.push_back(e);
                    } else if (!mhbClosure.happensBefore(r.getEventId(),
                                                         e.getEventId())) {
                        inFeasibleWrites.push_back(e);
                    }
                }

                feasibleWrites.erase(
                    std::remove_if(
                        feasibleWrites.begin(), feasibleWrites.end(),
                        [&feasibleWrites, r, this](Event w) {
                            return std::any_of(
                                feasibleWrites.begin(), feasibleWrites.end(),
                                [w, r, this](Event w2) {
                                    return this->mhbClosure.happensBefore(
                                               w.getEventId(),
                                               w2.getEventId()) &&
                                           this->mhbClosure.happensBefore(
                                               w2.getEventId(), r.getEventId());
                                });
                        }),
                    feasibleWrites.end());

                z3::expr_vector tmp_or(c);

                if (hasSameThreadPrevWrite) {
                    /* has a write event on same thread */

                    if (sameThreadPrevWrite.getVarValue() == r.getVarValue()) {
                        /* case 1.1: the value read is same */

                        z3::expr_vector tmp_and(c);
                        for (Event& ifw : inFeasibleWrites) {
                            if (mhbClosure.happensBefore(
                                    ifw.getEventId(),
                                    sameThreadPrevWrite.getEventId()))
                                continue;
                            
                            /* ifw < fw || r < ifw */
                            tmp_and.push_back(
                                (varMap[ifw.getEventId() - 1] <
                                 varMap[sameThreadPrevWrite.getEventId() -
                                        1]) ||
                                (varMap[r.getEventId() - 1] <
                                 varMap[ifw.getEventId() - 1]));
                        }

                        if (tmp_and.size() > 0)
                            tmp_or.push_back(z3::mk_and(tmp_and));

                        for (Event& fw : feasibleWrites) {
                            if (mhbClosure.happensBefore(
                                    fw.getEventId(),
                                    sameThreadPrevWrite.getEventId()))
                                continue;
                            z3::expr_vector tmp_and2(c);

                            /* fw < r */
                            tmp_and2.push_back(varMap[fw.getEventId() - 1] <
                                               varMap[r.getEventId() - 1]);
                            for (Event& ifw : inFeasibleWrites) {
                                if (mhbClosure.happensBefore(ifw.getEventId(),
                                                             fw.getEventId()) ||
                                    mhbClosure.happensBefore(
                                        ifw.getEventId(),
                                        sameThreadPrevWrite.getEventId()))
                                    continue;
                                
                                /* My own optimization need to check for
                                 * validity */
                                if (fw.getThreadId() == ifw.getThreadId() &&
                                    fw.getEventId() > ifw.getEventId())
                                    continue;

                                /* ifw < fw || r < ifw */
                                tmp_and2.push_back(
                                    (varMap[ifw.getEventId() - 1] <
                                     varMap[fw.getEventId() - 1]) ||
                                    (varMap[r.getEventId() - 1] <
                                     varMap[ifw.getEventId() - 1]));
                            }
                            if (tmp_and2.size() > 0)
                                tmp_or.push_back(z3::mk_and(tmp_and2));
                        }

                        if (tmp_or.size() > 0)
                            readCFConstraints.push_back(z3::mk_or(tmp_or));
                    } else {
                        /* case 1.2: the value read is different */
                        for (Event& fw : feasibleWrites) {
                            if (mhbClosure.happensBefore(
                                    fw.getEventId(),
                                    sameThreadPrevWrite.getEventId()))
                                continue;
                            z3::expr_vector tmp_and2(c);

                            /* fw < r */
                            tmp_and2.push_back(varMap[fw.getEventId() - 1] <
                                               varMap[r.getEventId() - 1]);

                            /* sameThreadPrevWrite < fw */
                            tmp_and2.push_back(varMap[sameThreadPrevWrite.getEventId() - 1] <
                                               varMap[fw.getEventId() - 1]);
                            
                            for (Event& ifw : inFeasibleWrites) {
                                if (mhbClosure.happensBefore(ifw.getEventId(),
                                                             fw.getEventId()) ||
                                    mhbClosure.happensBefore(
                                        ifw.getEventId(),
                                        sameThreadPrevWrite.getEventId()))
                                    continue;
                                /* My own optimization need to check for
                                 * validity */
                                if (fw.getThreadId() == ifw.getThreadId() &&
                                    fw.getEventId() > ifw.getEventId())
                                    continue;
                                
                                /* ifw < fw || r < ifw */
                                tmp_and2.push_back(
                                    (varMap[ifw.getEventId() - 1] <
                                     varMap[fw.getEventId() - 1]) ||
                                    (varMap[r.getEventId() - 1] <
                                     varMap[ifw.getEventId() - 1]));
                            }
                            if (tmp_and2.size() > 0)
                                tmp_or.push_back(z3::mk_and(tmp_and2));
                        }

                        if (tmp_or.size() > 0)
                            readCFConstraints.push_back(z3::mk_or(tmp_or));
                    }
                } else {
                    /* no write on the same var in the same thread */
                    Event sameThreadPrevRead;
                    bool hasSameThreadPrevRead = false;
                    for (Event& e : trace.getReadEvents(r.getVarId())) {
                        if (e.getThreadId() == r.getThreadId() &&
                            e.getEventId() > r.getEventId())
                            break;
                        if (e.getThreadId() == r.getThreadId()) {
                            sameThreadPrevRead = e;
                            hasSameThreadPrevRead = true;
                        }
                    }

                    if (hasSameThreadPrevRead &&
                        sameThreadPrevRead.getVarValue() != r.getVarValue()) {
                        /* case 2.1: sameThreadPrevRead that read a diff value is present
                         */
                        z3::expr_vector tmp_or(c);
                        for (Event& fw : feasibleWrites) {
                            if (mhbClosure.happensBefore(
                                    fw.getEventId(),
                                    sameThreadPrevRead.getEventId()))
                                continue;
                            z3::expr_vector tmp_and(c);

                            /* fw < r */
                            tmp_and.push_back(varMap[fw.getEventId() - 1] <
                                              varMap[r.getEventId() - 1]);

                            /* sameThreadPrevRead < fw */
                            tmp_and.push_back(varMap[sameThreadPrevRead.getEventId() - 1] <
                                              varMap[fw.getEventId() - 1]);
                            
                            for (Event& ifw : inFeasibleWrites) {
                                if (mhbClosure.happensBefore(ifw.getEventId(),
                                                             fw.getEventId()) ||
                                    mhbClosure.happensBefore(
                                        ifw.getEventId(),
                                        sameThreadPrevRead.getEventId()))
                                    continue;
                                /* My own optimization need to check for
                                 * validity */
                                if (fw.getThreadId() == ifw.getThreadId() &&
                                    fw.getEventId() > ifw.getEventId())
                                    continue;

                                /* ifw < fw || r < ifw */
                                tmp_and.push_back(
                                    (varMap[ifw.getEventId() - 1] <
                                     varMap[fw.getEventId() - 1]) ||
                                    (varMap[r.getEventId() - 1] <
                                     varMap[ifw.getEventId() - 1]));
                            }
                            if (tmp_and.size() > 0)
                                tmp_or.push_back(z3::mk_and(tmp_and));
                        }
                        if (tmp_or.size() > 0)
                            readCFConstraints.push_back(z3::mk_or(tmp_or));
                    } else {
                        /* case 2.2.1: sameThreadPrevRead is not present or read the same
                         * value */
                        if (same_initial_val) {
                            readCount++;
                            z3::expr_vector tmp(c);
                            for (Event& ifw : inFeasibleWrites) {
                                if (mhbClosure.happensBefore(ifw.getEventId(),
                                                             r.getEventId()))
                                    continue;
                                tmp.push_back(varMap[r.getEventId() - 1] <
                                              varMap[ifw.getEventId() - 1]);
                            }
                            if (tmp.size() > 0)
                                readCFConstraints.push_back(z3::mk_and(tmp));
                        } else {
                            /* case 2.2.2: TODO */
                            z3::expr_vector tmp_or(c);
                            for (Event& fw : feasibleWrites) {
                                z3::expr_vector tmp_and(c);

                                /* fw < r */
                                tmp_and.push_back(varMap[fw.getEventId() - 1] <
                                                  varMap[r.getEventId() - 1]);
                                for (Event& ifw : inFeasibleWrites) {
                                    if (mhbClosure.happensBefore(
                                            ifw.getEventId(), fw.getEventId()))
                                        continue;
                                    /* My own optimization need to check for
                                     * validity */
                                    if (fw.getThreadId() == ifw.getThreadId() &&
                                        fw.getEventId() > ifw.getEventId())
                                        continue;
                                    
                                    /* ifw < fw || r < ifw */
                                    tmp_and.push_back(
                                        (varMap[ifw.getEventId() - 1] <
                                         varMap[fw.getEventId() - 1]) ||
                                        (varMap[r.getEventId() - 1] <
                                         varMap[ifw.getEventId() - 1]));
                                }
                                if (tmp_and.size() > 0)
                                    tmp_or.push_back(z3::mk_and(tmp_and));
                            }
                            if (tmp_or.size() > 0)
                                readCFConstraints.push_back(z3::mk_or(tmp_or));
                        }
                    }
                }
            }
        }
        DEBUG_PRINT("Read count: " << readCount);
    }

    void filterCop() {
        for (const auto& [e1, e2] : trace.getCOPEvents()) {
            if (mhbClosure.happensBefore(e1.getEventId(), e2.getEventId()) ||
                mhbClosure.happensBefore(e2.getEventId(), e1.getEventId()))
                continue;
            if (lockSetEngine.hasCommonLock(e1, e2)) continue;
            filteredCopEvents.push_back({e1, e2});
        }
    }

    uint32_t solveForRace() {
        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);
        uint32_t race_count = 0;
        DEBUG_PRINT("No of COP events: " << filteredCopEvents.size());
        for (const auto& [e1, e2] : filteredCopEvents) {
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            z3::expr_vector test(c);
            // test.push_back((e1_expr == e2_expr || e1_expr - e2_expr == 1 ||
                            // e2_expr - e1_expr == 1));
            test.push_back(e1_expr == e2_expr);
            if (s.check(test) == z3::sat) {
                if (logWitness) {
                    auto model = s.get_model();
                    logger.logWitness(model, e1, e2);
                }
                race_count++;
            }
        }
        return race_count;
    }

    uint32_t solveForRace(uint32_t maxNoOfCOP) {
        s.add(mhbs);
        s.add(lockConstraints);
        s.add(readCFConstraints);
        // s.set("timeout", static_cast<unsigned>(120000));
        uint32_t race_count = 0;
        DEBUG_PRINT("No of COP events: " << filteredCopEvents.size());
        for (int i = 0; i < maxNoOfCOP; ++i) {
            if (i > filteredCopEvents.size()) break;
            auto [e1, e2] = filteredCopEvents[i];
            z3::expr e1_expr = varMap[e1.getEventId() - 1];
            z3::expr e2_expr = varMap[e2.getEventId() - 1];

            z3::expr_vector test(c);
            // test.push_back((e1_expr == e2_expr || e1_expr - e2_expr == 1 ||
            //                 e2_expr - e1_expr == 1));
            test.push_back(e1_expr == e2_expr);
            if (s.check(test) == z3::sat) race_count++;
        }
        return race_count;
    }

    void getStatistics() {
        DEBUG_PRINT("No of lock constraints: " << lockConstraints.size());
    }
};