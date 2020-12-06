#include "algorithms/fcfs/fcfs_algorithm.hpp"

#include <cassert>
#include <stdexcept>
#include <sstream>

#define FMT_HEADER_ONLY
#include "utilities/fmt/format.h"

/*
    Here is where you should define the logic for the FCFS algorithm.
*/

FCFSScheduler::FCFSScheduler(int slice) {
    if (slice != -1) {
        throw("FCFS must have a timeslice of -1");
    }    
}

std::shared_ptr<SchedulingDecision> FCFSScheduler::get_next_thread() {
    std::stringstream ss;
    if(threads.empty())
    {
        return nullptr;
    }
    else
    {
        std::shared_ptr<SchedulingDecision> sd = std::make_shared<SchedulingDecision>();
        sd->thread = threads.front();
        sd->time_slice = this->time_slice;
        ss << "Selected from " << this->size() << " threads. Will run to completion of burst.";
        threads.pop();
        sd->explanation = ss.str();
        return sd;
    }

}

void FCFSScheduler::add_to_ready_queue(std::shared_ptr<Thread> thread) {
    threads.push(thread);
}

size_t FCFSScheduler::size() const {
    return threads.size();
}
