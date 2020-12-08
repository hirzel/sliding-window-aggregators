#pragma once

#include<iostream>
#include<iterator>
#include<cassert>
#include "RingBufferQueue.hpp"

namespace timestamped_implicit_twostackslite {
    using namespace std;

    template<typename aggT, typename timeT>
    class __AggT {
    public:
        aggT _val;
        timeT _timestamp;
        __AggT() {}
        __AggT(aggT val_, timeT time_)
            : _val(val_), _timestamp(time_) {}
    };

    template<typename binOpFunc,
             typename Timestamp,
             typename queueT=RingBufferQueue<__AggT<typename binOpFunc::Partial, Timestamp>>>
    class Aggregate {
    public:
        typedef typename binOpFunc::In inT;
        typedef typename binOpFunc::Partial aggT;
        typedef typename binOpFunc::Out outT;
        typedef Timestamp timeT;
        typedef __AggT<aggT, timeT> AggT;

        Aggregate(binOpFunc binOp_, aggT identE_) 
            : _q(), _num_flipped(0), _binOp(binOp_), _backSum(identE_), _identE(identE_) {}

        size_t size() { return _q.size(); }

        void insert(timeT const& time, inT const& v) {
            aggT lifted = _binOp.lift(v);
            _backSum = _binOp.combine(_backSum, lifted);
            _q.push_back(AggT(lifted, time));
        }

        void evict() { 
            if (front_empty()) {
                // std::cerr << "evict: flippping" << std::endl;
                // front is empty, let's turn the "stack" implicity.
                iterT front = _q.begin();
                iterT it = _q.end()-1;
                aggT running_sum = _identE;
                // std::cerr << "evict: ++++ (initial) running_sum " << running_sum << std::endl;
                while (it != front) {
                    // std::cerr << "evict: ++++ val " << it->_val << " ";
                    running_sum = _binOp.combine(it->_val, running_sum);
                    it->_val = running_sum;
                    // std::cerr << "running_sum " << running_sum << std::endl;
                    --it;
                }
                _backSum = _identE;
                _num_flipped = size();
            }
            _q.pop_front();
            _num_flipped--;
        }

        outT query() {
            auto bp = _backSum;
            auto fp = (front_empty())?_identE:_q.front()._val;

            // std::cerr << "prequery: " << _binOp.combine(fp, bp) << std::endl;
            auto answer = _binOp.lower(_binOp.combine(fp, bp));
            // std::cerr << "query: " << bp << "--" << fp << "--" << answer << std::endl;
            return  answer;
        }

        timeT oldest() { return _q.front()._timestamp; }
        timeT youngest() {  return _q.back()._timestamp; }

    private:
        inline bool front_empty() { return _num_flipped == 0; }

        queueT _q;
        typedef typename queueT::iterator iterT;
        size_t _num_flipped;
        // the binary operator deck
        binOpFunc _binOp;
        aggT _backSum;
        aggT _identE;
    };

    template <typename timeT, class BinaryFunction, class T>
    Aggregate<BinaryFunction, timeT> make_aggregate(BinaryFunction f, T elem) {
        return Aggregate<BinaryFunction, timeT>(f, elem);
    }

    template <typename BinaryFunction, typename timeT>
    struct MakeAggregate {
        template <typename T>
        Aggregate<BinaryFunction, timeT> operator()(T elem) {
            BinaryFunction f;
            return make_aggregate<timeT, BinaryFunction>(f, elem);
        }
    };
}