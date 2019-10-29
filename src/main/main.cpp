#include <string>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>
#include <unistd.h>
#include <sys/stat.h>

#include "yaml-cpp/yaml.h"
#include "yntdl.h"
#include "settingsParser.h"
#include "topologyGenerator.h"
#include "topologyValidator.h"
#include "errorCode.h"

#define MAXPATHLEN 1024

#define PROJ_ROOT_DIR "ns3-lxc"
#define SETTINGS_FILE "settings.yaml"
#define TAG_TIME "time"

#define HELP_STATEMENT "usage: 'sudo bin/ns3lxc pathToTopology [-n] [-c] [-s] [-g] [-h]'\n\
-n: write only ns-3 script SUDO NOT REQUIRED\n\
-c: cleanup mode, teardown bridges, taps, and containers\n\
-s: spawn mode, do not teardown after simulation completion\n\
-g: GDB mode, run ns-3 script with GDB\n\
-h: show this help text"

using namespace std;

static std::string compute_settings_path(){
	char cwd[MAXPATHLEN];
	size_t substr;
	std::string dir;

	getcwd(cwd, MAXPATHLEN);
	dir = std::string(cwd);
	
	substr = dir.find(PROJ_ROOT_DIR);

	if(substr != std::string::npos){
		dir = dir.substr(0, substr + strlen(PROJ_ROOT_DIR));
		dir = dir + '/' + SETTINGS_FILE;
	} else {
		dir = SETTINGS_FILE;
	}

	return dir;
}

static map<string, string> parseArgs(int argc, char *argv[]){
	map<string, string> argMap;
	for(int i = 1; i < argc; ++i){
		string arg(argv[i]);
		if(arg.find('-') == 0){
			argMap[arg] = "exists";
		} else {
			argMap["file"] = arg;
		}
	}
	return argMap;
}

static void setOutputDest(string topologyName){
	Settings::top_output_dest = Settings::output_dest;
	if(Settings::top_output_dest[Settings::top_output_dest.length() - 1] != '/'){
		Settings::top_output_dest = Settings::top_output_dest + '/';
	}
	Settings::top_output_dest = Settings::top_output_dest + topologyName;
	check_make_dir(Settings::top_output_dest.c_str());
	if(Settings::top_output_dest[Settings::top_output_dest.length() - 1] != '/'){
		Settings::top_output_dest = Settings::top_output_dest + '/';
	}
}

static void applyNodeDefaultType(yntdl::Topology *top){
	for(auto nodePtr : top->nodes){
        if(nodePtr->type == ""){
        	nodePtr->type = Settings::node_type;
        }
    }
    for(auto subTopPtr : top->subTopologies){
        applyNodeDefaultType(subTopPtr.get());
    }
}

int main(int argc, char *argv[]){
	try{
		std::string settings_path = compute_settings_path();

		// open settings file and obtain directory locations
		int result = Settings::parse_settings_file(settings_path);

		if(argc < 2) {
			cout << HELP_STATEMENT << endl;
			throw Ns3lxcException(ErrorCode::NOT_ENOUGH_ARGS, to_string(argc));
		}
		map<string, string> argMap = parseArgs(argc, argv);
		yntdl::Topology topology;

		if(argMap.count("-h")){
			cout << HELP_STATEMENT << endl;
			exit(0);
		}

		if(argMap.count("file") < 1) {
			throw Ns3lxcException(ErrorCode::NO_FILE_PROVIDED, "");
		}

		//check for existence of topology file and make sure not folder
		std::ifstream infile(argMap.at("file"));
		infile.seekg(0, ios::end);
	    if(!infile.good()){
	    	throw Ns3lxcException(ErrorCode::FILE_NOT_FOUND, argMap.at("file"));
	    }
	    infile.close();

		if(argMap.size() > 1){
			Settings::run_mode = Mode::NONE;
		}
		if(argMap.count("-n")){
			Settings::run_mode |= Mode::NS3_GEN;
		}
		if(argMap.count("-c")){
			Settings::run_mode |= Mode::CLEANUP;
		}
		if(argMap.count("-s")){
			Settings::run_mode |= Mode::NODE_GEN;
		}
		if(argMap.count("-g") > 0){
			Settings::gdb = true;
			if(argMap.size() == 2){
				Settings::run_mode == Mode::NORMAL;
			}
		}

		if(geteuid() && Settings::run_mode != Mode::NS3_GEN){
			throw Ns3lxcException(ErrorCode::RUN_AS_ROOT, "");
		}

		topology = parseTopologyFile(argMap.at("file"));
		if(topology.additionalTags[TAG_TIME]){
			Settings::run_time = topology.additionalTags[TAG_TIME].as<double>();
		}
		setOutputDest(topology.name);
		applyNodeDefaultType(&topology);
		yntdl::Topology::reNumNodes(&topology);
		//call YNTDL's validation
		yntdl::validateTopology(&topology);
		//validate types vs type maps
		validateTopologyTypes(&topology);
		try{
			generateTopology(&topology);
		} catch(Ns3lxcException& e){
			//Handle teardown if applicable
			throw e;
		} catch(yntdl::YntdlException& e1){
			throw e1;
		}
	} catch(Ns3lxcException& e){
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
	
}
