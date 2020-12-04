#include "algorithms/rr/rr_algorithm.hpp"

#include <cassert>
#include <stdexcept>
#include <sstream>

/*
    Here is where you should define the logic for the round robin algorithm.
*/
std::queue<Thread> Threads;

RRScheduler::RRScheduler(int slice) {    
    if (slice == -1) {
        throw("RR must have a timeslice > -1");
    }  
    this->time_slice = slice;
}

std::shared_ptr<SchedulingDecision> RRScheduler::get_next_thread() {
    if(Threads.empty())
    {
        return nullptr;
    }
    else
    {
        Thread temp = Threads.front();
        Threads.pop();
        return temp;
    }
}

void RRScheduler::add_to_ready_queue(std::shared_ptr<Thread> thread) {
    Threads.pop(thread);
}

size_t RRScheduler::size() const {
    return Threads.size();
}
