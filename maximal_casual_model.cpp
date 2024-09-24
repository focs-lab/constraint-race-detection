#include <z3++.h>

#include <utility>
#include <vector>
#include <memory>

#include "event.cpp"
#include "trace.cpp"
#include "expr.cpp"

class MaximalCasualModel {
   private:
    Trace& trace;

    std::shared_ptr<Expr> mhbsExpr;
    std::shared_ptr<Expr> lockExpr;

    std::vector<std::shared_ptr<Expr>> mhbs;
    std::vector<std::shared_ptr<Expr>> allEvents;
    std::vector<std::shared_ptr<Expr>> lock_constraints;

    std::unordered_map<uint32_t, bool> generating_cf;
    std::unordered_map<uint32_t, std::shared_ptr<Expr>> cf_constraints;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const(std::to_string(e.getEventId()).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const(std::to_string(eid).c_str());
    }

   public:
    MaximalCasualModel(Trace& trace_)
        : mhbs(), allEvents(), lock_constraints(), trace(trace_) {}

    void generateMHBConstraints() {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                std::shared_ptr<Expr> eventExpr = ExprBuilder::var(std::to_string(events[i].getEventId()));
                std::shared_ptr<Expr> zeroExpr = ExprBuilder::intVal(0);
                allEvents.push_back(eventExpr);
                if (i == 0) {
                    mhbs.push_back(ExprBuilder::lessThan(zeroExpr, eventExpr));
                } else {
                    std::shared_ptr<Expr> prevEventExpr = ExprBuilder::var(std::to_string(events[i - 1].getEventId()));
                    mhbs.push_back(ExprBuilder::lessThan(prevEventExpr, eventExpr));
                }
            }
        }

        for (const auto& [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            std::shared_ptr<Expr> forkEventExpr = ExprBuilder::var(std::to_string(forkEvent));
            std::shared_ptr<Expr> beginEventExpr = ExprBuilder::var(std::to_string(beginEvent));
            mhbs.push_back(ExprBuilder::lessThan(forkEventExpr, beginEventExpr));
        }

        for (const auto& [joinEvent, endEvent] : trace.getJoinEndPairs()) {
            std::shared_ptr<Expr> joinEventExpr = ExprBuilder::var(std::to_string(joinEvent));
            std::shared_ptr<Expr> endEventExpr = ExprBuilder::var(std::to_string(endEvent));
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
                std::shared_ptr<Expr> acq1 = ExprBuilder::var(std::to_string(acq_rel_pairs[i].first.getEventId()));
                std::shared_ptr<Expr> rel1 = ExprBuilder::var(std::to_string(acq_rel_pairs[i].second.getEventId()));
                for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                    if (acq_rel_pairs[i].first.getThreadId() ==
                        acq_rel_pairs[j].first.getThreadId())
                        continue;
                    std::shared_ptr<Expr> acq2 = ExprBuilder::var(std::to_string(acq_rel_pairs[j].first.getEventId()));
                    std::shared_ptr<Expr> rel2 = ExprBuilder::var(std::to_string(acq_rel_pairs[j].second.getEventId()));
                    lock_constraints.push_back(ExprBuilder::orExpr(ExprBuilder::lessThan(rel1, acq2), ExprBuilder::lessThan(rel2, acq1)));
                }
            }
        }

        if (lock_constraints.size() == 0) return;

        lockExpr = lock_constraints[0];
        for (int i = 1; i < lock_constraints.size(); ++i) {
            lockExpr = ExprBuilder::andExpr(lockExpr, lock_constraints[i]);
        }
    }

    bool generateWriteFeasibilityConstraints(Event w) {
        /*
            get all read events from same thread as write r and with event id less than r
            generate cf constraints for each read event
        */
        if (cf_constraints.find(w.getEventId()) != cf_constraints.end())
            return !generating_cf[w.getEventId()];

        generating_cf[w.getEventId()] = true;
        cf_constraints[w.getEventId()] = nullptr;
        std::shared_ptr<Expr> exp = nullptr;

        for (Event& e : trace.getEventsFromThread(w.getThreadId())) {
            if (e.getEventId() > w.getEventId())
                break;

            if (e.getEventType() != Event::EventType::Read) continue;

            if (cf_constraints.find(e.getEventId()) != cf_constraints.end()) {
                if (generating_cf[e.getEventId()]) 
                    continue;
            } else {
                if (!generateReadFeasibilityConstraints(e)) 
                    continue;
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
        generating_cf[w.getEventId()] = false;

        return true;
    }

    bool generateReadFeasibilityConstraints(Event r) {
        /*
            get all write events for the read r var
            if w if a feasible write generate the cf constraints for it
            add (w < r && (w' < w || r < w') && cf(w)) if cf(w) is possible
        */
        if (cf_constraints.find(r.getEventId()) != cf_constraints.end())
           return !generating_cf[r.getEventId()];

        generating_cf[r.getEventId()] = true;
        cf_constraints[r.getEventId()] = nullptr;

        std::vector<Event> feasibleWrites;
        std::vector<Event> inFeasibleWrites;

        std::shared_ptr<Expr> exp = nullptr;
        std::shared_ptr<Expr> r_expr = ExprBuilder::var(std::to_string(r.getEventId()));

        bool same_initial_val = r.getVarValue() == trace.getReadEvents(r.getVarId())[0].getVarValue();

        for (Event& e : trace.getWriteEvents(r.getVarId())) {
            same_initial_val = same_initial_val && (e.getEventId() > r.getEventId());
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
                std::shared_ptr<Expr> ifw_expr = ExprBuilder::var(std::to_string(ifw.getEventId()));
                if (tmp_expr == nullptr) {
                    tmp_expr = ExprBuilder::lessThan(ifw_expr, r_expr);
                } else {
                    tmp_expr = ExprBuilder::andExpr(tmp_expr, ExprBuilder::lessThan(ifw_expr, r_expr));
                }
            }
            exp = tmp_expr;
        }

        for (Event& fw : feasibleWrites) {
            if (cf_constraints.find(fw.getEventId()) != cf_constraints.end()) {
                if (generating_cf[fw.getEventId()]) 
                    continue;
            } else {
                if (!generateWriteFeasibilityConstraints(fw)) 
                    continue;
            }
            std::shared_ptr<Expr> fw_expr = ExprBuilder::var(std::to_string(fw.getEventId()));

            std::shared_ptr<Expr> tmp_expr = ExprBuilder::lessThan(fw_expr, r_expr);
            for (Event& ifw : inFeasibleWrites) {
                std::shared_ptr<Expr> ifw_expr = ExprBuilder::var(std::to_string(ifw.getEventId()));
                tmp_expr = ExprBuilder::andExpr(tmp_expr, ExprBuilder::orExpr(ExprBuilder::lessThan(ifw_expr, fw_expr), ExprBuilder::lessThan(r_expr, ifw_expr)));
            }

            if (cf_constraints[fw.getEventId()] != nullptr) {
                tmp_expr = ExprBuilder::andExpr(tmp_expr, cf_constraints[fw.getEventId()]);
            }

            if (exp == nullptr) {
                exp = tmp_expr;
            } else {
                exp = ExprBuilder::orExpr(exp, tmp_expr);
            }
        }

        cf_constraints[r.getEventId()] = exp;
        generating_cf[r.getEventId()] = false;

        return true;
    }

    std::shared_ptr<Expr> getCFConstraints(Event e) {
        std::shared_ptr<Expr> exp = nullptr;
        for (Event e_p : trace.getEventsFromThread(e.getThreadId())) {
            if (e_p.getEventId() > e.getEventId())
                break;
            if (e_p.getEventType() != Event::EventType::Read)
                continue;
            if (cf_constraints.find(e_p.getEventId()) == cf_constraints.end()) {
                generateReadFeasibilityConstraints(e_p);
            }
            if (cf_constraints.find(e_p.getEventId()) == cf_constraints.end() || cf_constraints[e_p.getEventId()] == nullptr) {
                continue;
            }
            if (exp == nullptr) {
                exp = cf_constraints[e_p.getEventId()];
            } else {
                exp = ExprBuilder::andExpr(exp, cf_constraints[e_p.getEventId()]);
            }
        }
        return exp;
    }

    void solveForRace() {
        for (const auto& cop : trace.getCOPEvents()) {
            z3::context c_tmp;
            z3::solver s_tmp(c_tmp);
            z3::expr e1 = getZ3ExprFromEvent(cop.first, c_tmp);
            z3::expr e2 = getZ3ExprFromEvent(cop.second, c_tmp);

            z3::expr_vector allEv(c_tmp);
            for (const auto&ex : allEvents) {
                Z3ExprVisitor visitor(c_tmp);
                ex->accept(visitor);
                allEv.push_back(visitor.getResult());
            }

            s_tmp.add(z3::distinct(allEv));

            Z3ExprVisitor mhbsVisitor(c_tmp);
            mhbsExpr->accept(mhbsVisitor);
            s_tmp.add(mhbsVisitor.getResult());

            if (lockExpr != nullptr) {
                Z3ExprVisitor lockVisitor(c_tmp);
                lockExpr->accept(lockVisitor);
                s_tmp.add(lockVisitor.getResult());
            }

            std::shared_ptr<Expr> cf1 = getCFConstraints(cop.first);
            std::shared_ptr<Expr> cf2 = getCFConstraints(cop.second);

            if (cf1 != nullptr) {
                Z3ExprVisitor cf1Visitor(c_tmp);
                cf1->accept(cf1Visitor);
                s_tmp.add(cf1Visitor.getResult());
            }

            if (cf2 != nullptr) {
                Z3ExprVisitor cf2Visitor(c_tmp);
                cf2->accept(cf2Visitor);
                s_tmp.add(cf2Visitor.getResult());
            }

            s_tmp.add((e1 - e2 == 1) || (e2 - e1 == 1));

            if (s_tmp.check() == z3::sat) {
                // z3::model m = s_tmp.get_model();
                std::cout << "Race found between " << cop.first.getEventId() << " and " << cop.second.getEventId() << "\n";
                // std::cout << m << std::endl;
            } else {
                std::cout << "no race found" << "\n";
            }
            std::cout
                << "-------------------------------------------------------\n";
        }
    }
};