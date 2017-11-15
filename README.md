# A NUMA aware multithreaded executor for ROOT

* Proposal: [RE-0004](0001-TNUMAExecutor.md)
* Author: [Xavier Valls](https://github.com/xvallspl)
* Review Manager: TBD
* Status: **Awaiting review**

## Introduction

`TNUMAExecutor`is a NUMA-aware multithreaded executor for ROOT.

root-evolution thread: [Discussion thread topic for that proposal](https://root-forum.cern.ch/c/root-evolution)

## Motivation

Modern multicore/multi socket systems show significant non-uniform memory access (NUMA) effects, such a decrease in performance when accessing memory allocated in a different NUMA node or socket, or threads moving their execution to other sockets.
This new Executor will allow users to overcome this effects in NUMA architectures while using a known programming model, the one of the executors in ROOT.

## Proposed solution

I'm propose a new `TNUMAExecutor`implementing the MapReduce functionality of ROOT's executors by making use of its common interfaces. There is no current solution to this problem in ROOT, although the user may avoid this effects by concealing the execution of their program to a single socket (therefore using a limited amount of the available computing power).

This new executor will take care of partitioning the input data and distributing the partial evaluation on the partitions over the several NUMA nodes, in which each execution will be concealed. Partial results will be retrieved and merged in a final reduction step in the main thread. This will avoid I/O operations across several nodes, in addition to more complex solutions such as providing thread affinity, which would affect negatively performance increases obtained by relying on the work-stealing mechanisms provided by the TBB scheduler.


## Detailed design
An early implementation of `TNUMAexecutor` is available here: https://github.com/xvallspl/NUMAExecutor.

`TNUMAExecutor` implements the MapReduce interface common to ROOT's executors by using two of them: `TProcessExecutor`, which splits the workload and distributes it among the NUMA domains, and `TThreadExecutor`, which executes the work assigned to a NUMA domain in a multithreaded fashion.

In more detail, for each socket of the system, `TTProcessExecutor::MapReduce` executes a lambda that launches a `TThreadExecutor` instance and conceils its execution to the specified NUMA domain. In order to achieve this, `TPRocessExecutor::MapReduce` receives as arguments a callable to evaluate in each one of the `TThreadExecutor`'s tasks, a sequence representing the ID of each domain, and a reduction function to combine the retrieved partial results. This reduction function is propagated for its use by the `TThreadExecutor` instances. The parameter defining `TThreadExecutor`'s execution (data, number of times) is captured by reference by `TProcessExecutor`'s internal lambda.

In order to avoid copying the data in each one of the several forks, `TProcessExecutor` creates `std::array_view` as partitioned representations of the data. This requires another overload in `TThreadExecutor`'s Map and MapReduce operations.

## Source compatibility

This is a new class in ROOT that will not affect existing code. It will remain in ROOT::Experimental for—at least—one release cycle.

## Effect on ABI stability

This proposal will not change the ABI of existing language features.

## Other considerations

This implementation depends on libnuma. It adds another dependency in ROOT which in this case will only be available for GNU/Linux systems.
