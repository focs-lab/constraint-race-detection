#include <filesystem>
#include <iostream>

#include "logger.hpp"
#include "cmd_argument_parser.cpp"
#include "model_logger.hpp"
#include "trace.hpp"

bool isWitnessConsistent(const std::vector<uint32_t>& witness,
                         const Trace& trace) {
    std::vector<Event> events = trace.getAllEvents();

    std::unordered_map<uint32_t, uint32_t>
        threadEventTracker;  // thread id -> idx which event should be next in
                             // the thread
    std::unordered_map<uint32_t, std::vector<Event>>
        threadEvents;  // thread id -> events in the thread

    std::unordered_map<uint32_t, uint32_t>
        variableIdToVal;  // variable id -> supposed value of the variable

    std::unordered_map<uint32_t, bool>
        lockIdToLockStatus;  // lock id -> lock availability
    std::unordered_map<uint32_t, uint32_t>
        lockIdToThread;  // lock id -> thread id holding the lock


    size_t i = 0;
    for (auto e : witness) {
        if (i >= witness.size() - 2)
            break; // the last two events are the COP themselevs

        Event event = events[e - 1];
        uint32_t threadId = event.getThreadId();

        if (threadEventTracker.find(threadId) == threadEventTracker.end()) {
            threadEventTracker[threadId] = 0;
        }

        if (threadEvents.find(threadId) == threadEvents.end()) {
            threadEvents[threadId] = trace.getThread(threadId).getEvents();
        }

        /* Check if all the preceding events in the same thread has occured */
        if (threadEvents[threadId][threadEventTracker[threadId]].getEventId() !=
            e) {
            return false;
        } else {
            threadEventTracker[threadId]++;
        }

        /* If its a lock, ensure it is consistent */
        if (event.getEventType() == Event::EventType::Acquire) {
            uint32_t lockId = event.getTargetId();
            if (lockIdToLockStatus.find(lockId) != lockIdToLockStatus.end() &&
                !lockIdToLockStatus[lockId]) {
                return false;
            }
            lockIdToLockStatus[event.getTargetId()] = false;
            lockIdToThread[lockId] = threadId;
        } else if (event.getEventType() == Event::EventType::Release) {
            uint32_t lockId = event.getTargetId();
            if (lockIdToLockStatus.find(lockId) == lockIdToLockStatus.end() ||
                lockIdToLockStatus[lockId] ||
                lockIdToThread.find(lockId) == lockIdToThread.end() ||
                lockIdToThread[lockId] != threadId) {
                return false;
            }
            lockIdToLockStatus[lockId] = true;
        }

        /* If its a variable, ensure the reads get their value from the latest
         * write */
        if (event.getEventType() == Event::EventType::Read) {
            uint32_t varId = event.getTargetId();

            if (variableIdToVal.find(varId) == variableIdToVal.end()) {
                variableIdToVal[varId] = event.getTargetValue();
            }

            if (variableIdToVal[varId] != event.getTargetValue()) {
                return false;
            }
        } else if (event.getEventType() == Event::EventType::Write) {
            variableIdToVal[event.getTargetId()] = event.getTargetValue();
        }

        i++;
    }

    return true;
}

int main(int argc, char* argv[]) {
    try {
        Arguments args = Arguments::fromArgs(argc, argv);
        std::filesystem::path inputTracePath(args.executionTrace);
        std::string witnessPath =
            args.witnessDir + "/" + inputTracePath.stem().string();

        Trace trace = args.binaryFormat
                          ? Trace::fromBinaryFile(args.executionTrace)
                          : Trace::fromTextFile(args.executionTrace);

        std::vector<std::vector<uint32_t>> binaryWitness =
            ModelLogger::readBinaryWitness(witnessPath);

        int i = 0;
        
        std::vector<int> failed_witness;

        for (auto witness : binaryWitness) {
            bool res = isWitnessConsistent(witness, trace);

            if (!res) {
                failed_witness.push_back(i);
            }

            i++;
        }

        if (failed_witness.size() > 0) {
            std::cout << "Following witness failed: ";
            for (int fail : failed_witness)
                std::cout << fail << ", ";
            std::cout << "\n";
            LOG("Failed witness count: ", failed_witness.size());
        }

        if (failed_witness.size() == 0)
            std::cout << "all witness are consistent\n";

    } catch (std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}