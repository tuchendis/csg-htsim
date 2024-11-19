//
// Created by tartarus on 11/18/24.
//

#ifndef _ROCE_SWITCH_H
#define _ROCE_SWITCH_H

#include "callback_pipe.h"
#include "switch.h"
#include <unordered_map>

class RoCETopology;

class RoCESwitch : public Switch {
public:
    RoCESwitch(EventList& eventList, string s, uint32_t id, simtime_picosec delay);

    void setTopology(RoCETopology* topo);

    virtual void receivePacket(Packet& pkt);
    virtual Route* getNextHop(Packet& pkt, BaseQueue* ingressPort);

    static int8_t compare_flow_count(FibEntry* l, FibEntry* r);
    static int8_t compare_pause(FibEntry* l, FibEntry* r);
    static int8_t compare_bandwidth(FibEntry* l, FibEntry* r);
    static int8_t compare_queuesize(FibEntry* l, FibEntry* r);
    static int8_t compare_pqb(FibEntry* l, FibEntry* r);//compare pause,queue, bw.
    static int8_t compare_pq(FibEntry* l, FibEntry* r);//compare pause, queue
    static int8_t compare_pb(FibEntry* l, FibEntry* r);//compare pause, bandwidth
    static int8_t compare_qb(FibEntry* l, FibEntry* r);//compare pause, bandwidth

    static int8_t (*fn)(FibEntry*,FibEntry*);

    virtual void addHostPort(int addr, int flowid, PacketSink* transport);

    static double _ecn_threshold_fraction;

private:
    Pipe* _pipe;
    RoCETopology* _topo;

    static unordered_map<BaseQueue*,uint32_t> _port_flow_counts;

    uint32_t _crt_route;
    uint32_t _hash_salt;
    simtime_picosec _last_choice;

    unordered_map<Packet*,bool> _packets;
};

#endif // _ROCE_SWITCH_H
