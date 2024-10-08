#include "core/graph.h"
#include "core/utils.h"
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <thread>

#ifdef USE_INT
#define INIT_PAGE_RANK 100000
#define EPSILON 1000
#define PAGE_RANK(x) (15000 + (5 * x) / 6)
#define CHANGE_IN_PAGE_RANK(x, y) std::abs(x - y)
typedef int64_t PageRankType;
#else
#define INIT_PAGE_RANK 1.0
#define EPSILON 0.01
#define DAMPING 0.85
#define DEFAULT_STRATEGY "1"
#define PAGE_RANK(x) (1 - DAMPING + DAMPING * x)
#define CHANGE_IN_PAGE_RANK(x, y) std::fabs(x - y)
typedef double PageRankType;
#endif

CustomBarrier *barrier = nullptr;

struct thread_args {
    Graph *g;
    int max_iter;
    uintV startIndex;
    uintV endIndex;
    PageRankType* pr_curr;
    // PageRankType *pr_next;
    double time_taken;
    std::atomic<PageRankType> *all_atomic;
    int32_t n;
};

void pageRankThread(thread_args *thread_args){
    timer local;
    local.start();
    // std::cout<<"Inside method"<<std::endl;
    Graph *g = thread_args->g; 
    int max_iter = thread_args->max_iter; 
    uintV startIndex  = thread_args->startIndex; 
    uintV endIndex  = thread_args->endIndex; 
    PageRankType* pr_curr  = thread_args->pr_curr; 
    std::atomic<PageRankType> *all_atomic = thread_args->all_atomic;
    

    for (int iter = 0; iter < max_iter; iter++) {
    // for each vertex 'v' in this subset of vertices, process all its inNeighbors 'u'
        for (uintV startIndexCopy = startIndex; startIndexCopy < endIndex; startIndexCopy++){
            Vertex u = g->vertices_[startIndexCopy];
            uintE out_degree = u.getOutDegree();
            PageRankType out_degree_page_rank = (PageRankType) out_degree;
            uintV * outNeighbors = u.getOutNeighbors();

            // std::cout<<"Number of out degree"<< out_degree<<std::endl;
            for (uintE i = 0; i < out_degree; i++) {
                uintV v = outNeighbors[i];
                PageRankType vRank = pr_curr[startIndexCopy];
                PageRankType previous = all_atomic[v].load();
                PageRankType newVal = previous + (vRank / out_degree_page_rank);
                
                while(!all_atomic[v].compare_exchange_weak(previous, newVal)){
                    previous = all_atomic[v].load();
                    newVal = previous + (vRank / out_degree_page_rank);
                }
            }

        }
        
    // barrier wait here because we are about the switch curr and next
    barrier->wait();
    // std::cout<<"Done Waiting at the barrier"<<std::endl;

    for (uintV v = startIndex; v < endIndex; v++) {
      all_atomic[v] = PAGE_RANK(all_atomic[v]);

      // reset pr_curr for the next iteration
      pr_curr[v] = all_atomic[v];
      all_atomic[v] = 0.0;
    }
    barrier->wait();

  }
  thread_args->time_taken = local.stop();
}

void pageRankSerial(Graph &g, int max_iters, uint nThreads, uint strategy) {
  uintV n = g.n_;
  uintV m = g.m_;

  PageRankType *pr_curr = new PageRankType[n];
  std::vector<std::thread> all_threads(nThreads);
  thread_args *all_arguments = new thread_args [nThreads]; 
  std::atomic<PageRankType> *all_atomic = new std::atomic <PageRankType> [g.n_]; 

  for (uintV i = 0; i < n; i++) {
    pr_curr[i] = INIT_PAGE_RANK;
    // pr_next[i] = 0.0;
    all_atomic[i] = 0.0;
  }

  // Push based pagerank
  timer t1;
  double time_taken = 0.0;
  std::cout<<"Total num vertices "<<n<<std::endl;
  std::cout<<"Total num edges "<<m<<std::endl;
  uint numOfVerPerThread = g.n_/nThreads;
  uint remainder = g.n_% nThreads;
  uint numOfEdgesPerThread = m/nThreads;
  uint totalAssignedEdges = 0;
  uint startIndex = 0;
  uint endIndex = 0;
  t1.start();
  // Create threads and distribute the work across T threads
  // -------------------------------------------------------------------

  for (uint i =0 ; i < nThreads; i++){
    startIndex = endIndex;
    if(strategy == 1){
      endIndex = startIndex + numOfVerPerThread;
      if( i == 0){
          endIndex += remainder;
      }
    }
    else if(strategy == 2){
      while(totalAssignedEdges < ((i +1) *numOfEdgesPerThread)){
        totalAssignedEdges += g.vertices_[endIndex].in_degree_;
        endIndex ++;
      }
      if(i == nThreads -1){
        if(m - totalAssignedEdges){
          endIndex = n;
          // totalAssignedEdges +=1 
        }

      }
    // std::cout<<"StartInd: "<< startIndex<<"EndInd: "<<endIndex<< "Thread: "<<" edges assigned:"<<totalAssignedEdges<<i<<std::endl;
    }
    // std::cout<<"StartInd: "<< startIndex<<"EndInd: "<<endIndex<< "Thread: "<<i<<std::endl;
    all_arguments[i].g = &g;
    all_arguments[i].max_iter = max_iters;
    all_arguments[i].startIndex = startIndex;
    all_arguments[i].endIndex = endIndex;
    all_arguments[i].pr_curr = pr_curr;
    all_arguments[i].all_atomic = all_atomic;

    std::thread new_thread(pageRankThread,&all_arguments[i]);
    all_threads.push_back(std::move(new_thread));
  }
  for (auto& thread : all_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
  time_taken = t1.stop();
   // -------------------------------------------------------------------
  std::cout << "thread_id, time_taken" << std::endl;
  for (int i = 0 ; i<nThreads; i++){
    std::cout << i<<", " << all_arguments[i].time_taken << std::endl;
  }

  // Print the above statistics for each thread
  // Example output for 2 threads:
  // thread_id, time_taken
  // 0, 0.12
  // 1, 0.12

  PageRankType sum_of_page_ranks = 0;
  for (uintV u = 0; u < n; u++) {
    sum_of_page_ranks += pr_curr[u];
  }
  std::cout << "Sum of page ranks : " << sum_of_page_ranks << "\n";
  std::cout << "Time taken (in seconds) : " << time_taken << "\n";
  delete[] pr_curr;
//   delete[] pr_next;
  delete[] all_atomic;
  delete[] all_arguments;
}

int main(int argc, char *argv[]) {
  cxxopts::Options options(
      "page_rank_push",
      "Calculate page_rank using serial and parallel execution");
  options.add_options(
      "",
      {
          {"nThreads", "Number of Threads",
           cxxopts::value<uint>()->default_value(DEFAULT_NUMBER_OF_THREADS)},
          {"nIterations", "Maximum number of iterations",
           cxxopts::value<uint>()->default_value(DEFAULT_MAX_ITER)},
          {"inputFile", "Input graph file path",
           cxxopts::value<std::string>()->default_value(
               "/scratch/input_graphs/roadNet-CA")},
          {"strategy", "what algo",
           cxxopts::value<uint>()->default_value(DEFAULT_STRATEGY)},
      });

  auto cl_options = options.parse(argc, argv);
  uint n_threads = cl_options["nThreads"].as<uint>();
  uint max_iterations = cl_options["nIterations"].as<uint>();
  std::string input_file_path = cl_options["inputFile"].as<std::string>();
  uint strategy = cl_options["strategy"].as<uint>();


#ifdef USE_INT
  std::cout << "Using INT" << std::endl;
#else
  std::cout << "Using DOUBLE" << std::endl;
#endif
  std::cout << std::fixed;
  std::cout << "Number of Threads : " << n_threads << std::endl;
  std::cout << "Number of Iterations: " << max_iterations << std::endl;

  Graph g;
  std::cout << "Reading graph\n";
  g.readGraphFromBinary<int>(input_file_path);
  std::cout << "Created graph\n";
  barrier = new CustomBarrier((int)n_threads);
  pageRankSerial(g, max_iterations,n_threads,strategy);
  delete barrier;

  return 0;
}
