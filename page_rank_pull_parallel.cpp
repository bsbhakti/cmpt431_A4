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
#define nullptr nullptr
typedef double PageRankType;
#endif

// create several shared count variables (each should have a portion of the total vertices). each thread has one of the count variables this way there is less contention and some dynamic mapping?

CustomBarrier *barrier = nullptr;
std::atomic<uintV> nextProcessedVertex(0);
std::atomic<int> done(0);
std::vector<uintV> processVertices;

uint strategy = 1;
int k = 1;
uint endIndex = 0;
uint nThreads = 0;


struct thread_args {
    Graph *g;
    int max_iter;
    uintV startIndex;
    uintV endIndex;
    PageRankType* pr_curr;
    PageRankType *pr_next;
    double time_taken;
    uint thread_id;
    long processedVertices;
    long processedEdges;
    double barrier1_time;
    double barrier2_time;
    double getNextVertex_time;
};

uintV getNextProcessedVertex(uintV n){ //rewrite it to use compare and exchange
  uintV curr = nextProcessedVertex.fetch_add(k,std::memory_order_relaxed);

  if(curr >=n){
    // std::cout<<"nThreads:"<<nThreads<<std::endl;
      return -1;
  }
  return curr;
    // uintV previous = nextProcessedVertex.load();
  // uintV previous = nextProcessedVertex.load();
  // uintV newVal = previous + k;
                
  //   while(!nextProcessedVertex.compare_exchange_weak(previous, newVal)){ //if not equal previous is updated 
  //       newVal =  previous + k;
  //   }
  //   if(previous >= n){
  //     return -1;
  //   }
  //   return previous;

}

void incrementThreadsDone(int nThreads){ // increment done. if it is == nthreads. reset nextVertex and done
  int prev = done.fetch_add(1,std::memory_order_relaxed);
  if(prev == nThreads-1){
    nextProcessedVertex.store(0,std::memory_order_relaxed);
    done.store(0,std::memory_order_relaxed);
  }
}

void processVertex(Graph *g, uintV startIndexCopy, PageRankType *pr_curr, PageRankType *pr_next){
  uintE in_degree = g->vertices_[startIndexCopy].getInDegree();
  PageRankType localSum = 0.0; 
          // std::cout<<"Number of in degree"<< in_degree<<std::endl;
  for (uintE i = 0; i < in_degree; i++) {
          uintV u = g->vertices_[startIndexCopy].getInNeighbor(i);
          uintE u_out_degree = g->vertices_[u].getOutDegree();
          if (u_out_degree > 0){
              localSum += (pr_curr[u] / (PageRankType) u_out_degree);
        }
  }
  pr_next[startIndexCopy] += localSum;
}

void computePageRank(uintV v, PageRankType *pr_next, PageRankType*pr_curr){
    pr_next[v] = PAGE_RANK(pr_next[v]);
    // reset pr_curr for the next iteration
    pr_curr[v] = pr_next[v];
    pr_next[v] = 0.0;
}

void pageRankThread(thread_args *thread_args){
    // std::cout<<"Inside thread method"<<std::endl;
    timer local;
    timer localBarrier1;
    timer localBarrier2;
    timer localVertex;

    local.start();
    Graph *g = thread_args->g; 
    uintV n = g->n_; 
    int max_iter = thread_args->max_iter; 
    uintV startIndex  = thread_args->startIndex; 
    uintV endIndex  = thread_args->endIndex; 
    PageRankType* pr_curr  = thread_args->pr_curr; 
    PageRankType *pr_next  = thread_args->pr_next;
    uint thread_id  = thread_args->thread_id;
    uint processedVertex = 0;
    uint processedEdges = 0;



  for (int iter = 0; iter < max_iter; iter++) {
    // for each vertex 'v' in this subset of vertices, process all its inNeighbors 'u'
    if(strategy == 3 or strategy == 4){
      while(true ){
        localVertex.start();
        uintV u  = getNextProcessedVertex(n);
        thread_args->getNextVertex_time +=  localVertex.stop();
        // break;

        if(u == -1 ){
          break;
        }
        uintV end = std::min(u + k, n);
        for (uintV j = u; j < end; j++) {
            // std::cout<<u<<std::endl;
          processedEdges += g->vertices_[j].getInDegree();
          processVertex(g,j,pr_curr,pr_next);
          // u++;
          // if(u >= n) break;
        }
      }
    }
    else {
      for (uintV startIndexCopy = startIndex; startIndexCopy < endIndex; startIndexCopy++){
        processVertex(g,startIndexCopy,pr_curr,pr_next);
      }
    }
    // barrier wait here because we are about the switch curr and next
    incrementThreadsDone(nThreads);
    localBarrier1.start();
    barrier->wait();
    thread_args->barrier1_time +=  localBarrier1.stop();
    // std::cout<<"Done Waiting at the barrier"<<std::endl;
    if(strategy == 3 or strategy == 4){
      // if(thread_id == 0){ //make atomic
      //   nextProcessedVertex = 0; //make it equal to the number of processed vertices??
      // }
      // barrier->wait();
      while(true){
        localVertex.start();
        uintV v  = getNextProcessedVertex(n);
        thread_args->getNextVertex_time +=  localVertex.stop();
        // break;
        if( v == -1){
          break;
        }
        uintV end = std::min(v + k, n);
        for (uintV j = v; j < end; j++) {
            // std::cout<<u<<std::endl;
          computePageRank(j,pr_next, pr_curr);
          processedVertex ++;
          //vertices_processed += 1 // used in output validation
          // v++;
          // if(v >= n) break;
        }
      }

    }
    else {
      for (uintV v = startIndex; v < endIndex; v++) {
        computePageRank(v,pr_next, pr_curr);
      }
    }
    incrementThreadsDone(nThreads);
    localBarrier2.start();
    barrier->wait();
    thread_args->barrier2_time +=  localBarrier2.stop();
    // if(strategy == 3 or strategy == 4){
      // if(thread_id == 0){
      //   nextProcessedVertex.store(0,std::memory_order_relaxed);
      //   //  std::cout<<"making it 0 "<<nextProcessedVertex<<" "<<done<<std::endl;
      // }
      // barrier->wait();
    // }
  }
  thread_args->time_taken = local.stop();
  thread_args->processedVertices = processedVertex;
  thread_args->processedEdges = processedEdges;
}

void pageRankSerial(Graph &g, int max_iters, uint nThreads, uint strategy) {
  uintV n = g.n_;
  uintV m = g.m_;

  PageRankType *pr_curr = new PageRankType[n];
  PageRankType *pr_next = new PageRankType[n];
  std::vector<std::thread> all_threads(nThreads);
  thread_args *all_arguments = new thread_args [nThreads]; 

  for (uintV i = 0; i < n; i++) {
    pr_curr[i] = INIT_PAGE_RANK;
    pr_next[i] = 0.0;
  }

  // Pull based pagerank
  timer t1;
  double time_taken = 0.0;
  
  uint numOfVerPerThread = n/nThreads;
  uint remainder = g.n_% nThreads;
  uint numOfEdgesPerThread = m/nThreads;
  uint totalAssignedEdges = 0;
  uint startIndex = 0;
  uint endIndex = 0;
  Vertex *vertices = g.vertices_;
  t1.start();
  // std::cout<<"Total vertices:"<<n<<std::endl;

  // Create threads and distribute the work across T threads
  // -------------------------------------------------------------------
  for (uint i =0 ; i < nThreads; i++){
    startIndex = endIndex;
    if(strategy == 1){
      //default
      endIndex = startIndex + numOfVerPerThread;
      if( i == 0){
          endIndex += remainder;
      }
    }
    else if(strategy == 2){
      int jump = 10;
      int target = (i + 1) * numOfEdgesPerThread;
      while(totalAssignedEdges < target){
        int chunk = 0;
        for(int j = 0; j < jump && (endIndex + j) < n; j++){
          chunk += vertices[endIndex + j].in_degree_;
        }
        totalAssignedEdges += chunk;
        endIndex +=jump;
        if(target - totalAssignedEdges < jump) {
            jump = 1; // Switch back to single increments near the target
        }
      }
      if(i == nThreads -1 && totalAssignedEdges < m ){
          endIndex = n;
      }
    }
  
    // std::cout<<"StartInd: "<< startIndex<<"EndInd: "<<endIndex<< "Thread: "<<i<<std::endl;

    all_arguments[i].g = &g;
    all_arguments[i].max_iter = max_iters;
    all_arguments[i].startIndex = startIndex;
    all_arguments[i].endIndex = endIndex;
    all_arguments[i].pr_curr = pr_curr;
    all_arguments[i].pr_next = pr_next;
    all_arguments[i].thread_id = i;
    all_arguments[i].processedVertices = 0;
    all_arguments[i].processedEdges = 0;
    all_arguments[i].getNextVertex_time = 0.0;
    all_arguments[i].barrier1_time = 0.0;
    all_arguments[i].barrier2_time = 0.0;
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
  std::cout << "thread_id, num_vertices, num_edges, barrier1_time, barrier2_time, getNextVertex_time, total_time" << std::endl;
  for (int i = 0 ; i<nThreads; i++){
    std::cout << i<<", " << all_arguments[i].processedVertices<<", " << all_arguments[i].processedEdges<<", " << all_arguments[i].barrier1_time<<", " << all_arguments[i].barrier2_time<<", " << all_arguments[i].getNextVertex_time<<", " << all_arguments[i].time_taken << std::endl;
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
  delete[] pr_next;
  delete[] all_arguments;
}



int main(int argc, char *argv[]) {
  cxxopts::Options options(
      "page_rank_pull",
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
           {"granularity", "k value",
           cxxopts::value<uint>()->default_value(DEFAULT_STRATEGY)},
      });

  auto cl_options = options.parse(argc, argv);
  uint n_threads = cl_options["nThreads"].as<uint>();
  uint max_iterations = cl_options["nIterations"].as<uint>();
  std::string input_file_path = cl_options["inputFile"].as<std::string>();
  strategy = cl_options["strategy"].as<uint>();
  k = cl_options["granularity"].as<uint>();
  nThreads = n_threads;

#ifdef USE_INT
  std::cout << "Using INT\n";
#else
  std::cout << "Using DOUBLE\n";
#endif
  std::cout << std::fixed;
  std::cout << "Number of Threads : " << n_threads << std::endl;
  std::cout << "Strategy : " << strategy << std::endl;
  std::cout << "Granularity : " << k << std::endl;
  std::cout << "Iterations: " << max_iterations << std::endl;

  Graph g;
  std::cout << "Reading graph\n";
  g.readGraphFromBinary<int>(input_file_path);
  std::cout << "Created graph\n";
  barrier = new CustomBarrier((int)n_threads);

  pageRankSerial(g, max_iterations, n_threads, strategy);
  delete barrier;

  return 0;
}
