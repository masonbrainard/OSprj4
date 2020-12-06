#include "algorithms/rr/rr_algorithm.hpp"

#include <cassert>
#include <stdexcept>
#include <sstream>

/*
    Here is where you should define the logic for the round robin algorithm.
*/

RRScheduler::RRScheduler(int slice) {    
    if (slice == -1) {
        this->time_slice = 3;
    } else {
        this->time_slice = slice;
    }
}

std::shared_ptr<SchedulingDecision> RRScheduler::get_next_thread() {
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
        ss << "Selected from " << this->size() << " threads. Will run for at most " << this->time_slice << " ticks.";
        threads.pop();
        sd->explanation = ss.str();
        return sd;
    }
}

void RRScheduler::add_to_ready_queue(std::shared_ptr<Thread> thread) {
    threads.push(thread);
}

size_t RRScheduler::size() const {
    return threads.size();
}
