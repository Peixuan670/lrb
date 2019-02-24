//
// Created by zhenyus on 1/15/19.
//

#include <chrono>
#include "simulation_truncate.h"
#include "belady_truncate.h"
#include <fstream>
#include "request.h"
#include "annotate.h"
#include "utils.h"


using namespace std;
using namespace chrono;



map<string, string> _simulation_truncate(string trace_file, string cache_type, uint64_t cache_size,
                                    map<string, string> params){
    //annotate a file
    //not necessary to annotate, but it's easier
    annotate(trace_file);

    // create cache
    unique_ptr<Cache> _webcache = move(Cache::create_unique(cache_type));
    if(_webcache == nullptr) {
        cerr<<"cache type not implemented"<<endl;
        return {};
    }
    auto webcache = dynamic_cast<BeladyTruncateCache *>(_webcache.get());

    // configure cache size
    webcache->setSize(cache_size);

    uint64_t n_warmup = 0;
    bool uni_size = false;
    uint64_t segment_window = 1000000;

    for (auto it = params.cbegin(); it != params.cend();) {
        if (it->first == "n_warmup") {
            n_warmup = stoull(it->second);
            it = params.erase(it);
        } else if (it->first == "uni_size") {
            uni_size = static_cast<bool>(stoi(it->second));
            it = params.erase(it);
            if (uni_size == true) {
                cerr << "can only work for variable size"<<endl;
                exit(-1);
            }
        } else if (it->first == "segment_window") {
            segment_window = stoull((it->second));
            it = params.erase(it);
        } else {
            ++it;
        }
    }

    webcache->init_with_params(params);

    ifstream infile(trace_file+".ant");
    if (!infile) {
        cerr << "exception opening/reading file" << endl;
        return {};
    }
    //suppose already annotated
    uint64_t byte_req = 0, byte_hit = 0, obj_req = 0, obj_hit = 0;
    uint64_t t, id, size, next_t;
    uint64_t seg_byte_req = 0, seg_byte_hit = 0, seg_obj_req = 0, seg_obj_hit = 0;
    string seg_bhr;
    string seg_ohr;
    uint64_t byte_request_hit;



    cerr<<"simulating"<<endl;
    AnnotatedRequest req(0, 0, 0, 0);
    uint64_t seq = 0;
    auto t_now = system_clock::now();

    while (infile >> t >> id >> size >> next_t) {
        if (uni_size)
            size = 1;

        DPRINTF("seq: %lu\n", seq);


        req.reinit(id, size, t, next_t);

        //don't fetch all
        if (seq >= n_warmup)
            update_metric_req(byte_req, obj_req, size);
        update_metric_req(seg_byte_req, seg_obj_req, size);

        byte_request_hit = webcache->lookup_truncate(req);
        if (seq >= n_warmup)
            update_metric_req(byte_hit, obj_hit, byte_request_hit);
        update_metric_req(seg_byte_hit, seg_obj_hit, byte_request_hit);

        ++seq;

        if (!(seq%segment_window)) {
            auto _t_now = chrono::system_clock::now();
            cerr<<"delta t: "<<chrono::duration_cast<std::chrono::milliseconds>(_t_now - t_now).count()/1000.<<endl;
            cerr<<"seq: " << seq << endl;
            double _seg_bhr = double(seg_byte_hit) / seg_byte_req;
            double _seg_ohr = double(seg_obj_hit) / seg_obj_req;
            cerr<<"accu bhr: " << double(byte_hit) / byte_req << endl;
            cerr<<"seg bhr: " << _seg_bhr << endl;
            seg_bhr+=to_string(_seg_bhr)+"\t";
            seg_ohr+=to_string(_seg_ohr)+"\t";
            seg_byte_hit=seg_obj_hit=seg_byte_req=seg_obj_req=0;
            t_now = _t_now;
        }
    }

    infile.close();

    map<string, string> res = {
            {"byte_hit_rate", to_string(double(byte_hit) / byte_req)},
            {"object_hit_rate", to_string(double(obj_hit) / obj_req)},
            {"segment_byte_hit_rate", seg_bhr},
            {"segment_object_hit_rate", seg_ohr},
    };
    return res;
}