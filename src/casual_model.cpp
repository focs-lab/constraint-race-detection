#include "casual_model.hpp"

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
    LOG_INIT_COUT();
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& [lockId, threadIdToLockRegions] :
         trace_.getLockRegions()) {
        std::vector<uint32_t> threadIds;
        std::vector<LockRegion> lockRegions;

        for (const auto& pair : threadIdToLockRegions) {
            threadIds.push_back(pair.first);
            lockRegions.insert(lockRegions.end(), pair.second.begin(),
                               pair.second.end());
        }

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

        for (int tid1 = 0; tid1 < threadIds.size(); ++tid1) {
            for (int tid2 = tid1 + 1; tid2 < threadIds.size(); ++tid2) {
                uint32_t t1 = threadIds[tid1], t2 = threadIds[tid2];
                for (int i = 0; i < threadIdToLockRegions.at(t1).size(); ++i) {
                    const LockRegion& lr1 = threadIdToLockRegions.at(t1)[i];
                    for (int j = 0; j < threadIdToLockRegions.at(t2).size();
                         ++j) {
                        const LockRegion& lr2 = threadIdToLockRegions.at(t2)[j];
                        z3::expr condition =
                            var_map_[getEventIdx(lr1.getRelEvent())] <
                            var_map_[getEventIdx(lr2.getAcqEvent())];
                        for (int z = j + 1;
                             z < threadIdToLockRegions.at(t2).size(); ++z) {
                            const LockRegion& lr3 =
                                threadIdToLockRegions.at(t2)[z];
                            z3::expr result =
                                var_map_[getEventIdx(lr1.getRelEvent())] <
                                var_map_[getEventIdx(lr3.getAcqEvent())];
                            lock_constraints_.push_back(
                                implies(condition, result));
                        }
                    }
                }

                for (int i = 0; i < threadIdToLockRegions.at(t2).size(); ++i) {
                    const LockRegion& lr1 = threadIdToLockRegions.at(t2)[i];
                    for (int j = 0; j < threadIdToLockRegions.at(t1).size();
                         ++j) {
                        const LockRegion& lr2 = threadIdToLockRegions.at(t1)[j];
                        z3::expr condition =
                            var_map_[getEventIdx(lr1.getRelEvent())] <
                            var_map_[getEventIdx(lr2.getAcqEvent())];
                        for (int z = 0; z < threadIdToLockRegions.at(t1).size();
                             ++z) {
                            const LockRegion& lr3 =
                                threadIdToLockRegions.at(t1)[z];
                            z3::expr result =
                                var_map_[getEventIdx(lr1.getRelEvent())] <
                                var_map_[getEventIdx(lr3.getAcqEvent())];
                            lock_constraints_.push_back(
                                implies(condition, result));
                        }
                    }
                }
            }
        }
    }

    s_.add(lock_constraints_);

    auto end = std::chrono::high_resolution_clock::now();

    log(LOG_INFO) << "Lock constraints generation time: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - start)
                         .count()
                  << "\n";
}

z3::expr CasualModel::getPhiConc(Event e, bool track) {
    LOG_INIT_COUT();
    assert(e.getEventType() == Event::EventType::Read);

    if (read_to_phi_conc_offset_.find(e) == read_to_phi_conc_offset_.end()) {
        // if (track)
        //     log(LOG_INFO) << "PhiConc for: " << e.getEventId() << "\n";

        read_to_phi_conc_offset_[e] = read_to_phi_conc_.size();
        read_to_phi_conc_.push_back(getEventPhiZ3Expr(e));

        z3::expr phiAbs = getPhiAbs(e, track);
        z3::expr phiSC = getPhiSC(e, track);

        z3::expr phiConc = phiAbs & phiSC;

        read_to_phi_conc_.set(read_to_phi_conc_offset_[e], phiConc);
    }

    return getEventPhiZ3Expr(e);
}

z3::expr CasualModel::getPhiAbs(Event e, bool track) {
    LOG_INIT_COUT();
    Event prevRead = trace_.getPrevReadInThread(e);

    if (Event::isNullEvent(prevRead)) return c_.bool_val(true);

    return getPhiConc(prevRead, track);
}

z3::expr CasualModel::getPhiSC(Event e, bool track) {
    assert(e.getEventType() == Event::EventType::Read);
    LOG_INIT_COUT();

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

            rfConstraints.push_back(getPhiAbs(goodWrite, track));
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

            rfConstraints.push_back(getPhiAbs(goodWrite, track));
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

uint32_t CasualModel::solve(uint32_t maxCOPCheck, uint32_t maxRaceCheck) {
    uint32_t race_count = 0;
    LOG_INIT_COUT();

    z3::expr_vector race_constraints(c_);

    for (const auto& [e1, e2] : filtered_cop_events_) {
        z3::expr e1_expr = var_map_[getEventIdx(e1)];
        z3::expr e2_expr = var_map_[getEventIdx(e2)];

        bool track = e1.getEventId() == 235 && e2.getEventId() == 261;

        race_constraints.push_back((e1_expr == e2_expr) & getPhiAbs(e1, track) &
                                   getPhiAbs(e2, track));
    }

    for (const auto& [read, phiConcOffset] : read_to_phi_conc_offset_) {
        s_.add(getEventPhiZ3Expr(read) == read_to_phi_conc_[phiConcOffset]);
    }

    int i = 0;
    for (const auto& race_con : race_constraints) {
        if (maxCOPCheck && i > maxCOPCheck) break;

        z3::expr_vector race_sat(c_);
        race_sat.push_back(race_con);

        auto [e1, e2] = filtered_cop_events_[i];

        if (s_.check(race_sat) == z3::sat) {
            race_count++;
            if (log_witness_) {
                logger_.logWitnessPrefix(s_.get_model(), e1, e2);
            }

            if (maxRaceCheck && race_count >= maxRaceCheck) break;
        }

        i++;
    }

    return race_count;
}