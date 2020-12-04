#include <fstream>
#include <iostream>

#include "algorithms/fcfs/fcfs_algorithm.hpp"
#include "algorithms/rr/rr_algorithm.hpp"

#include "simulation/simulation.hpp"
#include "types/enums.hpp"

#include "utilities/flags/flags.hpp"

Simulation::Simulation(FlagOptions flags)
{
    // Hello!
    if (flags.scheduler == "FCFS")
    {
        // Create a FCFS scheduling algorithm
        this->scheduler = std::make_shared<FCFSScheduler>();
    }
    else if (flags.scheduler == "RR")
    {
        // Create a RR scheduling algorithm
        this->scheduler = std::make_shared<RRScheduler>(flags.time_slice);
    }
    this->flags = flags;
    this->logger = Logger(flags.verbose, flags.per_thread, flags.metrics);
}

void Simulation::run()
{
    this->read_file(this->flags.filename);

    while (!this->events.empty())
    {
        auto event = this->events.top();
        this->events.pop();

        // Invoke the appropriate method in the simulation for the given event type.

        switch (event->type)
        {
        case THREAD_ARRIVED:
            this->handle_thread_arrived(event);
            break;

        case THREAD_DISPATCH_COMPLETED:
        case PROCESS_DISPATCH_COMPLETED:
            this->handle_dispatch_completed(event);
            break;

        case CPU_BURST_COMPLETED:
            this->handle_cpu_burst_completed(event);
            break;

        case IO_BURST_COMPLETED:
            this->handle_io_burst_completed(event);
            break;
        case THREAD_COMPLETED:
            this->handle_thread_completed(event);
            break;

        case THREAD_PREEMPTED:
            this->handle_thread_preempted(event);
            break;

        case DISPATCHER_INVOKED:
            this->handle_dispatcher_invoked(event);
            break;
        }

        // If this event triggered a state change, print it out.
        if (event->thread && event->thread->current_state != event->thread->previous_state)
        {
            this->logger.print_state_transition(event, event->thread->previous_state, event->thread->current_state);
        }
        this->system_stats.total_time = event->time;
        event.reset();
    }
    // We are done!

    std::cout << "SIMULATION COMPLETED!\n\n";

    for (auto entry : this->processes)
    {
        this->logger.print_per_thread_metrics(entry.second);
    }

    logger.print_simulation_metrics(this->calculate_statistics());
}

//==============================================================================
// Event-handling methods
//==============================================================================

void Simulation::handle_thread_arrived(const std::shared_ptr<Event> event)
{
    //new thread needs to be added to ready queue
    this->scheduler->add_to_ready_queue(event.thread.set_state(ThreadState::READY, event.time));
    //if the cpu is idle, create a new dispatcher invoked event
    if(this->active_thread == nullptr)
    {
        event.scheduling_decision.explanation = "Thread arrived w/ cpu idle";
        this->events.push(Event(EventType::DISPATCHER_INVOKED, event.time, ++this->event_num, event.thread, event.scheduling_decision));
    }
}

void Simulation::handle_dispatch_completed(const std::shared_ptr<Event> event)
{
    //A thread switch has completed, allowing a new thread to start executing on the CPU
    this->scheduler->get_next_thread().set_state(ThreadState::RUNNING, event.time);
    //get next cpu burst from thread
    auto burst = event.thread.get_next_burst(BurstType::CPU);

    if(this->scheduler->time_slice != -1 && burst.length < this->scheduler->time_slice)
    {
        //preempt thread
        event.scheduling_decision.explanation = "Thread preempted bc length < timeslice.";
        this->events.push(Event(EventType::THREAD_PREEMPTED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));   
    }
    else 
    {
        event.thread.pop_next_burst(BurstType::CPU);
        if(event.thread.get_next_burst(BurstType::CPU) == nullptr) //if this is the last burst...
        {
            //thread completed!
            event.scheduling_decision.explanation = "Thread completed!";
            this->events.push(Event(EventType::THREAD_COMPLETED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
        }
        else //this isn't the last burst
        {
            event.scheduling_decision.explanation = "Cpu burst complete";
            this->events.push(Event(EventType::CPU_BURST_COMPLETED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
        }
    } 
}

void Simulation::handle_cpu_burst_completed(const std::shared_ptr<Event> event)
{
    //A thread has finished one of its CPU bursts and has initiated an I/O request.
    event.thread.set_state(ThreadState::BLOCKED, event.time);
    //pop off cpu burst
    auto burst = event.thread.get_next_burst(BurstType::CPU);
    event.thread.pop_next_burst();
    //update total cpu usage
    this->system_stats.total_cpu_time += burst.length;
    //get io burst
    burst = event.thread.get_next_burst(BurstType::IO);
    //create new io burst completed event and add to queue    
    event.scheduling_decision.explanation = "IO Burst";
    this->events.push(Event(EventType::IO_BURST_COMPLETED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
    //create new dispatcher invoked event
    event.scheduling_decision.explanation += ", Sent to Dispatcher";
    this->events.push(Event(EventType::DISPATCHER_INVOKED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
}

void Simulation::handle_io_burst_completed(const std::shared_ptr<Event> event)
{
    //A thread has finished one of its I/O bursts and is once again ready to be executed.
    this->scheduler->add_to_ready_queue(event.thread.set_state(ThreadState::READY, event.time));
    //add time to io time
    auto burst = event.thread.get_next_burst(BurstType::IO);
    event.thread.pop_next_burst();
    this->system_stats.total_io_time += burst.length;
    //check if the cpu is idle
    if(this->active_thread == nullptr)
    {
        //create new dispatcher invoked event
        event.scheduling_decision.explanation = "IO Burst Complete, Sending to Dispatcher";
        this->events.push(Event(EventType::DISPATCHER_INVOKED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
    }
}

void Simulation::handle_thread_completed(const std::shared_ptr<Event> event)
{
    //A thread has finished the last of its CPU bursts.
    event.thread.set_state(ThreadState::EXIT, event.time);
    //record cpu time and stuff
    auto burst = event.thread.get_next_burst(BurstType::CPU);
    event.thread.pop_next_burst();
    this->system_stats.total_cpu_time += burst.length;

    //is the cpu idle?
    if(this->active_thread == nullptr)
    {
        //invoke dispatcher
        event.scheduling_decision.explanation = "Thread Completed, Sending to Dispatcher";
        this->events.push(Event(EventType::DISPATCHER_INVOKED, this->system_stats.total_time, ++this->event_num, event.thread, event.scheduling_decision));
    }
}

void Simulation::handle_thread_preempted(const std::shared_ptr<Event> event)
{
    //A thread has been preempted during execution of one of its CPU bursts.
    this->scheduler->add_to_ready_queue(event.thread.set_state(ThreadState::READY, event.time));
    //get cpu burst and update total cpu time
    auto burst = event.thread.get_next_burst(BurstType::CPU);
    this->system_stats.total_cpu_time += burst.length - this->scheduler->time_slice;
    burst.update_time(this->scheduler->time_slice);
    //is the cpu idle?
    if(this->active_thread == nullptr)
    {
        event.scheduling_decision.explanation = "Thread preempted, returning to dispatcher.";
        this->events.push(Event(EventType::DISPATCHER_INVOKED, this->system_stats.total_time, ++this->event_num, event->thread, event.scheduling_decision));
    }
}

void Simulation::handle_dispatcher_invoked(const std::shared_ptr<Event> event)
{
    std::shared_ptr<Thread> next_thread = nullptr;
    //the OS dispatcher routine has been invoked to determine the next thread to be run on the CPU
    //cpu idle?
    if(this->active_thread == nullptr)
    {
        //make active thread prev thread
        this->prev_thread = this->active_thread;
    }
    //get next thread
    next_thread = this->scheduler->get_next_thread();
    //is there a next thread?
    if(next_thread == nullptr)
    {
        //set cpu to idle
        active_thread = nullptr;
    }
    else //oh boy lots of stuff
    {
        //make new thread into active thread
        this->active_thread = next_thread;
        SchedulingDecision sd;
        sd.thread = this->active_thread;
        sd.time_slice = this->scheduler->time_slice;
        //is the next thread from the same process?
        if(this->prev_thread->process_id == this->active_thread->process_id)
        {
            //update time spent on dispatch
            this->system_stats.total_time += this->thread_switch_overhead;
            this->system_stats.dispatch_time += this->thread_switch_overhead;
            //create a new thread dispatcher event
            sd.explanation = "Thread dispatched";
            this->events.push(Event(EventType::THREAD_DISPATCH_COMPLETED, this->system_stats.total_time, ++this->event_num, this->active_thread, sd));
        }
        else
        {
            //update time spent on dispatch
            this->system_stats.total_time += this->process_switch_overhead;
            this->system_stats.dispatch_time += this->process_switch_overhead;
            //create a new process dispatch event
            sd.explanation = "Process Dispatched dispatched";
            this->events.push(Event(EventType::PROCESS_DISPATCH_COMPLETED, this->system_stats.total_time, ++this->event_num, this->active_thread, sd));
        }
    }  
}

//==============================================================================
// Utility methods
//==============================================================================

SystemStats Simulation::calculate_statistics()
{
    // TODO: Implement functionality for calculating the simulation statistics
    this->system_stats.cpu_utilization = this->system_stats.total_cpu_time / this->system_stats.total_time;
    this->system_stats.cpu_efficiency = this->system_stats.service_time / this->system_stats.total_time;
    return this->system_stats;
}

void Simulation::add_event(std::shared_ptr<Event> event)
{
    if (event != nullptr)
    {
        this->events.push(event);
    }
}

void Simulation::read_file(const std::string filename)
{
    std::ifstream input_file(filename.c_str());

    if (!input_file)
    {
        std::cerr << "Unable to open simulation file: " << filename << std::endl;
        throw(std::logic_error("Bad file."));
    }

    int num_processes;

    input_file >> num_processes >> this->thread_switch_overhead >> this->process_switch_overhead;

    for (int proc = 0; proc < num_processes; ++proc)
    {
        auto process = read_process(input_file);

        this->processes[process->process_id] = process;
    }
}

std::shared_ptr<Process> Simulation::read_process(std::istream &input)
{
    int process_id, priority;
    int num_threads;

    input >> process_id >> priority >> num_threads;

    auto process = std::make_shared<Process>(process_id, (ProcessPriority)priority);

    // iterate over the threads
    for (int thread_id = 0; thread_id < num_threads; ++thread_id)
    {
        process->threads.emplace_back(read_thread(input, thread_id, process_id, (ProcessPriority)priority));
    }

    return process;
}

std::shared_ptr<Thread> Simulation::read_thread(std::istream &input, int thread_id, int process_id, ProcessPriority priority)
{
    // Stuff
    int arrival_time;
    int num_cpu_bursts;

    input >> arrival_time >> num_cpu_bursts;

    auto thread = std::make_shared<Thread>(arrival_time, thread_id, process_id, priority);

    for (int n = 0, burst_length; n < num_cpu_bursts * 2 - 1; ++n)
    {
        input >> burst_length;

        BurstType burst_type = (n % 2 == 0) ? BurstType::CPU : BurstType::IO;

        thread->bursts.push(std::make_shared<Burst>(burst_type, burst_length));
    }

    this->events.push(std::make_shared<Event>(EventType::THREAD_ARRIVED, thread->arrival_time, this->event_num, thread, nullptr));
    this->event_num++;

    return thread;
}
