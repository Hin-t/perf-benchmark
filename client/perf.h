
#include <thread>
#include <vector>
#include <atomic>

class Perf {

public:
  Perf(int clients, int connections, int requests);
  ~Perf();

  // 启动压测
  void start();
  // 结束压测
  void stop();

  // 获取统计信息
  struct Stats {
    std::atomic<long> total_requests{0};
    std::atomic<long> success_requests{0};
    std::atomic<long> failed_requests{0};
    std::atomic<long long> total_latency_ns{0};

    double average_latency_ms() const {
      if (success_requests == 0)
        return 0.0;
      return (total_latency_ns.load() / 1e6) / success_requests.load();
    }
  };

  const Stats &getStats() const { return stats_; }

  // 设置 RPC 请求任务（函数式接口）
  void setTask(std::function<bool()> task);

private:
  int clients;
  int connections;
  int requesets;

  std::vector<std::thread> workers_;
  std::atomic<bool> running_{false};

  Stats stats_;
  void workerThread(int id);
};