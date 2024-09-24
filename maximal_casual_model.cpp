#include <z3++.h>

#include <utility>
#include <vector>
#include <memory>

#include "event.cpp"
#include "trace.cpp"
#include "expr.cpp"

const std::shared_ptr<Expr> zeroExpr = ExprBuilder::var("0");

class MaximalCasualModel {
   private:
    Trace& trace;

    std::vector<std::shared_ptr<Expr>> mhbs;
    std::shared_ptr<Expr> mhbsExpr;
    std::vector<std::shared_ptr<Expr>> allEvents;
    std::vector<std::shared_ptr<Expr>> lock_constrains;
    std::shared_ptr<Expr> lockExpr;

    z3::expr getZ3ExprFromEvent(const Event& e, z3::context& c) {
        return c.int_const(std::to_string(e.getEventId()).c_str());
    }

    z3::expr getZ3ExprFromEvent(uint32_t eid, z3::context& c) {
        return c.int_const(std::to_string(eid).c_str());
    }

   public:
    MaximalCasualModel(Trace& trace_)
        : mhbs(), allEvents(), lock_constrains(), trace(trace_) {}

    void generateMHBConstraints() {
        for (const auto& [threadId, events] : trace.getThreadToEventsMap()) {
            for (int i = 0; i < events.size(); ++i) {
                // z3::expr eventExpr = getZ3ExprFromEvent(events[i]);
                std::shared_ptr<Expr> eventExpr = ExprBuilder::var(std::to_string(events[i]));
                allEvents.push_back(eventExpr);
                if (i == 0) {
                    std::shared_ptr<Expr> orderExpr = ExprBuilder::lessThan(zeroExpr, eventExpr);
                    mhbs.push_back(orderExpr);
                } else {
                    // z3::expr prevEventExpr = getZ3ExprFromEvent(events[i - 1]);
                    std::shared_ptr<Expr> prevEventExpr = ExprBuilder::var(std::to_string(events[i - 1]));
                    std::shared_ptr<Expr> orderExpr = ExprBuilder::lessThan(prevEventExpr, eventExpr);
                    mhbs.push_back(orderExpr);
                }
            }
        }

        for (const auto& [forkEvent, beginEvent] : trace.getForkBeginPairs()) {
            std::shared_ptr<Expr> forkEventExpr = ExprBuilder::var(std::to_string(forkEvent));
            std::shared_ptr<Expr> beginEventExpr = ExprBuilder::var(std::to_string(beginEvent));
            std::shared_ptr<Expr> orderExpr = ExprBuilder::lessThan(forkEventExpr, beginEventExpr);
            mhbs.push_back(orderExpr);
        }

        for (const auto& [joinEvent, endEvent] : trace.getJoinEndPairs()) {
            std::shared_ptr<Expr> joinEventExpr = ExprBuilder::var(std::to_string(joinEvent));
            std::shared_ptr<Expr> endEventExpr = ExprBuilder::var(std::to_string(endEvent));
            std::shared_ptr<Expr> orderExpr = ExprBuilder::lessThan(joinEventExpr, endEventExpr);
            mhbs.push_back(orderExpr);
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
                // z3::expr acq1 = getZ3ExprFromEvent(acq_rel_pairs[i].first);
                // z3::expr rel1 = getZ3ExprFromEvent(acq_rel_pairs[i].second);
                std::shared_ptr<Expr> acq1 = ExprBuilder::var(std::to_string(acq_rel_pairs[i].first.getEventId()));
                std::shared_ptr<Expr> rel1 = ExprBuilder::var(std::to_string(acq_rel_pairs[i].second.getEventId()));
                for (int j = i + 1; j < acq_rel_pairs.size(); ++j) {
                    if (acq_rel_pairs[i].first.getThreadId() ==
                        acq_rel_pairs[j].first.getThreadId())
                        continue;
                    // z3::expr acq2 = getZ3ExprFromEvent(acq_rel_pairs[j].first);
                    // z3::expr rel2 = getZ3ExprFromEvent(acq_rel_pairs[j].second);
                    std::shared_ptr<Expr> acq2 = ExprBuilder::var(std::to_string(acq_rel_pairs[j].first.getEventId()));
                    std::shared_ptr<Expr> rel2 = ExprBuilder::var(std::to_string(acq_rel_pairs[j].second.getEventId()));
                    lock_constrains.push_back(ExprBuilder::orExpr(ExprBuilder::lessThan(rel1, acq2), ExprBuilder::lessThan(rel2, acq1)));
                }
            }
        }

        if (lock_constrains.size() == 0) return;

        lockExpr = lock_constrains[0];
        for (int i = 1; i < lock_constrains.size(); ++i) {
            lockExpr = ExprBuilder::andExpr(lockExpr, lock_constrains[i]);
        }
    }

    void solveForRace() {
        for (const auto& cop : trace.getCOPEvents()) {
            z3::context c_tmp;
            z3::solver s_tmp(c_tmp);
            z3::expr e1 = getZ3ExprFromEvent(cop.first, c_tmp);
            z3::expr e2 = getZ3ExprFromEvent(cop.second, c_tmp);

            z3::expr zero = c_tmp.int_const("0");
            s_tmp.add(zero == 0);

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

            s_tmp.add((e1 - e2 == 1) || (e2 - e1 == 1));

            if (s_tmp.check() == z3::sat) {
                z3::model m = s_tmp.get_model();
                std::cout << "Race found between " << cop.first.getEventId() << " and " << cop.second.getEventId() << "\n";
                std::cout << m << std::endl;
            } else {
                std::cout << "no race found" << "\n";
            }
            std::cout
                << "-------------------------------------------------------\n";
        }
    }
};