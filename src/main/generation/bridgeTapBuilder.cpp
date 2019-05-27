#include <vector>
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>

#include "yntdl.h"
#include "errorCode.h"
#include "nodeTypeMap.h"
#include "bridgeTapBuilder.h"

using namespace std;

int assignBridgesTaps(yntdl::Topology *top, int curNum){
    for(auto linkPtr : top->links){
        for(yntdl::Iface *ifacePtr : linkPtr->ifaces){
            if(nodeTypeMap.at(ifacePtr->node->type)->createBridgesTaps()){
                ifacePtr->bridgeName = to_string(curNum) + "_b";
                ifacePtr->tapName = to_string(curNum) + "_t";
                curNum++;
            }
        }
    }
    for(auto topPtr : top->subTopologies){
        curNum = assignBridgesTaps(topPtr.get(), curNum);
    }
    return curNum;
}

void buildAllBridgesTaps(yntdl::Topology *top){
    for(auto topPtr : top->subTopologies){
        buildAllBridgesTaps(topPtr.get());
    }
    for(auto nodePtr : top->nodes){
        if(!nodeTypeMap.at(nodePtr->type)->createBridgesTaps()){
            continue;
        }
        for(auto it : nodePtr->ifaces){
            if(it.second.ip != nullptr && it.second.subnetMask != nullptr){
                buildBridgeTap(&it.second);
            }
        }
    }
}

void buildBridgeTap(yntdl::Iface *ifacePtr){
    int err;
    string tap = ifacePtr->tapName;
    string bridge = ifacePtr->bridgeName;
    //brctl addbr 'bridge'
    err = system(("brctl addbr " + bridge).c_str());
    if(err){
       throw Ns3lxcException(ErrorCode::BR_CREATE_FAILURE, bridge + " for iface: " + ifacePtr->name);
    }
    //ip tuntap add 'tap' mode tap
    err = system(("ip tuntap add " + tap + " mode tap").c_str());
    if(err){
       throw Ns3lxcException(ErrorCode::TAP_CREATE_FAILURE, tap + " for iface: " + ifacePtr->name);
    }
    //ifconfig 'tap' 0.0.0.0 promisc up
    err = system(("ifconfig " + tap + " 0.0.0.0 promisc up").c_str());
    if(err){
       throw Ns3lxcException(ErrorCode::TAP_CREATE_FAILURE, tap + " for iface: " + ifacePtr->name);
    }
    //brctl addif 'bridge' 'tap'
    err = system(("brctl addif " + bridge + " " + tap).c_str());
    if(err){
       throw Ns3lxcException(ErrorCode::BR_CREATE_FAILURE, bridge + " with tap " + tap + " for iface: " + ifacePtr->name);
    }

    err = system(("ifconfig " + bridge + " up ").c_str());
    if(err){
       throw Ns3lxcException(ErrorCode::BR_CREATE_FAILURE, bridge + " for iface: " + ifacePtr->name);
    }
}

void tearDownAllBridgesTaps(yntdl::Topology *top){
    for(auto topPtr : top->subTopologies){
        tearDownAllBridgesTaps(topPtr.get());
    }
    for(auto nodePtr : top->nodes){
        if(!nodeTypeMap.at(nodePtr->type)->createBridgesTaps()){
            continue;
        }
        for(auto it : nodePtr->ifaces){
            if(it.second.ip != nullptr && it.second.subnetMask != nullptr){
                tearDownBridgeTap(&it.second);
            }
        }
    }
}

void tearDownBridgeTap(yntdl::Iface *ifacePtr){
    int err;
    string tap = ifacePtr->tapName;
    string bridge = ifacePtr->bridgeName;
    string nullRedir = " 2>&1 > /dev/null";
    //ifconfig 'bridge' down
    // err = system(("ifconfig " + bridge + " down" + nullRedir).c_str());
    pclose(popen(("ifconfig " + bridge + " down" + nullRedir).c_str(), "r"));
    if(err){

    }
    //brctl delif 'bridge' 'tap'
    // err = system(("brctl delif " + bridge + " " + tap + nullRedir).c_str());
    pclose(popen(("brctl delif " + bridge + " " + tap + nullRedir).c_str(), "r"));
    if(err){

    }
    //brctl delbr 'bridge'
    // err = system(("brctl delbr " + bridge + nullRedir).c_str());
    pclose(popen(("brctl delbr " + bridge + nullRedir).c_str(), "r"));
    if(err){

    }
    //ifconfig 'tap' down
    // err = system(("ifconfig " + tap + " down" + nullRedir).c_str());
    pclose(popen(("ifconfig " + tap + " down" + nullRedir).c_str(), "r"));
    if(err){

    }
    //ip tuntap del 'tap' mode tap
    // err = system(("ip tuntap del " + tap + " mode tap" + nullRedir).c_str());
    pclose(popen(("ip tuntap del " + tap + " mode tap" + nullRedir).c_str(), "r"));
    if(err){

    }
}