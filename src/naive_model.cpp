#include "naive_model.hpp"

#include "logger.hpp"

void NaiveModel::initializeModel() {
    filterCOPs();
    generateZ3VarMap();
    generateProgramOrderConstraints();
    generateRFConstraints();
    generateLockConstraints();
}

void NaiveModel::filterCOPs() {
    for (const auto& [e1, e2] : trace_.getCOPs()) {
        if (lockset_engine_.hasCommonLock(e1, e2)) continue;

        const Variable& var = trace_.getVariable(e1.getTargetId());

        if (e1.getEventType() == Event::EventType::Read &&
            var.hasUniqueWriter(e1)) {
            filtered_cop_events_.push_back({e1, e2});
            continue;
        } else if (e2.getEventType() == Event::EventType::Read &&
                   var.hasUniqueWriter(e2)) {
            filtered_cop_events_.push_back({e1, e2});
            continue;
        }

        if (hb_closure_.happensBefore(e1, e2) ||
            hb_closure_.happensBefore(e2, e1))
            continue;
        filtered_cop_events_.push_back({e1, e2});
    }
}

void NaiveModel::generateZ3VarMap() {
    for (const auto& event : trace_.getAllEvents()) {
        var_map_.push_back(getEventOrderZ3Expr(event));
    }
}

void NaiveModel::generateProgramOrderConstraints() {
    for (const auto& thread : trace_.getThreads()) {
        std::vector<Event> events = thread.getEvents();

        for (size_t i = 0; i < events.size(); ++i) {
            if (i == 0) continue;

            Event e1 = events[i - 1];
            Event e2 = events[i];

            program_order_constraints_.push_back(
                implies(getEventInWitnessConstraint(e2),
                        (getEventExpr(e1) < getEventExpr(e2)) &&
                            getEventInWitnessConstraint(e1)));
        }
    }

    for (const auto& [forkEvent, beginEvent] : trace_.getForkBeginPairs()) {
        program_order_constraints_.push_back(
            implies(getEventInWitnessConstraint(beginEvent),
                    (getEventExpr(forkEvent) < getEventExpr(beginEvent)) &&
                        getEventInWitnessConstraint(forkEvent)));
    }

    for (const auto& [endEvent, joinEvent] : trace_.getEndJoinPairs()) {
        program_order_constraints_.push_back(
            implies(getEventInWitnessConstraint(joinEvent),
                    (getEventExpr(endEvent) < getEventExpr(joinEvent)) &&
                        getEventInWitnessConstraint(endEvent)));
    }

    s_.add(program_order_constraints_);
}

void NaiveModel::generateRFConstraints() {
    for (const auto& read : trace_.getAllReads()) {
        rf_constraints_.push_back(
            implies(getEventInWitnessConstraint(read), getRFConstraint(read)));
    }

    // LOG(rf_constraints_);

    s_.add(rf_constraints_);
}

void NaiveModel::generateLockConstraints() {
    for (const auto& [lockId, lockRegions] : trace_.getLockRegions()) {
        for (size_t i = 0; i < lockRegions.size(); ++i) {
            const LockRegion& lr1 = lockRegions[i];
            z3::expr_vector lockConstraints(c_);
            for (size_t j = 0; j < lockRegions.size(); ++j) {
                if (i == j) continue;

                const LockRegion& lr2 = lockRegions[j];

                if (lr1.getRegionThreadId() == lr2.getRegionThreadId())
                    continue;

                if (hb(lr1.getRelEvent(), lr2.getAcqEvent()) ||
                    hb(lr2.getRelEvent(), lr1.getAcqEvent()))
                    continue;

                z3::expr rel2_lt_acq1 =
                    getEventInWitnessConstraint(lr2.getRelEvent()) &&
                    getEventExpr(lr2.getRelEvent()) <
                        getEventExpr(lr1.getAcqEvent());

                z3::expr rel1_lt_acq2 =
                    getEventInWitnessConstraint(lr1.getRelEvent()) &&
                    (getEventExpr(lr1.getRelEvent()) <
                     getEventExpr(lr2.getAcqEvent()));

                lockConstraints.push_back(
                    implies(getEventInWitnessConstraint(lr2.getAcqEvent()),
                            rel2_lt_acq1 || rel1_lt_acq2));
            }

            if (lockConstraints.size() > 0)
                lock_constraints_.push_back(
                    implies(getEventInWitnessConstraint(lr1.getAcqEvent()),
                            z3::mk_and(lockConstraints)));
        }
    }

    // LOG(lock_constraints_);

    s_.add(lock_constraints_);
}

z3::expr NaiveModel::getRFConstraint(const Event read) {
    assert(read.getEventType() == Event::EventType::Read);

    std::vector<Event> goodWrites = trace_.getGoodWritesForRead(read);
    std::vector<Event> badWrites = trace_.getBadWritesForRead(read);

    bool sameInitialValue = trace_.hasSameInitialValue(read);

    generalGoodWritesFilter(goodWrites, read);
    generalBadWritesFilter(badWrites, read);

    z3::expr_vector rfConstraints(c_);

    if (sameInitialValue) {
        z3::expr_vector badWritesConstraints(c_);
        for (const Event& badWrite : badWrites) {
            badWritesConstraints.push_back(
                implies(getEventInWitnessConstraint(badWrite),
                        getEventExpr(read) < getEventExpr(badWrite)));
        }
        for (const Event& goodWrite : goodWrites) {
            badWritesConstraints.push_back(
                implies(getEventInWitnessConstraint(goodWrite),
                        getEventExpr(read) < getEventExpr(goodWrite)));
        }
        if (badWritesConstraints.size() > 0)
            rfConstraints.push_back(z3::mk_and(badWritesConstraints));
    }

    for (const Event& goodWrite : goodWrites) {
        z3::expr_vector goodWriteConstraints(c_);

        goodWriteConstraints.push_back(getEventInWitnessConstraint(goodWrite));
        goodWriteConstraints.push_back(getEventExpr(goodWrite) <
                                       getEventExpr(read));

        z3::expr_vector badWriteConstraints(c_);
        for (const Event& badWrite : badWrites) {
            badWriteConstraints.push_back(
                implies(getEventInWitnessConstraint(badWrite),
                        makeRFConstraint(read, goodWrite, badWrite)));
        }
        if (badWriteConstraints.size() > 0)
            goodWriteConstraints.push_back(z3::mk_and(badWriteConstraints));

        rfConstraints.push_back(z3::mk_and(goodWriteConstraints));
    }

    if (rfConstraints.size() > 0)
        return z3::mk_or(rfConstraints);

    return c_.bool_val(true);
}

uint32_t NaiveModel::solve(uint32_t maxCOPCheck, uint32_t maxRaceCheck) {
    uint32_t race_count = 0;

    z3::expr_vector race_constraints(c_);

    size_t i = 0;

    // LOG(s_);

    for (const auto& [e1, e2] : filtered_cop_events_) {
        if (maxCOPCheck && i >= maxCOPCheck) break;

        z3::expr_vector race_sat(c_);

        race_sat.push_back(getEventExpr(e1) == 0);
        race_sat.push_back(getEventExpr(e2) == 0);

        Event prev_e1 = trace_.getPrevEventInThread(e1);
        Event prev_e2 = trace_.getPrevEventInThread(e2);

        if (!Event::isNullEvent(prev_e1)) {
            race_sat.push_back(getEventExpr(prev_e1) > 0);
        }

        if (!Event::isNullEvent(prev_e2)) {
            race_sat.push_back(getEventExpr(prev_e2) > 0);
        }

        // LOG(race_sat);

        if (s_.check(race_sat) == z3::sat) {
            // LOG("race found between ", e1.getEventId(), " and ",
            //     e2.getEventId());
            race_count++;
        }
        i++;
    }
    return race_count;
}