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

uint strategy = 1;
uint k = 1;
std::mutex vertexLock;
uint endIndex = 0;
uint nThreads = 0;


struct thread_args {
    Vertex *vertices;
    int max_iter;
    uintV n;
    uintV startIndex;
    uintV endIndex;
    PageRankType* pr_curr;
    PageRankType *pr_next;
    double time_taken;
    long processedVertices;
    long processedEdges;
    double barrier1_time;
    double barrier2_time;
    double t1;
    double t2;
    double t3;
    double t4;
    double t5;
    double t6;

    double getNextVertex_time;
    uint thread_id;
};

uintV getNextProcessedVertex(uintV n){ //rewrite it to use compare and exchange

  uintV previous;
  while (true) {
    previous = nextProcessedVertex.load();
    if (previous >= n) {
        return -1; 
    }
    uintV newVal = previous + k;
    if (nextProcessedVertex.compare_exchange_weak(previous, newVal)) {
        return previous; 
    }
  }
}



void processVertex(Vertex *vertices, uintV startIndexCopy, PageRankType *pr_curr, PageRankType *pr_next, PageRankType in_degree){
          // std::cout<<"Number of in degree"<< in_degree<<std::endl;
        for (uintE i = 0; i < in_degree; i++) {
          uintV u = vertices[startIndexCopy].getInNeighbor(i);
          uintE u_out_degree = vertices[u].getOutDegree();
          if (u_out_degree > 0)
              pr_next[startIndexCopy] += (pr_curr[u] / (PageRankType) u_out_degree);
        }
}

void computePageRank(uintV v, PageRankType *pr_next, PageRankType*pr_curr){
    pr_next[v] = PAGE_RANK(pr_next[v]);
    // reset pr_curer for the next iteration
    pr_curr[v] = pr_next[v];
    pr_next[v] = 0.0;
}

void pageRankThread(thread_args *thread_args){
    // std::cout<<"Inside thread method"<<std::endl;
    timer local;
    timer localBarrier1;
    timer localBarrier2;
    timer localVertex;
    timer bug1;
    timer bug2;
    timer bug3;
    timer bug4;
    timer bug5;
    timer bug6;



    Vertex *vertices = thread_args->vertices; 
    uintV n = thread_args->n; 
    int max_iter = thread_args->max_iter; 
    uintV startIndex  = thread_args->startIndex; 
    uintV endIndex  = thread_args->endIndex; 
    PageRankType* pr_curr  = thread_args->pr_curr; 
    PageRankType *pr_next  = thread_args->pr_next;
    uint processedVertex = 0;
    uint processedEdges = 0;
    double timeVertex = 0;
    uint thread_id = thread_args->thread_id;
    double t1 = 0;
    double t2 = 0;
    double t3 = 0;
    double t4 = 0;
    double t5 = 0;
    double t6 = 0;



  local.start();
  for (int iter = 0; iter < max_iter; iter++) {
    // for each vertex 'v' in this subset of vertices, process all its inNeighbors 'u'
    if(strategy == 3 or strategy == 4){
      while(true){
        localVertex.start();
        uintV u  = getNextProcessedVertex(n);
        // std::cout<<"this is new thread processed "<<u<<std::endl;
        timeVertex +=  localVertex.stop();
        // break;
        bug1.start();
        if(u == -1 ){
          break;
        }
        
        for (uintV j = 0; j < k; j++) {
            // std::cout<<u<<std::endl;
          PageRankType in_degree = vertices[u].getInDegree();
          processedEdges += in_degree;
          processVertex(vertices,u,pr_curr,pr_next,in_degree);
          u++;
          if(u >= n) break;
        }
        t1 += bug1.stop();
      }
    }
    else {
      bug1.start();
      for (uintV startIndexCopy = startIndex; startIndexCopy < endIndex; startIndexCopy++){
        PageRankType in_degree = vertices[startIndexCopy].getInDegree();
        processedEdges += in_degree;
        processVertex(vertices,startIndexCopy,pr_curr,pr_next, in_degree);
      }
      t1 += bug1.stop();
    }
    // if (done.fetch_add(1) == nThreads - 1) {
    //   nextProcessedVertex.store(0);
    //   done.store(0);
    // }

    localBarrier1.start();
    barrier->wait();
    thread_args->barrier1_time +=  localBarrier1.stop();
  
    // std::cout<<"Done Waiting at the barrier"<<std::endl;
    if(strategy == 3 or strategy == 4){
      bug2.start();
      if(thread_id == 0){
        nextProcessedVertex.store(0);
      }
      t2 += bug2.stop();

      barrier->wait();
      while(true){
        localVertex.start();
        // std::cout<<"this is new thread processed 2nd loop before "<<nextProcessedVertex<<std::endl;
        uintV v  = getNextProcessedVertex(n);
        // std::cout<<"this is new thread processed 2nd loop "<<v<<std::endl;
        timeVertex +=  localVertex.stop();
        bug4.start();
        // break;;
        if( v == -1){
          break;
        }bug4.start()
        for (uintV j = 0; j < k; j++) {
            // std::cout<<u<<std::endl;
          bug5.start();
          // computePageRank(v,pr_next, pr_curr);
          pr_next[v] = PAGE_RANK(pr_next[v]);
          // reset pr_curer for the next iteration
          pr_curr[v] = pr_next[v];
          pr_next[v] = 0.0;
          processedVertex ++;
          t5 += bug5.stop();

          bug6.start();
          //vertices_processed += 1 // used in output validation
          v++;
          if(v >= n) break;
          t6 = bug6.stop();
        }
        t4 += bug4.stop();
      }
    }
    else {
        bug4.start();
      for (uintV v = startIndex; v < endIndex; v++) {
          bug5.start();
        
        computePageRank(v,pr_next, pr_curr);
        processedVertex ++;
          t5 += bug5.stop();

      }
      t4 += bug4.stop();
    }
   
    // if (done.fetch_add(1) == nThreads - 1) {
    //       nextProcessedVertex.store(0);
    //       done.store(0);
    //   }
    localBarrier2.start();
    barrier->wait();
    thread_args->barrier2_time +=  localBarrier2.stop();
    bug3.start();
  
    if(strategy == 3 or strategy == 4){
      if(thread_id == 0){
        nextProcessedVertex.store(0,std::memory_order_relaxed);
        //  std::cout<<"making it 0 "<<nextProcessedVertex<<" "<<done<<std::endl;
        //  std::cout<<"making it 0 "<<nextProcessedVertex<<" "<<done<<std::endl;
      }
    }
      barrier->wait();
      t3 += bug3.stop();

  }
  // std::cout <<"this is total processed"<<processedVertex<<std::endl;
  thread_args->time_taken = local.stop();
  thread_args->processedVertices = processedVertex;
  thread_args->processedEdges = processedEdges;
  thread_args->getNextVertex_time = timeVertex;
  thread_args->t1 = t1;
  thread_args->t2 = t2;
  thread_args->t3 = t3;
  thread_args->t4 = t4;
  thread_args->t5 = t5;
  thread_args->t6 = t6;


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
  uint remainder = n% nThreads;
  uint numOfEdgesPerThread = m/nThreads;
  uint totalAssignedEdges = 0;
  uint startIndex = 0;
  uint endIndex = 0;
  Vertex * vertices = g.vertices_;
  t1.start();
  // std::cout<<"Total vertices:"<<n<<std::endl;
  // std::cout<<"Total threads:"<<nThreads<<std::endl;


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
      // edge assignment
      int target = (i + 1) * numOfEdgesPerThread;
      while(totalAssignedEdges < target){
        totalAssignedEdges += vertices[endIndex].in_degree_;
        endIndex ++;
      }
      if(i == nThreads -1 && totalAssignedEdges < m ){
          endIndex = n;
      }
    }
  
    // std::cout<<"StartInd: "<< startIndex<<"EndInd: "<<endIndex<< "Thread: "<<i<<std::endl;

    all_arguments[i].vertices = vertices;
    all_arguments[i].max_iter = max_iters;
    all_arguments[i].startIndex = startIndex;
    all_arguments[i].endIndex = endIndex;
    all_arguments[i].pr_curr = pr_curr;
    all_arguments[i].pr_next = pr_next;
    all_arguments[i].n = n;
    all_arguments[i].processedVertices = 0;
    all_arguments[i].processedEdges = 0;
    all_arguments[i].getNextVertex_time = 0.0;
    all_arguments[i].barrier1_time = 0.0;
    all_arguments[i].barrier2_time = 0.0;
    all_arguments[i].t1 = 0.0;
    all_arguments[i].t2 = 0.0;
    all_arguments[i].t3 = 0.0;
    all_arguments[i].t4 = 0.0;
    all_arguments[i].t5 = 0.0;
    all_arguments[i].t6 = 0.0;

    all_arguments[i].thread_id = i;
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
    std::cout << i<<", " << all_arguments[i].processedVertices<<", " << all_arguments[i].processedEdges<<", " << all_arguments[i].barrier1_time<<", " << all_arguments[i].barrier2_time<<", " << all_arguments[i].getNextVertex_time<<", " << all_arguments[i].time_taken <<", " << all_arguments[i].t1 <<", " << all_arguments[i].t2 <<", " << all_arguments[i].t3 <<", " << all_arguments[i].t4 <<", " << all_arguments[i].t5 <<", " << all_arguments[i].t6 <<std::endl;
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
  if (strategy != 1 && strategy != 2 && strategy != 3 && strategy != 4) {
    strategy = 1;
  }
  if(k <= 0 || k != (int)k ){
    k = 1;
  }
  if(strategy != 4){
    k = 1;
  }


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
