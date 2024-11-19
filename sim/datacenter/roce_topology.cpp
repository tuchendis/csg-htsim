#include "roce_topology.h"
#include "config.h"
#include <string>
#include <algorithm>
#include <stdlib.h>
#include <fstream>

simtime_picosec RoCETopology::_hop_latency = 0;
simtime_picosec RoCETopology::_switch_latency = 0;

RoCETopology::RoCETopology(const char *topoFile, QueueLoggerFactory *logger_factory, EventList& eventList) {
    std::ifstream topof;
    topof.open(topoFile);
    string gpu_type_str;
    uint32_t nodesNum, gpusPerServer, nvSwitchesNum, switchesNum, linksNum;

    topof >> nodesNum >> gpusPerServer >> nvSwitchesNum >> switchesNum >>
          linksNum >> gpu_type_str;

    std::vector<uint32_t> node_type(nodesNum, 0);
    for (uint32_t i = 0; i < nvSwitchesNum; i++) {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 2;
    }
    for (uint32_t i = 0; i < switchesNum; i++) {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
    }
    for (uint32_t i = 0; i < nodesNum; i++) {
        uint32_t sid;
        topof >> sid;
        node_type[sid] = 1;
    }

    for (uint32_t i = 0; i < nodesNum; i++){
        if (node_type[i] == 1) {
            string switchName = "Switch" + i;
            _switches.push_back(new RoCESwitch(eventList, switchName, i, _switch_latency));
            _queues.push_back(new LosslessOutputQueue(speed, queuesize, *_eventlist, queueLogger));
        }
//        else if (node_type[i] == 2) {
//            _nvSwitches.push_back(new NVSwitch(ev, i, this));
//        }
    }
}