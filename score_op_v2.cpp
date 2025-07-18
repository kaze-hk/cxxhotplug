// score_op_v2.cpp
#include "operator_interface.h"
#include <iostream>
#include <cmath>

struct ScoreOperatorV2 : IScoreOperator {
    double compute_score(const Feature& feature) override {
        // V2算法：更复杂的非线性计算 + 偏置
        double base_score = feature.user_feature * 0.4 + feature.item_feature * 0.6;
        return base_score * (1.0 + 0.1 * sin(feature.user_id * 0.1)) + 2.0;
    }
    const char* name() const override {
        return "ScoreOperatorV2";
    }
};

extern "C" IScoreOperator* create_operator() {
    return new ScoreOperatorV2();
}
extern "C" void destroy_operator(IScoreOperator* op) {
    delete op;
}
