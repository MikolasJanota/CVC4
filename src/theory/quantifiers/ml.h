/*
 * File:  src/theory/quantifiers/ml.h
 * Author:  mikolas
 * Created on:  Thu Jan 14 11:55:09 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */

#ifndef CVC4__ML
#define CVC4__ML

#include "lightgbm.h"
namespace CVC4 {
class LightGBMWrapper
{
 public:
  LightGBMWrapper(const char* modelFile);
  double predict(const float * features);
  virtual ~LightGBMWrapper();

 protected:
  BoosterHandle d_handle;
  int d_numIterations;
};
}  // namespace CVC4

#endif
