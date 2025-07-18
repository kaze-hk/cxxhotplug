// score_op_v1.cpp
#include "operator_interface.h"
#include <iostream>
#include <cmath>

struct ScoreOperatorV1 : IScoreOperator {
    double compute_score(const Feature& feature) override {
        // V1算法：简单线性组合
        return feature.user_feature * 0.7 + feature.item_feature * 0.3;
    }
    const char* name() const override {
        return "ScoreOperatorV1";
    }
};

extern "C" IScoreOperator* create_operator() {
    return new ScoreOperatorV1();
}
extern "C" void destroy_operator(IScoreOperator* op) {
    delete op;
}
