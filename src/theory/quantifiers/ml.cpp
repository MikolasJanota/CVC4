/*
 * File:  src/theory/quantifiers/ml.cpp
 * Author:  mikolas
 * Created on:  Thu Jan 14 11:55:02 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */

#include <climits>
#include <iostream>
#include <vector>

#include "base/check.h"
#include "cvc4_private.h"
#include "theory/quantifiers/ml.h"

#include "options/quantifiers_options.h"

namespace CVC4 {
LightGBMWrapper::LightGBMWrapper(const char* modelFile)
{  
  const int ec = LGBM_BoosterCreateFromModelfile(
      modelFile, &d_numIterations, &d_handle);
  /* std::cout << "; Loaded LGBM model " << modelFile << " with " */
  /*           << d_numIterations << " iterations." << std::endl; */
  AlwaysAssert(ec == 0);
}

double LightGBMWrapper::predict(TimerStat& timer, const float* features)
{
  TimerStat::CodeTimer codeTimer(timer);
  static_assert(CHAR_BIT * sizeof(float) == 32, "require 32-bit floats");
  double returnValue = 0;
  int64_t returnSize;
  const int ec = LGBM_BoosterPredictForMatSingleRow(d_handle,
                                                    features,
                                                    C_API_DTYPE_FLOAT32,
                                                    4,
                                                    1,
                                                    C_API_PREDICT_NORMAL,
                                                    0,
                                                    -1,
                                                    "early_stopping_rounds=100",
                                                    &returnSize,
                                                    &returnValue);
  AlwaysAssert(ec == 0);
  AlwaysAssert(returnSize == 1);
  return returnValue;
}

LightGBMWrapper::~LightGBMWrapper() {}

}  // namespace CVC4
