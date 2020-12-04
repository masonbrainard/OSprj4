#include <cassert>
#include <cstddef>
#include <stdexcept>
#include "types/thread/thread.hpp"

void Thread::set_ready(int time) 
{
    //from running
    if(this->previous_state == ThreadState::RUNNING)
    {
        //time cpu was being utilized
        this->service_time += time - this->state_change_time;
    }
    //from blocked
    if(this->previous_state == ThreadState::BLOCKED)
    {
        //time io was being used
        this->io_time += time - this->state_change_time;
    }    
}

void Thread::set_running(int time) {  
   //from ready, and the start time hasn't been set
   if(this->previous_state == ThreadState::READY && this->start_time != -1)
   {
       this->start_time = time;
   }
}

void Thread::set_blocked(int time) {
    //from running
    if(this->previous_state == ThreadState::RUNNING)
    {
        //time cpu was being used
        this->service_time += time - this->state_change_time;
    }
}

void Thread::set_finished(int time) {
    //from running
    if(this->previous_state == ThreadState::RUNNING)
    {
        //time cpu was being used
        this->service_time += time - this->state_change_time;
        //time this process was switched to finished
        this->end_time = time;
    }
}

int Thread::response_time() const {
    return this->start_time;
}

int Thread::turnaround_time() const {
    return this->exit_time - this->start_time;
}

void Thread::set_state(ThreadState state, int time) {
    // TODO
    //first set current state to previous state
    this->previous_state = this->current_state;
    //next set current state
    this->current_state = state;
    //check if theres any special times that need to be considered
    switch(state)
    {
        case ThreadState::NEW :
            Thread::set_new(time);
            break;
        case ThreadState::READY :
            Thread::set_ready(time);
            break;
        case ThreadState::RUNNING :
            Thread::set_running(time);
            break;
        case ThreadState::BLOCKED :
            Thread::set_blocked(time);
            break;
        case ThreadState::EXIT :
            Thread::set_finished(time);
            break;
        default :
            throw("Invalid State");
            break;
    }
    
    //now set time to state_change_time since it happens at every state
    this->state_change_time = time;    
}

std::shared_ptr<Burst> Thread::get_next_burst(BurstType type) {
    if(bursts.empty())
    {
        return nullptr;
    }
    else
    {
        return bursts.front();
    }
}

std::shared_ptr<Burst> Thread::pop_next_burst(BurstType type) {
    if(bursts.empty())
    {
        throw("There aren't any bursts to pop loser.\n")
    }
    if(bursts.front().type != type)
    {
        throw("There aren't any bursts of that type to pop loser!\n")
    }
    else
    {
        bursts.pop();
    }

    return nullptr;
}