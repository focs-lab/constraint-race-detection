#include "trace.hpp"

#include <fstream>

Trace Trace::createTrace(const std::vector<uint64_t>& raw_events) {
    std::vector<Event> all_events;

    std::vector<std::pair<Event, Event>> cops;
    std::vector<std::pair<Event, Event>> fork_begin_pairs;
    std::vector<std::pair<Event, Event>> end_join_pairs;

    std::unordered_map<uint32_t, Thread> thread_id_to_thread;
    std::unordered_map<uint32_t, Variable> var_id_to_variable;
    std::unordered_map<uint32_t, std::vector<LockRegion>>
        lock_id_to_lock_region;

    // start from event id 1 because 0 is reserved for null event
    uint32_t event_id = 1;

    // auxiliary structures to help in the construction of the trace
    std::unordered_map<uint32_t, Event> forks;
    std::unordered_map<uint32_t, Event> ends;
    std::unordered_map<uint32_t, Event> last_acquire;

    for (uint64_t raw_event : raw_events) {
        Event e(raw_event, event_id++);

        all_events.push_back(e);

        if (thread_id_to_thread.find(e.getThreadId()) ==
            thread_id_to_thread.end()) {
            thread_id_to_thread[e.getThreadId()] = Thread(e.getThreadId());
        }

        thread_id_to_thread[e.getThreadId()].addEvent(e);

        switch (e.getEventType()) {
            case Event::EventType::Fork:
                forks[e.getTargetId()] = e;
                break;
            case Event::EventType::Begin:
                fork_begin_pairs.emplace_back(forks[e.getThreadId()], e);
                break;
            case Event::EventType::End:
                ends[e.getThreadId()] = e;
                break;
            case Event::EventType::Join:
                end_join_pairs.emplace_back(ends[e.getTargetId()], e);
                break;
            case Event::EventType::Acquire:
                last_acquire[e.getTargetId()] = e;
                break;
            case Event::EventType::Release:
                lock_id_to_lock_region[e.getTargetId()].emplace_back(
                    last_acquire[e.getTargetId()], e);
                break;
            case Event::EventType::Read:
                if (var_id_to_variable.find(e.getTargetId()) ==
                    var_id_to_variable.end()) {
                    var_id_to_variable[e.getTargetId()] =
                        Variable(e.getTargetId());
                }
                var_id_to_variable[e.getTargetId()].addEvent(e);
                break;
            case Event::EventType::Write:
                if (var_id_to_variable.find(e.getTargetId()) ==
                    var_id_to_variable.end()) {
                    var_id_to_variable[e.getTargetId()] =
                        Variable(e.getTargetId());
                }
                var_id_to_variable[e.getTargetId()].addEvent(e);
                break;
            default:
                break;
        }
    }

    return Trace(all_events, fork_begin_pairs, end_join_pairs,
                 thread_id_to_thread, var_id_to_variable,
                 lock_id_to_lock_region);
}

Trace Trace::fromBinaryFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file_size % sizeof(uint64_t) != 0) {
        throw std::runtime_error("Invalid file size: " + filename);
    }

    std::vector<uint64_t> raw_events(file_size / sizeof(uint64_t));
    file.read(reinterpret_cast<char*>(raw_events.data()),
              file_size);  // safe to do this reinterpret_cast because we know
                           // the file is multiple of 64 bits and if the events
                           // are wrong, any way we read in the data will lead
                           // to invalid events which we can handle later

    file.close();

    return createTrace(raw_events);
}

Trace Trace::fromTextFile(const std::string& filename) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::unordered_map<std::string, Event::EventType> eventTypeMapping = {
        {"read", Event::EventType::Read},   {"write", Event::EventType::Write},
        {"acq", Event::EventType::Acquire}, {"rel", Event::EventType::Release},
        {"begin", Event::EventType::Begin}, {"end", Event::EventType::End},
        {"fork", Event::EventType::Fork},   {"join", Event::EventType::Join}};

    /* Global Trace information */
    std::unordered_map<std::string, uint32_t> varNameMapping;
    uint32_t availVarId = 0;
    std::unordered_map<std::string, uint32_t> lockNameMapping;
    uint32_t availLockId = 0;

    std::vector<uint64_t> raw_events;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string EventTypeStr, varName;
        uint32_t threadId, varValue, varId;

        if (!(iss >> EventTypeStr >> threadId >> varName >> varValue)) {
            throw std::runtime_error("Invalid event: " + line);
        }

        std::transform(EventTypeStr.begin(), EventTypeStr.end(),
                       EventTypeStr.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (eventTypeMapping.find(EventTypeStr) == eventTypeMapping.end()) {
            throw std::runtime_error("Invalid event type: " + EventTypeStr);
        }

        Event::EventType eventType = eventTypeMapping[EventTypeStr];

        if (eventType == Event::EventType::Read ||
            eventType == Event::EventType::Write) {
            if (varNameMapping.find(varName) == varNameMapping.end()) {
                varNameMapping[varName] = availVarId++;
            }
            varId = varNameMapping[varName];
        } else if (eventType == Event::EventType::Acquire ||
                   eventType == Event::EventType::Release) {
            if (lockNameMapping.find(varName) == lockNameMapping.end()) {
                lockNameMapping[varName] = availLockId++;
            }
            varId = lockNameMapping[varName];
        } else if (eventType == Event::EventType::Fork ||
                   eventType == Event::EventType::Join) {
            varId = std::stoul(varName);
        } else {
            varId = 0;
        }

        raw_events.push_back(
            Event::createRawEvent(eventType, threadId, varId, varValue));
    }

    return createTrace(raw_events);
}

std::vector<Event> Trace::getAllEvents() const { return all_events_; }

std::vector<std::pair<Event, Event>> Trace::getCOPs() const {
    std::vector<std::pair<Event, Event>> cops;

    for (auto& [_, var] : var_id_to_variable_) {
        std::vector<std::pair<Event, Event>> var_cops = var.getCOP();
        cops.insert(cops.end(), var_cops.begin(), var_cops.end());
    }

    return cops;
}

std::vector<std::pair<Event, Event>> Trace::getForkBeginPairs() const {
    return fork_begin_pairs_;
}

std::vector<std::pair<Event, Event>> Trace::getEndJoinPairs() const {
    return end_join_pairs_;
}

std::unordered_map<uint32_t, std::vector<LockRegion>> Trace::getLockRegions()
    const {
    return lock_id_to_lock_region_;
}

std::unordered_map<uint32_t,
                   std::unordered_map<uint32_t, std::vector<LockRegion>>>
Trace::getThreadIdToLockIdToLockRegions() const {
    std::unordered_map<uint32_t,
                       std::unordered_map<uint32_t, std::vector<LockRegion>>>
        res;
    for (const auto& [lockId, lockRegions] : lock_id_to_lock_region_) {
        for (const LockRegion& lockRegion : lockRegions) {
            res[lockRegion.getRegionThreadId()][lockId].push_back(lockRegion);
        }
    }
    return res;
}

Thread Trace::getThread(uint32_t thread_id) const {
    assert(thread_id_to_thread_.find(thread_id) != thread_id_to_thread_.end());
    return thread_id_to_thread_.at(thread_id);
}

std::vector<Thread> Trace::getThreads() const {
    std::vector<Thread> threads;
    for (const auto& [_, thread] : thread_id_to_thread_) {
        threads.push_back(thread);
    }
    return threads;
}

std::vector<Event> Trace::getGoodWritesForRead(const Event& read) const {
    assert(var_id_to_variable_.find(read.getTargetId()) !=
           var_id_to_variable_.end());
    return var_id_to_variable_.at(read.getTargetId()).getGoodWrites(read);
}

std::vector<Event> Trace::getBadWritesForRead(const Event& read) const {
    assert(var_id_to_variable_.find(read.getTargetId()) !=
           var_id_to_variable_.end());
    return var_id_to_variable_.at(read.getTargetId()).getBadWrites(read);
}

Event Trace::getEvent(uint32_t eid) const {
    assert(eid >= 1 && eid <= all_events_.size());
    return all_events_[eid - 1]; // minus 1 since eid are starting from 1
}

Event Trace::getPrevReadInThread(const Event& e) const {
    return thread_id_to_thread_.at(e.getThreadId()).getPrevRead(e);
}

Event Trace::getSameThreadSameVarPrevWrite(const Event& e) const {
    return var_id_to_variable_.at(e.getTargetId()).getPrevWriteInThread(e);
}

Event Trace::getPrevDiffReadInThread(const Event& e) const {
    return var_id_to_variable_.at(e.getTargetId()).getPrevDiffReadInThread(e);
}

bool Trace::hasSameInitialValue(const Event& e) const {
    return var_id_to_variable_.at(e.getTargetId()).sameInitialValue(e);
}