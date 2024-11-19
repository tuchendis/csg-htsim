//
// Created by tartarus on 11/18/24.
//

#ifndef ROCE_TOPOLOGY_H
#define ROCE_TOPOLOGY_H

#include "config.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "network.h"
#include "pipe.h"
#include "roce_switch.h"
#include "topology.h"
#include <ostream>
#include <fstream>

#define MIX(a, b, c)                            \
    do {                                        \
        a -= b; a -= c; a ^= (c >> 13);         \
        b -= c; b -= a; b ^= (a << 8);          \
        c -= a; c -= b; c ^= (b >> 13);         \
        a -= b; a -= c; a ^= (c >> 12);         \
        b -= c; b -= a; b ^= (a << 16);         \
        c -= a; c -= b; c ^= (b >> 5);          \
        a -= b; a -= c; a ^= (c >> 3);          \
        b -= c; b -= a; b ^= (a << 10);         \
        c -= a; c -= b; c ^= (b >> 15);         \
    } while (/*CONSTCOND*/0)

static inline uint32_t freeBSDHash(uint32_t target1, uint32_t target2 = 0, uint32_t target3 = 0)
{
    uint32_t a = 0x9e3779b9, b = 0x9e3779b9, c = 0; // hask key

    b += target3;
    c += target2;
    a += target1;
    MIX(a, b, c);
    return c;
}

#undef MIX

class RoCETopology {
public:
    uint32_t _nodesNum;
    uint32_t _gpusPerServer;
    uint32_t _hostsNum;
    uint32_t _linksNum;
    uint32_t _nvSwitchesNum;
    uint32_t _switchesNum;

    vector<Pipe*> _pipes;
    vector<BaseQueue*> _queues;
    vector<RoceSrc*> _roceSrcs;
    vector<RoceSink*> _roceSinks;
    vector<Switch*> _switches;
    // vector<Switch*> _nvSwitches;

    vector<vector< vector<BaseQueue*> > > queues;
    vector<vector< vector<Pipe*> > > pipes;

    QueueLoggerFactory* _loggerFactory;
    EventList* _eventList;

    static simtime_picosec _hop_latency;
    static simtime_picosec _switch_latency;

    RoCETopology(const char * topoFile, QueueLoggerFactory* logger_factory, EventList& eventList);
private:
    linkspeed_bps linkSpeed;
    mem_b queueSize;

};

#endif // ROCE_TOPOLOGY_H
