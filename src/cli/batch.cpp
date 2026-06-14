#include "verosim/cli/batch.h"

#include <algorithm>
#include <atomic>
#include <future>
#include <sstream>
#include <thread>

#include "verosim/cli/list_io.h"
#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

void CompareBatchToJson(const std::vector<PairRow> &pairs, const std::string &base_dir, int jobs,
    const CompareCliOptions &options, std::ostream &out)
{
    if (jobs <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        jobs = hw == 0 ? 1 : static_cast<int>(hw);
    }
    jobs = std::max(1, std::min(jobs, static_cast<int>(std::max<std::size_t>(pairs.size(), 1))));

    std::vector<std::string> records(pairs.size());

    std::atomic<std::size_t> next{ 0 };
    std::vector<std::future<void>> workers;
    for (int w = 0; w < jobs; ++w) {
        workers.push_back(std::async(std::launch::async, [&] {
            VrvBridge bridge;
            for (;;) {
                const std::size_t i = next.fetch_add(1);
                if (i >= pairs.size()) break;
                const std::string pred = JoinBaseDir(base_dir, pairs[i].pred);
                const std::string gt = JoinBaseDir(base_dir, pairs[i].gt);
                std::ostringstream record;
                ComparePairToJson(bridge, pred, gt, options, record);
                records[i] = record.str();
            }
        }));
    }
    for (auto &worker : workers) worker.get();

    for (const std::string &record : records) out << record;
}

} // namespace verosim
