/** CPUs.c
 * ===========================================================
 * Name: Rust III, John
 * Section: M4
 * Project: PEX2 - CPU Scheduling Simulator
 * Purpose: Implements six CPU scheduling algorithms as POSIX threads.
 *          Each thread follows the same pattern every timestep:
 *            1. Wait on its per-CPU semaphore (posted by main).
 *            2. Optionally preempt the current process back to readyQ.
 *            3. Select a new process from readyQ if idle.
 *            4. Decrement burstRemaining by one (one unit of execution).
 *            5. Move the process to finishedQ if it is complete.
 *            6. Post to mainSem to signal the clock thread.
 *          All accesses to readyQ and finishedQ are protected by their
 *          respective mutex locks.
 * ===========================================================
 * Documentation Statement: <describe any help received>
 * 
 * =========================================================== */

#include <stdio.h>
#include <limits.h>
#include "CPUs.h"
#include "processQueue.h"

// ============================================================
// FIFO — First In First Out (non-preemptive)
// Runs each process to completion; always selects the head of
// the ready queue (the process that arrived earliest).
//
// Synchronization overview (same pattern used by all algorithms):
//   - main() is the clock driver. Each tick it posts cpuSems[i] to
//     wake this thread, then waits on mainSem for this thread to reply.
//   - This thread does one unit of work per tick, then posts mainSem
//     so main can advance the clock to the next timestep.
//   - readyQ and finishedQ are shared with main and other CPU threads,
//     so every access must be wrapped in the corresponding mutex lock.
// ============================================================
void* FIFOcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    // p is the process currently running on this CPU.
    // p == NULL means the CPU is idle and must pick a new process from readyQ.
    Process* p = NULL;

    // This thread runs forever — one loop iteration = one simulation timestep.
    while (1) {
        // ── Sync point 1: wait for main to start this timestep ──────────
        // main() posts cpuSems[threadNum] once per tick.  We block here
        // until that post arrives, keeping this CPU in lockstep with the clock.
        sem_wait(svars->cpuSems[threadNum]);

        // ── Selection (only when idle) ───────────────────────────────────
        // FIFO is non-preemptive: once a process is running (p != NULL) we
        // never replace it mid-burst.  We only enter this block when the CPU
        // has nothing to run.
        if (p == NULL) {
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // Index 0 = head of the list = the process that has been waiting
            // the longest (qInsert always appends to the tail, so the head is
            // always the oldest arrival — that is the FIFO selection rule).
            p = qRemove(&(svars->readyQ), 0);

            if (p == NULL) {
                // readyQ was empty — CPU stays idle this tick.
                printf("No process to schedule\n");
            } else {
                printf("Scheduling PID %d\n", p->PID);
            }

            pthread_mutex_unlock(&(svars->readyQLock));
        }

        // ── Execution: one unit of work ──────────────────────────────────
        // If we have a process (carried over from a prior tick or just
        // selected above), burn one unit of its remaining CPU burst.
        if (p != NULL) {
            p->burstRemaining--;

            if (p->burstRemaining == 0) {
                // Process is done — // readyQ was empty — CPU stays idle this tick.move it to finishedQ so main can
                // compute and print wait-time statistics at simulation end.
                pthread_mutex_lock(&(svars->finishedQLock));
                qInsert(&(svars->finishedQ), p);
                pthread_mutex_unlock(&(svars->finishedQLock));

                // CPU is now idle; it will select a new process next tick.
                p = NULL;
            }
        }

        // ── Sync point 2: signal main that this CPU is done ─────────────
        // main() waits on mainSem once per CPU per tick.  Posting here
        // tells main this CPU has finished its work for the current timestep.
        sem_post(svars->mainSem);
    }
}

// ============================================================
// SJF — Shortest Job First (non-preemptive)
// Runs each process to completion; selects the process with the
// smallest burstRemaining (equals burstTotal for unscheduled processes).
// ============================================================
void* SJFcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    // p is the process currently running on this CPU.
    // p == NULL means the CPU is idle and must pick a new process from readyQ.
    Process* p = NULL;

    // This thread runs forever — one loop iteration = one simulation timestep.
    while (1) {
        // ── Sync point 1: wait for main to start this timestep ──────────
        // main() posts cpuSems[threadNum] once per tick.  We block here
        // until that post arrives, keeping this CPU in lockstep with the clock.
        sem_wait(svars->cpuSems[threadNum]);

        // ── Selection (only when idle) ───────────────────────────────────
        // SJF is non-preemptive: once a process is running (p != NULL) we
        // never replace it mid-burst.  We only enter this block when the CPU
        // has nothing to run.
        if (p == NULL) {
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // qShortest returns the position with the shortest burstRemianing
            // out of all the processes in ReadyQ (that is the SJF selection rule).
            p = qRemove(&(svars->readyQ), qShortest(&(svars->readyQ)));

            if (p == NULL) {
                // readyQ was empty — CPU stays idle this tick.
                printf("No process to schedule\n");
            } else {
                printf("Scheduling PID %d\n", p->PID);
            }

            pthread_mutex_unlock(&(svars->readyQLock));
        }

        // ── Execution: one unit of work ──────────────────────────────────
        // If we have a process (carried over from a prior tick or just
        // selected above), burn one unit of its remaining CPU burst.
        if (p != NULL) {
            p->burstRemaining--;

            if (p->burstRemaining == 0) {
                // Process is done — // readyQ was empty — CPU stays idle this tick.move it to finishedQ so main can
                // compute and print wait-time statistics at simulation end.
                pthread_mutex_lock(&(svars->finishedQLock));
                qInsert(&(svars->finishedQ), p);
                pthread_mutex_unlock(&(svars->finishedQLock));

                // CPU is now idle; it will select a new process next tick.
                p = NULL;
            }
        }

        // ── Sync point 2: signal main that this CPU is done ─────────────
        // main() waits on mainSem once per CPU per tick.  Posting here
        // tells main this CPU has finished its work for the current timestep.
        sem_post(svars->mainSem);
    }
}

// ============================================================
// NPP — Non-Preemptive Priority
// Runs each process to completion; selects the process with the
// highest priority (lowest-numbered priority value) from the queue.
// Remember: lower priority number = higher priority.
// ============================================================
void* NPPcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    // p is the process currently running on this CPU.
    // p == NULL means the CPU is idle and must pick a new process from readyQ.
    Process* p = NULL;

    // This thread runs forever — one loop iteration = one simulation timestep.
    while (1) {
        // ── Sync point 1: wait for main to start this timestep ──────────
        // main() posts cpuSems[threadNum] once per tick.  We block here
        // until that post arrives, keeping this CPU in lockstep with the clock.
        sem_wait(svars->cpuSems[threadNum]);

        // ── Selection (only when idle) ───────────────────────────────────
        // NPP is non-preemptive: once a process is running (p != NULL) we
        // never replace it mid-burst.  We only enter this block when the CPU
        // has nothing to run.
        if (p == NULL) {
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // qPriority returns the position with the highest-priority process (lowest
            // number) in the ReadyQ (that is the NNP selection rule).
            p = qRemove(&(svars->readyQ), qPriority(&(svars->readyQ)));

            if (p == NULL) {
                // readyQ was empty — CPU stays idle this tick.
                printf("No process to schedule\n");
            } else {
                printf("Scheduling PID %d\n", p->PID);
            }

            pthread_mutex_unlock(&(svars->readyQLock));
        }

        // ── Execution: one unit of work ──────────────────────────────────
        // If we have a process (carried over from a prior tick or just
        // selected above), burn one unit of its remaining CPU burst.
        if (p != NULL) {
            p->burstRemaining--;

            if (p->burstRemaining == 0) {
                // Process is done — // readyQ was empty — CPU stays idle this tick.move it to finishedQ so main can
                // compute and print wait-time statistics at simulation end.
                pthread_mutex_lock(&(svars->finishedQLock));
                qInsert(&(svars->finishedQ), p);
                pthread_mutex_unlock(&(svars->finishedQLock));

                // CPU is now idle; it will select a new process next tick.
                p = NULL;
            }
        }

        // ── Sync point 2: signal main that this CPU is done ─────────────
        // main() waits on mainSem once per CPU per tick.  Posting here
        // tells main this CPU has finished its work for the current timestep.
        sem_post(svars->mainSem);
    }
}

// ============================================================
// RR — Round Robin (quantum-based preemption)
// Runs a process for at most 'quantum' timesteps before requeuing
// it; always selects from the head of the ready queue.
// ============================================================
void* RRcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    int num_timesteps = 0;
    bool out_of_quantum = false;

    // p is the process currently running on this CPU.
    // p == NULL means the CPU is idle and must pick a new process from readyQ.
    Process* p = NULL;
    Process* p_requeue = NULL;


    // This thread runs forever — one loop iteration = one simulation timestep.
    while (1) {
        // ── Sync point 1: wait for main to start this timestep ──────────
        // main() posts cpuSems[threadNum] once per tick.  We block here
        // until that post arrives, keeping this CPU in lockstep with the clock.
        sem_wait(svars->cpuSems[threadNum]);

        // ── Selection (only when idle) ───────────────────────────────────
        // NPP is non-preemptive: once a process is running (p != NULL) we
        // never replace it mid-burst.  We only enter this block when the CPU
        // has nothing to run.
        if (p == NULL) {
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // qPriority returns the position with the highest-priority process (lowest
            // number) in the ReadyQ (that is the NNP selection rule).
            
            if (out_of_quantum) {
                p_requeue->requeued = true;
                qInsert(&(svars->readyQ), p_requeue);
                out_of_quantum = false;
            }
            p = qRemove(&(svars->readyQ), 0);

            if (p == NULL) {
                // readyQ was empty — CPU stays idle this tick.
                printf("No process to schedule\n");
            } else {
                printf("Scheduling PID %d\n", p->PID);
            }

            pthread_mutex_unlock(&(svars->readyQLock));
        }

        // ── Execution: one unit of work ──────────────────────────────────
        // If we have a process (carried over from a prior tick or just
        // selected above), burn one unit of its remaining CPU burst.
        if (p != NULL) {
            p->burstRemaining--;
            num_timesteps++;

            if (p->burstRemaining == 0) {
                // Process is done — // readyQ was empty — CPU stays idle this tick.move it to finishedQ so main can
                // compute and print wait-time statistics at simulation end.
                pthread_mutex_lock(&(svars->finishedQLock));
                qInsert(&(svars->finishedQ), p);
                pthread_mutex_unlock(&(svars->finishedQLock));

                // CPU is now idle; it will select a new process next tick.
                p = NULL;

                num_timesteps = 0;
            }
            else if (num_timesteps == svars->quantum && p->burstRemaining != 0) { //
                p_requeue = p;
                p = NULL;
                num_timesteps = 0;
                out_of_quantum = true;
            }
        }

        // ── Sync point 2: signal main that this CPU is done ─────────────
        // main() waits on mainSem once per CPU per tick.  Posting here
        // tells main this CPU has finished its work for the current timestep.
        sem_post(svars->mainSem);
    }
}

// ============================================================
// SRTF — Shortest Remaining Time First (preemptive)
// Preempts the running process whenever a shorter job is in the
// ready queue; selects the process with the smallest burstRemaining.
// ============================================================
void* SRTFcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    // p is the process currently running on this CPU.
    // p == NULL means the CPU is idle and must pick a new process from readyQ.
    Process* p = NULL;

    // This thread runs forever — one loop iteration = one simulation timestep.
    while (1) {
        // ── Sync point 1: wait for main to start this timestep ──────────
        // main() posts cpuSems[threadNum] once per tick.  We block here
        // until that post arrives, keeping this CPU in lockstep with the clock.
        sem_wait(svars->cpuSems[threadNum]);

        // ── Selection (each time) ───────────────────────────────────
        // SRTF is preemptive: replacement can occur after each time tick cycle.
        // Preemption occurs when a shorter job arrives in the queue.
        
        // Checks for initial selection or when the CPU is empty and selects correct process
        if (p == NULL) {
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // qShortest selects the process with the shortest reminaing burst time left
            // to run and removes it from the ReadyQ
            p = qRemove(&(svars->readyQ), qShortest(&(svars->readyQ)));

            if (p == NULL) {
                // readyQ was empty — CPU stays idle this tick.
                printf("No process to schedule\n");
            } else {
                printf("Scheduling PID %d\n", p->PID);
            }

            pthread_mutex_unlock(&(svars->readyQLock));
        }
        // Checks to see if a process in the ReadyQ has a shorter remaining burst time left
        else if (p->burstRemaining > qShortestBR(&(svars->readyQ))) {
            p->requeued = true;
            
            // Lock readyQ before inspecting or modifying it — another CPU
            // thread (or main inserting a new arrival) could touch it right now.
            pthread_mutex_lock(&(svars->readyQLock));

            // Puts the current process back into the ReadyQ while grabbing to the current
            // shortest time remaining process to run and removing it from the ReadyQ
            qInsert(&(svars->readyQ), p);
            p = qRemove(&(svars->readyQ), qShortest(&(svars->readyQ)));

            printf("Scheduling PID %d\n", p->PID);

            pthread_mutex_unlock(&(svars->readyQLock));
        }

        // ── Execution: one unit of work ──────────────────────────────────
        // If we have a process (carried over from a prior tick or just
        // selected above), burn one unit of its remaining CPU burst.
        if (p != NULL) {
            p->burstRemaining--;

            if (p->burstRemaining == 0) {
                // Process is done — // readyQ was empty — CPU stays idle this tick.move it to finishedQ so main can
                // compute and print wait-time statistics at simulation end.
                pthread_mutex_lock(&(svars->finishedQLock));
                qInsert(&(svars->finishedQ), p);
                pthread_mutex_unlock(&(svars->finishedQLock));

                // CPU is now idle; it will select a new process next tick.
                p = NULL;
            }
        }

        // ── Sync point 2: signal main that this CPU is done ─────────────
        // main() waits on mainSem once per CPU per tick.  Posting here
        // tells main this CPU has finished its work for the current timestep.
        sem_post(svars->mainSem);
    }
}

// ============================================================
// PP — Preemptive Priority
// Preempts the running process when a higher-priority (lower-
// numbered) process is in the ready queue.
// ============================================================
void* PPcpu(void* param) {
    int threadNum = ((CpuParams*) param)->threadNumber;
    SharedVars* svars = ((CpuParams*) param)->svars;

    // Process* p = NULL;  // TODO: uncomment when you implement this function

    while (1) {
        sem_wait(svars->cpuSems[threadNum]);

        sem_post(svars->mainSem);
    }
}
