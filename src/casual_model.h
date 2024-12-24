#include <z3++.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include "event.h"
#include "lockset_engine.h"
#include "trace.h"
#include "transitive_closure.h"

class CasualModel {
   private:
    Trace& trace_;

    z3::context c_;
    z3::solver s_;

    z3::expr_vector var_map_;
    z3::expr_vector mhb_constraints_;
    z3::expr_vector lock_constraints_;
    z3::expr_vector read_to_phi_conc_;

    std::unordered_map<Event, uint32_t, EventHash> read_to_phi_conc_offset_;

    LocksetEngine lockset_engine_;
    TransitiveClosure mhb_closure_;

    std::vector<std::pair<Event, Event>> filtered_cop_events_;

    void filterCOPs();

    void generateZ3VarMap();
    void generateMHBConstraints();
    void generateLockConstraints();

    z3::expr getPhiConc(Event e);
    z3::expr getPhiAbs(Event e);
    z3::expr getPhiSC(Event e);

    inline uint32_t getEventIdx(const Event e) { return e.getEventId() - 1; }

    inline z3::expr getEventOrderZ3Expr(const Event e) {
        return c_.int_const(("e_" + std::to_string(e.getEventId())).c_str());
    }

    inline z3::expr getEventPhiZ3Expr(const Event e) {
        return c_.bool_const(("phi_" + std::to_string(e.getEventId())).c_str());
    }

    inline bool hb(const Event& e1, const Event& e2) {
        return mhb_closure_.happensBefore(e1, e2) ||
               (e1.getTargetId() == e2.getTargetId() &&
                e1.getEventId() < e2.getEventId());
    }

    inline z3::expr makeRFConstraint(const Event& read, const Event& goodWrite,
                                     const Event& badWrite) {
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

    inline void generalGoodWritesFilter(std::vector<Event>& goodWrites,
                                        const Event& r) {
        /* Filter out good writes w s.t. w < w' < r where w' is another good
         * write or if r < w.
         */
        goodWrites.erase(
            std::remove_if(goodWrites.begin(), goodWrites.end(),
                           [&goodWrites, r, this](const Event& write) {
                               return hb(r, write) ||
                                      std::any_of(
                                          goodWrites.begin(), goodWrites.end(),
                                          [&write, r,
                                           this](const Event& otherWrite) {
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
        badWrites.erase(
            std::remove_if(badWrites.begin(), badWrites.end(),
                           [&badWrites, r, this](const Event& write) {
                               return hb(r, write);
                           }),
            badWrites.end());
    }

    inline void prevWriteExistFilter(std::vector<Event>& writes,
                                     const Event& prevWrite) {
        /* When a previous write exists in the same thread, erase all writes
         * that occur before it. This can occur in two ways:
         * 1. The other write m.h.b before the previous write
         * 2. The other write is in the same thread as the previous write (but
         * we know previous write is the latest wrtie)
         */
        writes.erase(std::remove_if(writes.begin(), writes.end(),
                                    [prevWrite, this](const Event& write) {
                                        return hb(write, prevWrite);
                                    }),
                     writes.end());
    }

   public:
    CasualModel(Trace& trace)
        : trace_(trace),
          lockset_engine_(trace_.getThreadIdToLockIdToLockRegions()),
          c_(),
          s_(c_, "QF_IDL"),
          var_map_(c_),
          mhb_constraints_(c_),
          lock_constraints_(c_),
          read_to_phi_conc_(c_) {
        z3::params p(c_);
        p.set("auto_config", false);
        p.set("smt.arith.solver", (unsigned)1);
        s_.set(p);
        generateZ3VarMap();
        generateMHBConstraints();
        generateLockConstraints();
        filterCOPs();
    }

    uint32_t solve();
};