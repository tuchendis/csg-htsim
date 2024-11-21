#include "roce_topology.h"
#include "config.h"
#include <string>
#include <algorithm>
#include <stdlib.h>
#include <fstream>

simtime_picosec RoCETopology::_hop_latency = 0;
simtime_picosec RoCETopology::_switch_latency = 0;
linkspeed_bps RoCETopology::_linkSpeed = 1000000;
mem_b RoCETopology::_queueSize = 10;

RoCETopology::RoCETopology(const char *topoFile, QueueLoggerFactory *logger_factory, EventList& eventList) {
    std::ifstream topof;
    topof.open(topoFile);
    string gpu_type_str;
    uint32_t nodesNum, gpusPerServer, nvSwitchesNum, switchesNum, linksNum;

    topof >> nodesNum >> gpusPerServer >> nvSwitchesNum >> switchesNum >>
          linksNum >> gpu_type_str;

    _nodesType.resize(nodesNum, 0);
    for (uint32_t i = 0; i < nvSwitchesNum; i++) {
        uint32_t sid;
        topof >> sid;
        _nodesType[sid] = 2;
    }
    for (uint32_t i = 0; i < switchesNum; i++) {
        uint32_t sid;
        topof >> sid;
        _nodesType[sid] = 1;
    }
    for (uint32_t i = 0; i < nodesNum; i++) {
        uint32_t sid;
        topof >> sid;
        _nodesType[sid] = 1;
    }

    QueueLogger* queueLogger = logger_factory->createQueueLogger();

    for (uint32_t i = 0; i < nodesNum; i++) {
        if (_nodesType[i] == 1) {
            string switchName = "Switch" + i;
            _switches.push_back(new RoCESwitch(eventList, switchName, i, _switch_latency));
        }
//        else if (node_type[i] == 2) {
//            _nvSwitches.push_back(new NVSwitch(ev, i, this));
//        }
    }

    for (uint32_t i = 0; i < linksNum; i++) {
        uint32_t src, dst;
        std::string data_rate, link_delay;
        linkspeed_bps dataRate;
        simtime_picosec hopLatency;
        double error_rate;
        topof >> src >> dst >> data_rate >> link_delay >> error_rate;
//        dataRate = getDataRate(data_rate);
//        hopLatency = getHopLatency(link_delay);
        _queues[src][dst].push_back(new LosslessOutputQueue(_linkSpeed, _queueSize, *_eventList, queueLogger));
        _pipes[src][dst].push_back(new Pipe(_hop_latency, *_eventList));
        _neighbors[src].push_back(dst);
    }

    calculateRoutes();
}

void RoCETopology::calculateRoutes() {
    for (uint32_t i = 0; i < _nodesNum; i++) {
        calculateRoutes(i);
    }
}

void RoCETopology::calculateRoutes(uint32_t host) {
    vector<uint32_t> q;
    map<uint32_t , int> dis;
    q.push_back(host);
    dis[host] = 0;
    for (int i = 0; i < (int)q.size(); i++) {
        uint32_t now = q[i];
        int d = dis[now];
        for (auto it = _neighbors[now].begin(); it != _neighbors[now].end(); it++) {
            uint32_t next = *it;
            if (dis.find(next) == dis.end()) {
                dis[next] = d + 1;
                if (_nodesType[next] == 1 || _nodesType[next] == 2) {
                    q.push_back(next);
                }
            }

            bool via_nvswitch = false;
            if (d + 1 == dis[next]) {
                for(auto x : _nextHop[next][host]) {
                    if( _nodesType[x] == 2) via_nvswitch = true;
                }
                if(via_nvswitch == false) {
                    if( _nodesType[now] == 2) {
                        while(_nextHop[next][host].size() != 0)
                            _nextHop[next][host].pop_back();
                    }
                    _nextHop[next][host].push_back(now);
                } else if(via_nvswitch == true &&  _nodesType[now] == 2) {
                    _nextHop[next][host].push_back(now);
                }
                if( _nodesType[next] == 0 && _nextHop[next][now].size() == 0) {
                    _nextHop[next][now].push_back(now);
                }
            }
        }
    }
}

void RoCETopology::setRoutingEntries() {
    for (auto i = _nextHop.begin(); i != _nextHop.end(); i++) {
        uint32_t node = i->first;
        auto &table = i->second;
        for (auto j = table.begin(); j != table.end(); j++) {
            uint32_t dst = j->first;
            vector<uint32_t> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++) {
                uint32_t next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType() == 1) {
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
                } else if(node->GetNodeType() == 2){
                    DynamicCast<NVSwitchNode>(node)->AddTableEntry(dstAddr, interface);
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface, true);
                } else {
                    bool is_nvswitch = false;
                    if(next->GetNodeType() == 2){
                        is_nvswitch = true;
                    }
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface, is_nvswitch);
                    if(next->GetId() == dst->GetId())  {
                        node->GetObject<RdmaDriver>()->m_rdma->add_nvswitch(dst->GetId());
                    }
                }
            }
        }
    }
}