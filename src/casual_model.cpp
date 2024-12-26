#include "casual_model.h"

void CasualModel::filterCOPs() {
    for (const auto& [e1, e2] : trace_.getCOPs()) {
        if (mhb_closure_.happensBefore(e1, e2) ||
            mhb_closure_.happensBefore(e2, e1))
            continue;
        if (lockset_engine_.hasCommonLock(e1, e2)) continue;
        filtered_cop_events_.push_back({e1, e2});
    }
}

void CasualModel::generateZ3VarMap() {
    for (const auto& event : trace_.getAllEvents()) {
        var_map_.push_back(getEventOrderZ3Expr(event));
    }
}

void CasualModel::generateMHBConstraints() {
    TransitiveClosure::Builder builder(trace_.getAllEvents().size());

    for (const auto& thread : trace_.getThreads()) {
        std::vector<Event> events = thread.getEvents();

        for (int i = 0; i < events.size(); ++i) {
            if (i == 0) {
                builder.createNewGroup(events[i]);
                continue;
            };
            Event e1 = events[i - 1];
            Event e2 = events[i];

            mhb_constraints_.push_back(var_map_[getEventIdx(e1)] <
                                       var_map_[getEventIdx(e2)]);

            if (e1.getEventType() == Event::EventType::Fork ||
                e2.getEventType() == Event::EventType::Join) {
                builder.createNewGroup(e2);
                builder.addRelation(e1, e2);
            } else {
                builder.addToGroup(e2, e1);
            }
        }
    }

    for (const auto [forkEvent, beginEvent] : trace_.getForkBeginPairs()) {
        mhb_constraints_.push_back(var_map_[getEventIdx(forkEvent)] <
                                   var_map_[getEventIdx(beginEvent)]);
        builder.addRelation(forkEvent, beginEvent);
    }

    for (const auto [endEvent, joinEvent] : trace_.getEndJoinPairs()) {
        mhb_constraints_.push_back(var_map_[getEventIdx(endEvent)] <
                                   var_map_[getEventIdx(joinEvent)]);
        builder.addRelation(endEvent, joinEvent);
    }

    mhb_closure_ = builder.build();

    s_.add(mhb_constraints_);
}

void CasualModel::generateLockConstraints() {
    for (const auto& [lockId, lockRegions] : trace_.getLockRegions()) {
        for (int i = 0; i < lockRegions.size(); ++i) {
            for (int j = i + 1; j < lockRegions.size(); ++j) {
                const LockRegion& lr1 = lockRegions[i];
                const LockRegion& lr2 = lockRegions[j];

                if (lr1.getRegionThreadId() == lr2.getRegionThreadId())
                    continue;

                if (mhb_closure_.happensBefore(lr1.getRelEvent(),
                                               lr2.getAcqEvent()) ||
                    mhb_closure_.happensBefore(lr2.getRelEvent(),
                                               lr1.getAcqEvent()))
                    continue;

                z3::expr rel1_lt_acq2 =
                    var_map_[getEventIdx(lr1.getRelEvent())] <
                    var_map_[getEventIdx(lr2.getAcqEvent())];
                z3::expr rel2_lt_acq1 =
                    var_map_[getEventIdx(lr2.getRelEvent())] <
                    var_map_[getEventIdx(lr1.getAcqEvent())];

                lock_constraints_.push_back(rel1_lt_acq2 ^ rel2_lt_acq1);
            }
        }
    }

    s_.add(lock_constraints_);
}

z3::expr CasualModel::getPhiConc(Event e) {
    assert(e.getEventType() == Event::EventType::Read);

    if (read_to_phi_conc_offset_.find(e) == read_to_phi_conc_offset_.end()) {
        read_to_phi_conc_offset_[e] = read_to_phi_conc_.size();
        read_to_phi_conc_.push_back(getEventPhiZ3Expr(e));

        z3::expr phiAbs = getPhiAbs(e);
        z3::expr phiSC = getPhiSC(e);

        z3::expr phiConc = phiAbs & phiSC;

        read_to_phi_conc_.set(read_to_phi_conc_offset_[e], phiConc);
    }

    return getEventPhiZ3Expr(e);
}

z3::expr CasualModel::getPhiAbs(Event e) {
    Event prevRead = trace_.getPrevReadInThread(e);

    if (Event::isNullEvent(prevRead)) return c_.bool_val(true);

    return getPhiConc(prevRead);
}

z3::expr CasualModel::getPhiSC(Event e) {
    assert(e.getEventType() == Event::EventType::Read);

    std::vector<Event> goodWrites = trace_.getGoodWritesForRead(e);
    std::vector<Event> badWrites = trace_.getBadWritesForRead(e);

    bool sameInitialValue = trace_.hasSameInitialValue(e);

    Event sameThreadSameVarPrevWrite = trace_.getSameThreadSameVarPrevWrite(e);

    generalGoodWritesFilter(goodWrites, e);
    generalBadWritesFilter(badWrites, e);

    z3::expr_vector maintainReadValConstraints(c_);

    if (sameInitialValue) {
        z3::expr_vector rfConstraints(c_);
        for (const Event& badWrite : badWrites) {
            rfConstraints.push_back(var_map_[getEventIdx(e)] <
                                    var_map_[getEventIdx(badWrite)]);
        }

        /* If same initial value, it does not need a good write. Without the
         * below constraints we cannot capture this scenario */
        for (const Event& goodWrite : goodWrites) {
            rfConstraints.push_back(var_map_[getEventIdx(e)] <
                                    var_map_[getEventIdx(goodWrite)]);
        }
        if (rfConstraints.size() > 0)
            maintainReadValConstraints.push_back(z3::mk_and(rfConstraints));
    }

    if (!Event::isNullEvent(sameThreadSameVarPrevWrite)) {
        /* case 1 optimization: extra filtering */
        prevWriteExistFilter(goodWrites, sameThreadSameVarPrevWrite);
        prevWriteExistFilter(badWrites, sameThreadSameVarPrevWrite);

        if (sameThreadSameVarPrevWrite.getTargetValue() == e.getTargetValue()) {
            z3::expr_vector rfConstraints(c_);
            for (const Event& badWrite : badWrites) {
                rfConstraints.push_back(
                    makeRFConstraint(e, sameThreadSameVarPrevWrite, badWrite));
            }
            if (rfConstraints.size() > 0)
                maintainReadValConstraints.push_back(z3::mk_and(rfConstraints));
        }

        for (const Event& goodWrite : goodWrites) {
            z3::expr_vector rfConstraints(c_);

            rfConstraints.push_back(getPhiAbs(goodWrite));
            rfConstraints.push_back(var_map_[getEventIdx(goodWrite)] <
                                    var_map_[getEventIdx(e)]);

            /* case 1.2 optimization: extra constraints */
            if (sameThreadSameVarPrevWrite.getTargetValue() !=
                e.getTargetValue())
                rfConstraints.push_back(
                    var_map_[getEventIdx(sameThreadSameVarPrevWrite)] <
                    var_map_[getEventIdx(goodWrite)]);

            for (const Event& badWrite : badWrites) {
                if (hb(badWrite, goodWrite)) continue;

                rfConstraints.push_back(
                    makeRFConstraint(e, goodWrite, badWrite));
            }

            if (rfConstraints.size() > 0)
                maintainReadValConstraints.push_back(z3::mk_and(rfConstraints));
        }
    } else {
        /* case 2: has no write event on the same thread previously */
        Event sameThreadPrevDiffRead = trace_.getPrevDiffReadInThread(e);

        if (!Event::isNullEvent(sameThreadPrevDiffRead)) {
            /* we can treat the prev diff read as a 'bad write' and filter out
             * all good writes that hb this prev diff read */
            prevWriteExistFilter(goodWrites, sameThreadPrevDiffRead);

            // TODO: Should I filter out bad writes that hb this prev diff read
            // as well?
        }

        for (const Event& goodWrite : goodWrites) {
            z3::expr_vector rfConstraints(c_);

            rfConstraints.push_back(getPhiAbs(goodWrite));
            rfConstraints.push_back(var_map_[getEventIdx(goodWrite)] <
                                    var_map_[getEventIdx(e)]);

            if (!Event::isNullEvent(sameThreadPrevDiffRead))
                rfConstraints.push_back(
                    var_map_[getEventIdx(sameThreadPrevDiffRead)] <
                    var_map_[getEventIdx(goodWrite)]);

            for (const Event& badWrite : badWrites) {
                if (hb(badWrite, goodWrite)) continue;

                rfConstraints.push_back(
                    makeRFConstraint(e, goodWrite, badWrite));
            }

            if (rfConstraints.size() > 0)
                maintainReadValConstraints.push_back(z3::mk_and(rfConstraints));
        }
    }

    if (maintainReadValConstraints.size() > 0)
        return z3::mk_or(maintainReadValConstraints);

    return c_.bool_val(true);
}

uint32_t CasualModel::solve() {
    uint32_t race_count = 0;

    z3::expr_vector race_constraints(c_);

    for (const auto& [e1, e2] : filtered_cop_events_) {
        z3::expr e1_expr = var_map_[getEventIdx(e1)];
        z3::expr e2_expr = var_map_[getEventIdx(e2)];

        race_constraints.push_back(e1_expr == e2_expr & getPhiAbs(e1) &
                                   getPhiAbs(e2));
    }

    for (const auto& [read, phiConcOffset] : read_to_phi_conc_offset_) {
        s_.add(getEventPhiZ3Expr(read) == read_to_phi_conc_[phiConcOffset]);
    }

    int i = 0;
    for (const auto& race_con : race_constraints) {
        z3::expr_vector race_sat(c_);
        race_sat.push_back(race_con);

        if (s_.check(race_sat) == z3::sat) {
            race_count++;

            if (log_witness_) {
                auto [e1, e2] = filtered_cop_events_[i];
                logger_.logWitnessPrefix(s_.get_model(), e1, e2);
            }
        }
        i++;
    }

    return race_count;
}