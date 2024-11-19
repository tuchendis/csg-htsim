//
// Created by tartarus on 11/18/24.
//

#include "roce_switch.h"
#include "routetable.h"
#include "roce_topology.h"
#include "callback_pipe.h"
#include "queue_lossless.h"
#include "queue_lossless_output.h"

unordered_map<BaseQueue*, uint32_t> RoCESwitch::_port_flow_counts;
double RoCESwitch::_ecn_threshold_fraction = 1.0;
int8_t (*RoCESwitch::fn)(FibEntry*,FibEntry*)= &RoCESwitch::compare_queuesize;

RoCESwitch::RoCESwitch(EventList &eventList, string s, uint32_t id, simtime_picosec delay): Switch(eventList, s) {
    _id = id;
    _pipe = new CallbackPipe(delay, eventList, this);
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventList.now();
    _fib = new RouteTable();
}

void RoCESwitch::setTopology(RoCETopology *topo) {
    _topo = topo;
}

void RoCESwitch::receivePacket(Packet &pkt) {
    if (pkt.type() == ETH_PAUSE) {
        EthPausePacket* p = (EthPausePacket*) &pkt;
        for (size_t i = 0; i < _ports.size(); i++) {
            LosslessQueue* q = (LosslessQueue*)_ports.at(i);
            if (q->getRemoteEndpoint() && ((Switch*)q->getRemoteEndpoint())->getID() == p->senderID()) {
                q->receivePacket(pkt);
                break;
            }
        }
        return;
    }

    if (_packets.find(&pkt)==_packets.end()) {
        _packets[&pkt] = true;
        const Route *nh = getNextHop(pkt, NULL);
        pkt.set_route(*nh);

        //emulate the switching latency between ingress and packet arriving at the egress queue.
        _pipe->receivePacket(pkt);
    } else {
        _packets.erase(&pkt);
        pkt.sendOn();
    }
}

void RoCESwitch::addHostPort(int dst, int flowid, PacketSink *transport) {
    Route* rt = new Route();
    rt->push_back(_topo->queues[_id][dst][0]);
    rt->push_back(_topo->pipes[_id][dst][0]);
    rt->push_back(transport);
    _fib->addHostRoute(dst, rt, flowid);
}

int8_t RoCESwitch::compare_pause(FibEntry* left, FibEntry* right){
    Route * r1= left->getEgressPort();
    assert(r1 && r1->size()>1);
    LosslessOutputQueue* q1 = dynamic_cast<LosslessOutputQueue*>(r1->at(0));
    Route * r2= right->getEgressPort();
    assert(r2 && r2->size()>1);
    LosslessOutputQueue* q2 = dynamic_cast<LosslessOutputQueue*>(r2->at(0));

    if (!q1->is_paused()&&q2->is_paused())
        return 1;
    else if (q1->is_paused()&&!q2->is_paused())
        return -1;
    else
        return 0;
}

int8_t RoCESwitch::compare_flow_count(FibEntry* left, FibEntry* right){
    Route * r1= left->getEgressPort();
    assert(r1 && r1->size()>1);
    BaseQueue* q1 = (BaseQueue*)(r1->at(0));
    Route * r2= right->getEgressPort();
    assert(r2 && r2->size()>1);
    BaseQueue* q2 = (BaseQueue*)(r2->at(0));

    if (_port_flow_counts.find(q1)==_port_flow_counts.end())
        _port_flow_counts[q1] = 0;

    if (_port_flow_counts.find(q2)==_port_flow_counts.end())
        _port_flow_counts[q2] = 0;

    //cout << "CMP q1 " << q1 << "=" << _port_flow_counts[q1] << " q2 " << q2 << "=" << _port_flow_counts[q2] << endl;

    if (_port_flow_counts[q1] < _port_flow_counts[q2])
        return 1;
    else if (_port_flow_counts[q1] > _port_flow_counts[q2] )
        return -1;
    else
        return 0;
}

int8_t RoCESwitch::compare_queuesize(FibEntry* left, FibEntry* right){
    Route * r1= left->getEgressPort();
    assert(r1 && r1->size()>1);
    BaseQueue* q1 = dynamic_cast<BaseQueue*>(r1->at(0));
    Route * r2= right->getEgressPort();
    assert(r2 && r2->size()>1);
    BaseQueue* q2 = dynamic_cast<BaseQueue*>(r2->at(0));

    if (q1->quantized_queuesize() < q2->quantized_queuesize())
        return 1;
    else if (q1->quantized_queuesize() > q2->quantized_queuesize())
        return -1;
    else
        return 0;
}

int8_t RoCESwitch::compare_bandwidth(FibEntry* left, FibEntry* right){
    Route * r1= left->getEgressPort();
    assert(r1 && r1->size()>1);
    BaseQueue* q1 = dynamic_cast<BaseQueue*>(r1->at(0));
    Route * r2= right->getEgressPort();
    assert(r2 && r2->size()>1);
    BaseQueue* q2 = dynamic_cast<BaseQueue*>(r2->at(0));

    if (q1->quantized_utilization() < q2->quantized_utilization())
        return 1;
    else if (q1->quantized_utilization() > q2->quantized_utilization())
        return -1;
    else
        return 0;

    /*if (q1->average_utilization() < q2->average_utilization())
        return 1;
    else if (q1->average_utilization() > q2->average_utilization())
        return -1;
    else
        return 0;        */
}

int8_t RoCESwitch::compare_pqb(FibEntry* left, FibEntry* right){
    //compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p!=0)
        return p;

    p = compare_queuesize(left,right);

    if (p!=0)
        return p;

    return compare_bandwidth(left,right);
}

int8_t RoCESwitch::compare_pq(FibEntry* left, FibEntry* right){
    //compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p!=0)
        return p;

    return compare_queuesize(left,right);
}

int8_t RoCESwitch::compare_qb(FibEntry* left, FibEntry* right){
    //compare pause, queuesize, bandwidth.
    int8_t p = compare_queuesize(left, right);

    if (p!=0)
        return p;

    return compare_bandwidth(left,right);
}

int8_t RoCESwitch::compare_pb(FibEntry* left, FibEntry* right){
    //compare pause, queuesize, bandwidth.
    int8_t p = compare_pause(left, right);

    if (p!=0)
        return p;

    return compare_bandwidth(left,right);
}

Route* RoCESwitch::getNextHop(Packet& pkt, BaseQueue* ingressPort) {
    vector<FibEntry*> * available_hops = _fib->getRoutes(pkt.dst());

    if (available_hops) {
        uint32_t ecmp_choice = 0;
        if (available_hops->size() > 1) {
            ecmp_choice = freeBSDHash(pkt.flow_id(),pkt.pathid(),_hash_salt) % available_hops->size();
        }

        FibEntry* e = (*available_hops)[ecmp_choice];

        return e->getEgressPort();
    }

    cerr << "Route need to be filled." << endl;
    abort();
}