#pragma once

namespace tf {

// ----------------------------------------------------------------------------
// taskflow
// ----------------------------------------------------------------------------

class Algorithm;
class Node;
class Graph;
class FlowBuilder;
class Semaphore;
class Subflow;
class Runtime;
class Task;
class TaskView;
class Taskflow;
class Topology;
class TopologyBase;
class Executor;
class Worker;
class WorkerView;
class ObserverInterface;
class ChromeTracingObserver;
class TFProfObserver;
class TFProfManager;

template <typename T>
class Future;

template <typename...Fs>
class Pipeline;

// ----------------------------------------------------------------------------
// cudaFlow
// ----------------------------------------------------------------------------
class cudaFlowNode;
class cudaFlowGraph;
class cudaTask;
class cudaFlow;
class cudaFlowCapturer;
class cudaFlowOptimizerBase;
class cudaFlowLinearOptimizer;
class cudaFlowSequentialOptimizer;
class cudaFlowRoundRobinOptimizer;

template <typename C, typename D>
class cudaGraphExecBase;

// ----------------------------------------------------------------------------
// struct 
// ----------------------------------------------------------------------------
struct TaskParams;
struct DefaultTaskParams;


}  // end of namespace tf -----------------------------------------------------




