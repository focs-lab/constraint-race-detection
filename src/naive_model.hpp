#pragma once

#include <z3++.h>

#include <utility>
#include <vector>

#include "lockset_engine.hpp"
#include "model.hpp"

class NaiveModel : public Model {
   private:
    z3::context c_;
    z3::solver s_;

    LocksetEngine lockset_engine_;

    std::vector<std::pair<Event, Event>> filtered_cop_events_;

    z3::expr_vector var_map_;
    z3::expr_vector program_order_constraints_;
    z3::expr_vector rf_constraints_;
    z3::expr_vector lock_constraints_;

    static constexpr const char* EVENT_ORDER_PREFIX = "e_";
    static constexpr const char* EVENT_PHI_PREFIX = "phi_";

    void filterCOPs();
    void initializeModel();

    void generateZ3VarMap();

    void generateProgramOrderConstraints();
    void generateRFConstraints();
    void generateLockConstraints();

    z3::expr getRFConstraint(const Event read);

    inline size_t getEventIdx(const Event e) { return e.getEventId() - 1; }

    inline z3::expr getEventOrderZ3Expr(const Event e) {
        return c_.int_const(
            (EVENT_ORDER_PREFIX + std::to_string(e.getEventId())).c_str());
    }

    inline z3::expr getEventPhiZ3Expr(const Event e) {
        return c_.bool_const(
            (EVENT_PHI_PREFIX + std::to_string(e.getEventId())).c_str());
    }

    inline z3::expr getEventPhiZ3Expr(const EID e) {
        return c_.bool_const((EVENT_PHI_PREFIX + std::to_string(e)).c_str());
    }

    inline z3::expr getEventExpr(const Event e) {
        return var_map_[getEventIdx(e)];
    }

    inline z3::expr getEventInWitnessConstraint(const Event e) {
        return getEventExpr(e) > 0;
    }

    inline z3::expr makeRFConstraint(const Event read, const Event goodWrite, const Event badWrite) {
        /* if gw < bw -> r < bw for rf to be maintained */
        if (hb(goodWrite, badWrite))
            return var_map_[getEventIdx(read)] <
                   var_map_[getEventIdx(badWrite)];

        /* if bw < r -> bw < gw for rf to be maintained */
        if (hb(badWrite, read))
            return var_map_[getEventIdx(badWrite)] <
                   var_map_[getEventIdx(goodWrite)];

        /* if no hb relations then bw < gw || r < bw */
        return var_map_[getEventIdx(badWrite)] <
                   var_map_[getEventIdx(goodWrite)] ||
               var_map_[getEventIdx(read)] < var_map_[getEventIdx(badWrite)];
    }

    inline bool hb(const Event& e1, const Event& e2) {
        return hb_closure_.happensBefore(e1, e2) ||
               (e1.getThreadId() == e2.getThreadId() &&
                e1.getEventId() < e2.getEventId());
    }

    inline void generalGoodWritesFilter(std::vector<Event>& goodWrites,
                                        const Event& r) {
        /* Filter out good writes w s.t. w < w' < r where w' is another good
         * write or if r < w.
         */
        // Make a copy for the inner loop to avoid iterator invalidation
        const std::vector<Event> goodWritesCopy = goodWrites;

        goodWrites.erase(
            std::remove_if(
                goodWrites.begin(), goodWrites.end(),
                [&goodWritesCopy, r, this](const Event& write) {
                    return hb(r, write) ||
                           std::any_of(
                               goodWritesCopy.begin(), goodWritesCopy.end(),
                               [&write, r, this](const Event& otherWrite) {
                                   return (hb(write, otherWrite) &&
                                           hb(otherWrite, r));
                               });
                }),
            goodWrites.end());
    }

    inline void generalBadWritesFilter(std::vector<Event>& badWrites,
                                       const Event& r) {
        /* Filter out bad writes w' s.t. e < w'
         */
        // TODO: can we also filter out all w' < w < r where w is some good
        // write?
        badWrites.erase(std::remove_if(badWrites.begin(), badWrites.end(),
                                       [r, this](const Event& write) {
                                           return hb(r, write);
                                       }),
                        badWrites.end());
    }

   public:
    NaiveModel(Trace& trace, VecTransitiveClosure& vc_hb, ModelLogger& logger,
               bool log_witness)
        : Model(trace, vc_hb, logger, log_witness),
          c_(),
          s_(c_, "QF_IDL"),
          var_map_(c_),
          program_order_constraints_(c_),
          rf_constraints_(c_),
          lock_constraints_(c_),
          lockset_engine_(trace_.getThreadIdToLockIdToLockRegions()) {
        initializeModel();
    }

    uint32_t solve(uint32_t maxCOPCheck, uint32_t maxRaceCheck) override;
};