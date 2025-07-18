// operator_interface.h
#pragma once

struct Feature {
    int user_id;
    int item_id;
    double user_feature;
    double item_feature;
};

// 算子基类接口
struct IScoreOperator {
    virtual ~IScoreOperator() = default;
    virtual double compute_score(const Feature& feature) = 0;
    virtual const char* name() const = 0; // 方便验证版本
};
