#include <cmath>
#include <cassert>
#include <cstring>
#include <sys/time.h>

#include <iostream>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <vector>
#include <unordered_map>

#include "kraken.h"

#define FEATUREVECTORS_FILE "../../1000genomes_import/chr22_feature.vectors"
#define GENES_FILE "../../1000genomes_import/genes.txt"
#define POWER_DATASET_FILE "../../power_import/DEBS2012-ChallengeData.txt"

static double gettime(void) {
  struct timeval now_tv;
  gettimeofday (&now_tv,NULL);
  return ((double)now_tv.tv_sec) + ((double)now_tv.tv_usec) / 1000000.0;
}

// determines the statistical average (mean) of a given sequence of doubles.
static double getaverage(double* runtimes, size_t n) {
  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    sum += runtimes[i];
  }

  return (sum / n);
}

// determines the standard deviation of a given sequence of doubles.
static double getstddev(double* runtimes, size_t n) {
  double avg = 0.0;
  double std_dev = 0.0;

  for (size_t i = 0; i < n; ++i) {
    avg += runtimes[i];
  }
  avg = (avg / n);

  for (size_t i = 0; i < n; ++i) {
    std_dev += (runtimes[i] - avg) * (runtimes[i] - avg);
  }
  std_dev = std_dev / n;
  std_dev = sqrt(std_dev);

  return std_dev;
}

int main(int argc, char* argv[]) {
  if (argc < 4) {
    std::cout << "Usage: " << argv[0] << " num_elements num_dimensions distribution(0=normal, 1=clustered, 2=uniform, 3=gmrqb, 4=power)" << std::endl;
    return 1;
  }

  size_t n = atoi(argv[1]);
  size_t m = atoi(argv[2]);
  size_t threads = 1;
  int o = 1;
  float selectivity = 0.5;

  std::cout << "INFO: " << n << " vectors, " << m << " dimensions." << std::endl;

  if (atoi(argv[3]) == 2 && argc == 5) {
    selectivity = atof(argv[4]);
    std::cout << "Query Selectivity per Dimension: " << selectivity << std::endl;
  }

  std::vector< std::vector<float> > data_points(n, std::vector<float>(m));
  ctpl::thread_pool *hscan_pool, *vscan_pool;
  KrakenIndex* index;
  if (argc == 6) {
	  hscan_pool = new ctpl::thread_pool(atoi(argv[5]));
          threads = atoi(argv[5]);
  } else {
	  hscan_pool = new ctpl::thread_pool(m);
	  threads = std::thread::hardware_concurrency();
  }
  index = create_kraken(m, threads);
  vscan_pool = new ctpl::thread_pool(threads);
  std::cout << "THREADS: " << threads << std::endl;

  if (atoi(argv[3]) == 3) {
    size_t i = 0;
    std::ifstream feature_vectors(FEATUREVECTORS_FILE);
    std::string line;
    std::string token;

    while (std::getline(feature_vectors, line) && i < n) {
      std::vector<float> data_point(m);
      std::vector<std::string> line_tokens;
      std::istringstream iss(line);
      while(std::getline(iss, token, ' '))
        line_tokens.push_back(token);
      // parse dimensions
      for (size_t j = 0; j < m; ++j)
        data_point[j] = stof(line_tokens[j]);
      data_points[i++] = data_point;
    }
  } else if(atoi(argv[3]) == 4) {
    size_t i = 0;
    std::ifstream tuples(POWER_DATASET_FILE);
    std::string line;
    std::string token;

    while (std::getline(tuples, line) && i < n) {
      std::vector<float> data_point(3);
      std::vector<std::string> line_tokens;
      std::istringstream iss(line);
      while(std::getline(iss, token, '\t'))
        line_tokens.push_back(token);
      // parse attributes
      for (size_t j = 1; j < 4; ++j)
        data_point[j-1] = stof(line_tokens[j]);
      data_points[i++] = data_point;
    }
    std::cout << "Loaded " << n << " tuples from the POWER data set." << std::endl;
  } else if(atoi(argv[3]) == 1) {
    if (argc != 5) {
      std::cout << "ERROR: Please specify a data source for the subclustered experiment!" << std::endl;
      return 0;
    }
    size_t i = 0;
    std::ifstream tuples(argv[4]);
    std::string line;
    std::string token;

    while (std::getline(tuples, line) && i < n) {
      std::vector<float> data_point(m);
      std::vector<std::string> line_tokens;
      std::istringstream iss(line);
      while(std::getline(iss, token, ','))
        line_tokens.push_back(token);
      // parse attributes
      for (size_t j = 0; j < m; ++j)
        data_point[j] = stof(line_tokens[j]);
      data_points[i++] = data_point;
    }
    std::cout << "Loaded " << n << " tuples from '" << argv[4] << "'." << std::endl;
  } else {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> nd(0.5,0.5);
    std::uniform_real_distribution<double> ud(0,o);

    for (size_t i = 0; i < n; ++i) {
      std::vector<float> data_point(m);
      for (size_t j = 0; j < m; ++j) {
        if (atoi(argv[3]) == 0)
          data_point[j] = (float) nd(gen);
        else
          data_point[j] = (float) ((rand() % (o * 1000000)) / 1000000.0);
      }
      data_points[i] = data_point;
    }
  }

  // random insertion order
  std::random_shuffle(data_points.begin(), data_points.end());

  double start = gettime();
  double* runtimes;

  runtimes = new double[n];
  std::cout<<"Scan [inserts]"<<std::endl;
  for (size_t i = 0; i < n; ++i)
  {
    start = gettime();
	insert(index, data_points[i]);
	runtimes[i] = (gettime() - start) * 1000000;
  }
  std::cout<< "Mean: "<<getaverage(runtimes,n)<< " Standard Deviation: " <<getstddev(runtimes,n)<<std::endl;
  delete runtimes;


  std::cout<<"Scan [Exact Search]"<<std::endl;
  runtimes = new double[n];
   for (size_t i = 0; i < n; ++i)
  {
    start = gettime();
	std::vector<float> results = data_points[i];
	runtimes[i] = (gettime() - start) * 1000000;
  }
  std::cout<< "Mean: "<<getaverage(runtimes,n)<< " Standard Deviation: " <<getstddev(runtimes,n)<<std::endl;
  delete runtimes;
  
  int avg_result_size = 0;
  size_t repeat = 1;
  // Generate rq queries
  int rq = 1000;
  std::vector<std::vector<float> > lb_queries(rq, std::vector<float>(m, std::numeric_limits<float>::min()));
  std::vector<std::vector<float> > ub_queries(rq, std::vector<float>(m, std::numeric_limits<float>::max()));
  if (atoi(argv[3]) == 3) {
    rq = 100;
    size_t i = 0;
    std::ifstream queries(argv[4]);
    std::cout << "Loading queries from '" << argv[4] << "'" << std::endl;
    std::string line;
    std::string token;

    while (std::getline(queries, line) && i < rq) {
      std::vector<std::string> line_tokens;
      std::istringstream iss(line);
      while(std::getline(iss, token, ' '))
        line_tokens.push_back(token);
      for (size_t j = 0; j < m; ++j) {
        if (line_tokens[j] != "min") {
          lb_queries[i][j] = (float) std::stof(line_tokens[j]);
        }
      }
      std::getline(queries, line);
      std::vector<std::string> line_tokens2;
      std::istringstream iss2(line);
      while(std::getline(iss2, token, ' '))
        line_tokens2.push_back(token);
      for (size_t j = 0; j < m; ++j) {
        if (line_tokens2[j] != "max")
          ub_queries[i][j] = (float) std::stof(line_tokens2[j]);
      }
      i++;
    }
  } else {
    for (size_t i = 0; i < rq; ++i) {
          int first  = rand() % n;
          int second = rand() % n;
          for (size_t j = 0; j < m; ++j) {
                lb_queries[i][j] = std::min(data_points[first][j], data_points[second][j]);
                ub_queries[i][j] = std::max(data_points[first][j], data_points[second][j]);
                if (argc == 5 && atoi(argv[3]) == 2) {
			lb_queries[i][j] = (float) ((rand() % (o * 10000)) / 1000000.0);
			ub_queries[i][j] = lb_queries[i][j] + o*selectivity;
                }
          }
    }
  }

 
  
  avg_result_size = 0;
 // start = gettime();
  runtimes = new double[rq*repeat];
  std::cout<<"Scan range queries"<<std::endl;
  for (size_t r = 0; r < repeat; ++r)  {
    for (size_t i = 0; i < rq; ++i) {
	 start = gettime();
     avg_result_size += partitioned_range_simd(index, vscan_pool, lb_queries[i], ub_queries[i], threads).size();
	 runtimes[r+i] = (gettime() - start) * 1000000;
    }
  }
  std::cout<< "Mean: "<<getaverage(runtimes,rq*repeat)<< " Standard Deviation: " <<getstddev(runtimes,rq*repeat)<<std::endl;
  delete runtimes;
  
  printf("MDRQ Throughput (multi-threaded/Vertical Partitioning/SIMD): %f ops/s [avg result size: %f].\n",
         (float) ((rq*repeat) / (gettime() - start)),
         (float) (avg_result_size / (float) (rq*repeat)));

  avg_result_size = 0;
  start = gettime();
  for (size_t r = 0; r < repeat; ++r)  {
    for (size_t i = 0; i < rq; ++i) {
      avg_result_size += parallel_simd_scan(index, hscan_pool, lb_queries[i], ub_queries[i]).size();
    }
  }
  printf("MDRQ Throughput (multi-threaded/Horizontal Partitioning/SIMD): %f ops/s [avg result size: %f].\n",
         (float) ((rq*repeat) / (gettime() - start)),
         (float) (avg_result_size / (float) (rq*repeat)));

  delete index, hscan_pool, vscan_pool;

  return 0;
}
