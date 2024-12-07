#include <z3++.h>

#include <memory>
#include <utility>
#include <vector>

#include "event.cpp"
#include "expr.cpp"
#include "model.cpp"
#include "trace.cpp"

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

enum ConstraintStatus { Possible, NotPossible, Empty };

class CustomMaximalCasualModel : public Model {
   private:
    Trace& trace;

    std::shared_ptr<Expr> mhbsExpr;
    std::shared_ptr<Expr> lockExpr;
    std::shared_ptr<Expr> cfExpr;

    std::vector<std::shared_ptr<Expr>> mhbs;
    std::vector<std::shared_ptr<Expr>> allEvents;
    std::vector<std::shared_ptr<Expr>> lock_constraints;
    std::vector<std::shared_ptr<Expr>> read_cf_constraints;

    std::unordered_map<uint32_t, bool> generating_cf;
    std::unordered_map<uint32_t, std::shared_ptr<Expr>> cf_constraints;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const(std::to_string(e.getEventId()).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const(std::to_string(eid).c_str());
    }

   public:
    CustomMaximalCasualModel(Trace& trace_)
        : mhbs(), allEvents(), lock_constraints(), trace(trace_) {}

    void generateMHBConstraints() {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                std::shared_ptr<Expr> eventExpr =
                    ExprBuilder::var(events[i].getEventId());
                std::shared_ptr<Expr> zeroExpr = ExprBuilder::intVal(0);
                allEvents.push_back(eventExpr);
                if (i == 0) {
                    mhbs.push_back(ExprBuilder::lessThan(zeroExpr, eventExpr));
                } else {
                    std::shared_ptr<Expr> prevEventExpr =
                        ExprBuilder::var(events[i - 1].getEventId());
                    mhbs.push_back(
                        ExprBuilder::lessThan(prevEventExpr, eventExpr));
                }
            }
        }

        for (const auto& [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            if (forkEvent == 0) {
                std::cout << "got 0 fork event for begin event " << beginEvent
                          << "\n";
            } else if (beginEvent == 0) {
                std::cout << "got 0 begin event\n";
            }
            std::shared_ptr<Expr> forkEventExpr = ExprBuilder::var(forkEvent);
            std::shared_ptr<Expr> beginEventExpr = ExprBuilder::var(beginEvent);
            mhbs.push_back(
                ExprBuilder::lessThan(forkEventExpr, beginEventExpr));
        }

        for (const auto& [joinEvent, endEvent] : trace.getJoinEndPairs()) {
            if (joinEvent == 0) {
                std::cout << "got 0 join event\n";
            } else if (endEvent == 0) {
                std::cout << "got 0 end event\n";
            }
            std::shared_ptr<Expr> joinEventExpr = ExprBuilder::var(joinEvent);
            std::shared_ptr<Expr> endEventExpr = ExprBuilder::var(endEvent);
            mhbs.push_back(ExprBuilder::lessThan(joinEventExpr, endEventExpr));
        }

        if (mhbs.size() == 0) return;

        mhbsExpr = mhbs[0];
        for (int i = 1; i < mhbs.size(); ++i) {
            mhbsExpr = ExprBuilder::andExpr(mhbsExpr, mhbs[i]);
        }
    }

    void generateLockConstraints() {
        for (const auto& [lockId, acq_rel_pairs] : trace.getLockPairs()) {
            for (int i = 0; i < acq_rel_pairs.size(); ++i) {
                std::shared_ptr<Expr> acq1 =
                    ExprBuilder::var(acq_rel_pairs[i].first.getEventId());
                std::shared_ptr<Expr> rel1 =
                    ExprBuilder::var(acq_rel_pairs[i].second.getEventId());
                for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                    if (acq_rel_pairs[i].first.getThreadId() ==
                        acq_rel_pairs[j].first.getThreadId())
                        continue;
                    std::shared_ptr<Expr> acq2 =
                        ExprBuilder::var(acq_rel_pairs[j].first.getEventId());
                    std::shared_ptr<Expr> rel2 =
                        ExprBuilder::var(acq_rel_pairs[j].second.getEventId());
                    lock_constraints.push_back(
                        ExprBuilder::orExpr(ExprBuilder::lessThan(rel1, acq2),
                                            ExprBuilder::lessThan(rel2, acq1)));
                }
            }
        }

        if (lock_constraints.size() == 0) return;

        lockExpr = lock_constraints[0];
        for (int i = 1; i < lock_constraints.size(); ++i) {
            lockExpr = ExprBuilder::andExpr(lockExpr, lock_constraints[i]);
        }
    }

    void generateWriteFeasibilityConstraints(Event w) {
        /*
            get all read events from same thread as write r and with event id
           less than r generate cf constraints for each read event
        */
        if (cf_constraints.find(w.getEventId()) != cf_constraints.end()) return;

        cf_constraints[w.getEventId()] = nullptr;
        std::shared_ptr<Expr> exp = nullptr;

        for (Event& e : trace.getEventsFromThread(w.getThreadId())) {
            if (e.getEventId() > w.getEventId()) break;

            if (e.getEventType() != Event::EventType::Read) continue;

            if (cf_constraints.find(e.getEventId()) == cf_constraints.end()) {
                generateReadFeasibilityConstraints(e);
            }

            if (cf_constraints[e.getEventId()] == nullptr) {
                continue;
            }

            if (exp == nullptr) {
                exp = cf_constraints[e.getEventId()];
            } else {
                exp = ExprBuilder::orExpr(exp, cf_constraints[e.getEventId()]);
            }
        }

        cf_constraints[w.getEventId()] = exp;

        return;
    }

    void generateReadFeasibilityConstraints(Event r) {
        /*
            get all write events for the read r var
            if w if a feasible write generate the cf constraints for it
            add (w < r && (w' < w || r < w') && cf(w)) if cf(w) is possible
        */
        if (cf_constraints.find(r.getEventId()) != cf_constraints.end()) return;

        cf_constraints[r.getEventId()] = nullptr;

        std::vector<Event> feasibleWrites;
        std::vector<Event> inFeasibleWrites;

        std::shared_ptr<Expr> exp = nullptr;
        std::shared_ptr<Expr> r_expr = ExprBuilder::var(r.getEventId());

        bool same_initial_val =
            r.getVarValue() ==
            trace.getReadEvents(r.getVarId())[0].getVarValue();

        for (Event& e : trace.getWriteEvents(r.getVarId())) {
            same_initial_val =
                same_initial_val && (e.getEventId() > r.getEventId());
            if (e.getThreadId() == r.getThreadId() &&
                e.getEventId() > r.getEventId())
                continue;
            if (e.getVarValue() == r.getVarValue()) {
                feasibleWrites.push_back(e);
            } else {
                inFeasibleWrites.push_back(e);
            }
        }

        if (same_initial_val && !feasibleWrites.empty()) {
            std::shared_ptr<Expr> tmp_expr = nullptr;
            for (Event& ifw : inFeasibleWrites) {
                std::shared_ptr<Expr> ifw_expr =
                    ExprBuilder::var(ifw.getEventId());
                if (tmp_expr == nullptr) {
                    tmp_expr = ExprBuilder::lessThan(ifw_expr, r_expr);
                } else {
                    tmp_expr = ExprBuilder::andExpr(
                        tmp_expr, ExprBuilder::lessThan(r_expr, ifw_expr));
                }
            }
            exp = tmp_expr;
        }

        for (Event& fw : feasibleWrites) {
            if (cf_constraints.find(fw.getEventId()) == cf_constraints.end()) {
                generateWriteFeasibilityConstraints(fw);
            }
            std::shared_ptr<Expr> fw_expr = ExprBuilder::var(fw.getEventId());

            std::shared_ptr<Expr> tmp_expr =
                ExprBuilder::lessThan(fw_expr, r_expr);
            for (Event& ifw : inFeasibleWrites) {
                std::shared_ptr<Expr> ifw_expr =
                    ExprBuilder::var(ifw.getEventId());
                tmp_expr = ExprBuilder::andExpr(
                    tmp_expr, ExprBuilder::orExpr(
                                  ExprBuilder::lessThan(ifw_expr, fw_expr),
                                  ExprBuilder::lessThan(r_expr, ifw_expr)));
            }

            if (cf_constraints[fw.getEventId()] != nullptr) {
                tmp_expr = ExprBuilder::andExpr(
                    tmp_expr, cf_constraints[fw.getEventId()]);
            }

            if (exp == nullptr) {
                exp = tmp_expr;
            } else {
                exp = ExprBuilder::orExpr(exp, tmp_expr);
            }
        }

        cf_constraints[r.getEventId()] = exp;

        return;
    }

    std::shared_ptr<Expr> getCFConstraints(Event e) {
        std::shared_ptr<Expr> exp = nullptr;
        for (Event e_p : trace.getEventsFromThread(e.getThreadId())) {
            if (e_p.getEventId() > e.getEventId()) break;
            if (e_p.getEventType() != Event::EventType::Read) continue;
            if (cf_constraints.find(e_p.getEventId()) == cf_constraints.end()) {
                generateReadFeasibilityConstraints(e_p);
            }
            if (cf_constraints.find(e_p.getEventId()) == cf_constraints.end() ||
                cf_constraints[e_p.getEventId()] == nullptr) {
                continue;
            }
            if (exp == nullptr) {
                exp = cf_constraints[e_p.getEventId()];
            } else {
                exp =
                    ExprBuilder::andExpr(exp, cf_constraints[e_p.getEventId()]);
            }
        }
        return exp;
    }

    std::pair<z3::expr, ConstraintStatus> getWriteCFConstraints(
        Event w, z3::context& c) {
        z3::expr_vector res(c);

        for (Event& e : trace.getEventsFromThread(w.getThreadId())) {
            if (e.getEventId() > w.getEventId()) break;
            if (e.getEventType() != Event::EventType::Read) continue;
            if (cf_constraints.find(e.getEventId()) != cf_constraints.end() &&
                generating_cf[e.getEventId()]) {
                return {z3::expr(c), ConstraintStatus::NotPossible};
            }
            auto [cf, valid] = getReadCFConstraints(e, c);
            if (valid == ConstraintStatus::NotPossible) {
                return {z3::expr(c), ConstraintStatus::NotPossible};
            }
            if (valid != ConstraintStatus::Empty) {
                res.push_back(cf);
            }
        }

        // z3::expr w_cf = z3::mk_and(res);
        if (res.size() == 0) {
            return {z3::expr(c), ConstraintStatus::Empty};
        }

        return {z3::mk_and(res), ConstraintStatus::Possible};
    }

    std::pair<z3::expr, ConstraintStatus> getReadCFConstraints(Event r,
                                                               z3::context& c) {
        if (generating_cf.find(r.getEventId()) != generating_cf.end() &&
            generating_cf[r.getEventId()]) {
            return {z3::expr(c), ConstraintStatus::NotPossible};
        }
        generating_cf[r.getEventId()] = true;

        z3::expr_vector res(c);
        std::vector<Event> feasibleWrites;
        std::vector<Event> inFeasibleWrites;

        z3::expr r_expr = getZ3ExprFromEvent(r.getEventId(), c);
        bool same_initial_val =
            r.getVarValue() ==
            trace.getReadEvents(r.getVarId())[0].getVarValue();

        for (Event& e : trace.getWriteEvents(r.getVarId())) {
            same_initial_val =
                same_initial_val && (e.getEventId() > r.getEventId());
            if (e.getThreadId() == r.getThreadId() &&
                e.getEventId() > r.getEventId())
                continue;
            if (e.getVarValue() == r.getVarValue()) {
                feasibleWrites.push_back(e);
            } else {
                inFeasibleWrites.push_back(e);
            }
        }

        if (same_initial_val && !inFeasibleWrites.empty()) {
            z3::expr_vector tmp(c);
            for (Event& ifw : inFeasibleWrites) {
                z3::expr ifw_expr = getZ3ExprFromEvent(ifw.getEventId(), c);
                tmp.push_back((r_expr < ifw_expr));
            }
            res.push_back(z3::mk_and(tmp));
        }

        for (Event& fw : feasibleWrites) {
            auto [fw_cf, valid] = getWriteCFConstraints(fw, c);
            if (valid == ConstraintStatus::NotPossible) {
                continue;
            }
            z3::expr_vector tmp(c);
            z3::expr fw_expr = getZ3ExprFromEvent(fw.getEventId(), c);

            for (Event& ifw : inFeasibleWrites) {
                z3::expr ifw_expr = getZ3ExprFromEvent(ifw.getEventId(), c);
                tmp.push_back((ifw_expr < fw_expr || r_expr < ifw_expr));
            }

            if (valid != ConstraintStatus::Empty) {
                tmp.push_back(fw_cf);
            }

            if (fw.getThreadId() == r.getThreadId()) {
                res.push_back(z3::mk_and(tmp));
            } else {
                res.push_back(fw_expr < r_expr && z3::mk_and(tmp));
            }
        }

        generating_cf[r.getEventId()] = false;

        if (res.size() == 0) {
            return {z3::expr(c), ConstraintStatus::Empty};
        }

        return {z3::mk_or(res), ConstraintStatus::Possible};
    }

    z3::expr getCFConstraints(Event e, z3::context& c) {
        z3::expr_vector res(c);
        generating_cf.clear();
        for (Event e_p : trace.getEventsFromThread(e.getThreadId())) {
            if (e_p.getEventId() > e.getEventId()) break;
            if (e_p.getEventType() != Event::EventType::Read) continue;
            auto [cf, status] = getReadCFConstraints(e_p, c);
            if (status == ConstraintStatus::NotPossible) {
                std::cout << "We have a problem\n";
                continue;
            }
            if (status == ConstraintStatus::Empty) {
                continue;
            }
            res.push_back(cf);
        }
        return z3::mk_and(res);
    }

    void generateAllCFConstraints(z3::context& c, z3::solver& s) {
        for (auto [threadId, events] : trace.getThreadToEventsMap()) {
            for (Event& r : events) {
                if (r.getEventType() != Event::EventType::Read) continue;
                std::vector<Event> feasibleWrites;
                std::vector<Event> inFeasibleWrites;
                bool same_initial_val =
                    r.getVarValue() ==
                    trace.getReadEvents(r.getVarId())[0].getVarValue();
                for (Event& e : trace.getWriteEvents(r.getVarId())) {
                    same_initial_val =
                        same_initial_val && (e.getEventId() > r.getEventId());
                    if (e.getThreadId() == r.getThreadId() &&
                        e.getEventId() > r.getEventId())
                        continue;
                    if (e.getVarValue() == r.getVarValue()) {
                        feasibleWrites.push_back(e);
                    } else {
                        inFeasibleWrites.push_back(e);
                    }
                }
                z3::expr_vector cf(c);
                z3::expr r_expr = getZ3ExprFromEvent(r, c);
                if (same_initial_val && !inFeasibleWrites.empty()) {
                    z3::expr_vector tmp(c);
                    for (Event ifw : inFeasibleWrites) {
                        z3::expr ifw_expr = getZ3ExprFromEvent(ifw, c);
                        tmp.push_back(r_expr < ifw_expr);
                    }
                    cf.push_back(z3::mk_and(tmp));
                }
                for (Event fw : feasibleWrites) {
                    z3::expr_vector tmp(c);
                    z3::expr fw_expr = getZ3ExprFromEvent(fw, c);
                    for (Event ifw : inFeasibleWrites) {
                        z3::expr ifw_expr =
                            getZ3ExprFromEvent(ifw.getEventId(), c);
                        tmp.push_back(ifw_expr < fw_expr || r_expr < ifw_expr);
                    }
                    cf.push_back(fw_expr < r_expr && z3::mk_and(tmp));
                }
                s.add(z3::mk_or(cf));
            }
        }
    }

    void generateReadCFConstraints() {
        for (auto [threadId, events] : trace.getThreadToEventsMap()) {
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

                std::shared_ptr<Expr> exp = nullptr;
                std::shared_ptr<Expr> r_expr = ExprBuilder::var(r.getEventId());

                for (Event& fw : feasibleWrites) {
                    std::shared_ptr<Expr> fw_expr =
                        ExprBuilder::var(fw.getEventId());
                    std::shared_ptr<Expr> tmp_expr =
                        ExprBuilder::lessThan(fw_expr, r_expr);
                    for (Event& ifw : inFeasibleWrites) {
                        std::shared_ptr<Expr> ifw_expr =
                            ExprBuilder::var(ifw.getEventId());
                        tmp_expr = ExprBuilder::andExpr(
                            tmp_expr,
                            ExprBuilder::orExpr(
                                ExprBuilder::lessThan(ifw_expr, fw_expr),
                                ExprBuilder::lessThan(r_expr, ifw_expr)));
                    }
                    if (exp == nullptr) {
                        exp = tmp_expr;
                    } else {
                        exp = ExprBuilder::orExpr(exp, tmp_expr);
                    }
                }

                if (same_initial_val) {
                    std::shared_ptr<Expr> tmp_expr = nullptr;
                    for (Event& ifw : inFeasibleWrites) {
                        std::shared_ptr<Expr> ifw_expr =
                            ExprBuilder::var(ifw.getEventId());
                        if (tmp_expr == nullptr) {
                            tmp_expr = ExprBuilder::lessThan(r_expr, ifw_expr);
                        } else {
                            tmp_expr = ExprBuilder::andExpr(
                                tmp_expr,
                                ExprBuilder::lessThan(r_expr, ifw_expr));
                        }
                    }
                    if (exp == nullptr) {
                        exp = tmp_expr;
                    } else {
                        exp = ExprBuilder::orExpr(exp, tmp_expr);
                    }
                }

                if (exp != nullptr) {
                    read_cf_constraints.push_back(exp);
                }
            }
        }

        if (read_cf_constraints.size() == 0) return;

        cfExpr = read_cf_constraints[0];
        for (int i = 1; i < read_cf_constraints.size(); ++i) {
            cfExpr = ExprBuilder::andExpr(cfExpr, read_cf_constraints[i]);
        }
    }

    uint32_t solveForRace(uint32_t maxNoOfCOP) {
        generateMHBConstraints();
        DEBUG_PRINT("Generated MHB constraints");

        generateLockConstraints();
        DEBUG_PRINT("Generated lock constraints");

        generateReadCFConstraints();
        DEBUG_PRINT("Generated read cf constraints");

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

            // z3::expr_vector allEv(c);
            for (const auto& ex : allEvents) {
                Z3ExprVisitor visitor(c, varMap);
                ex->accept(visitor);
                // allEv.push_back(visitor.getResult());
            }

            // s.add(z3::distinct(allEv));

            if (mhbsExpr != nullptr) {
                Z3ExprVisitor mhbsVisitor(c, varMap);
                mhbsExpr->accept(mhbsVisitor);
                s.add(mhbsVisitor.getResult());
                DEBUG_PRINT("added mhbs constraints");
            }

            if (lockExpr != nullptr) {
                Z3ExprVisitor lockVisitor(c, varMap);
                lockExpr->accept(lockVisitor);
                s.add(lockVisitor.getResult());
                DEBUG_PRINT("added lock constraints");
            }

            if (cfExpr != nullptr) {
                // ConstraintCounter cfCounter;
                // cfExpr->accept(cfCounter);
                // DEBUG_PRINT("no of cf constraints: " <<
                // cfCounter.getCount());

                Z3ExprVisitor cfVisitor(c, varMap);
                cfExpr->accept(cfVisitor);
                s.add(cfVisitor.getResult());
                DEBUG_PRINT("added cf constraints");
            }

            auto cop = trace.getCOPEvents()[i];

            z3::expr e1 = varMap[cop.first.getEventId() - 1];
            z3::expr e2 = varMap[cop.second.getEventId() - 1];

            s.add((e1 - e2 == 1) || (e2 - e1 == 1));

            if (s.check() == z3::sat) {
                race_count++;
            }
        }

        return race_count;
    }

    uint32_t solveForRace() {
        generateMHBConstraints();
        DEBUG_PRINT("Generated MHB constraints");

        generateLockConstraints();
        DEBUG_PRINT("Generated lock constraints");

        generateReadCFConstraints();
        DEBUG_PRINT("Generated read cf constraints");

        uint32_t race_count = 0;

        for (auto cop : trace.getCOPEvents()) {
            z3::context c;
            z3::solver s(c);
            z3::expr_vector varMap(c);
            for (const auto& ev : trace.getAllEvents()) {
                varMap.push_back(getZ3ExprFromEvent(ev, c));
            }

            // z3::expr_vector allEv(c);
            for (const auto& ex : allEvents) {
                Z3ExprVisitor visitor(c, varMap);
                ex->accept(visitor);
                // allEv.push_back(visitor.getResult());
            }

            // s.add(z3::distinct(allEv));

            if (mhbsExpr != nullptr) {
                Z3ExprVisitor mhbsVisitor(c, varMap);
                mhbsExpr->accept(mhbsVisitor);
                s.add(mhbsVisitor.getResult());
                // DEBUG_PRINT("added mhbs constraints");
            }

            if (lockExpr != nullptr) {
                Z3ExprVisitor lockVisitor(c, varMap);
                lockExpr->accept(lockVisitor);
                s.add(lockVisitor.getResult());
                // DEBUG_PRINT("added lock constraints");
            }

            if (cfExpr != nullptr) {
                // ConstraintCounter cfCounter;
                // cfExpr->accept(cfCounter);
                // DEBUG_PRINT("no of cf constraints: " <<
                // cfCounter.getCount());

                Z3ExprVisitor cfVisitor(c, varMap);
                cfExpr->accept(cfVisitor);
                s.add(cfVisitor.getResult());
                // DEBUG_PRINT("added cf constraints");
            }

            z3::expr e1 = varMap[cop.first.getEventId() - 1];
            z3::expr e2 = varMap[cop.second.getEventId() - 1];

            s.add((e1 - e2 == 1) || (e2 - e1 == 1));

            if (s.check() == z3::sat) {
                race_count++;
            }
        }

        return race_count;
    }

    void getStatistics() {}
};