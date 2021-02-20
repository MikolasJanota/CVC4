/*
 * File:  src/theory/quantifiers/ml.h
 * Author:  mikolas
 * Created on:  Thu Jan 14 11:55:09 CET 2021
 * Copyright (C) 2021, Mikolas Janota
 */

#ifndef CVC4__ML
#define CVC4__ML

#include <cmath>
#include <vector>

#include "lightgbm.h"

namespace CVC4 {
class LearningInterface
{
 public:
  virtual ~LearningInterface() {}
  virtual double predict(const float* features) = 0;
  virtual size_t numberOfFeatures() const = 0;
};

class Sigmoid : public LearningInterface
{
 public:
  Sigmoid(const char* modelFile);
  virtual ~Sigmoid() {}

  inline double sigmoid(double x)
  {
    if (x < 0)
    {
      const double expx = std::exp(x);
      return expx * (1 + expx);
    }
    return 1 / (1 + std::exp(-x));
  }

  virtual double predict(const float* features) override
  {
    double exponent = d_coefficients[0];  //  assuming intercept on the first position
    for (size_t i = 1; i < d_coefficients.size(); i++)
    {
      exponent += d_coefficients[i] * features[i - 1];
    }
    return sigmoid(exponent);
  }

  virtual size_t numberOfFeatures() const override
  {
    return d_coefficients.size() - 1;
  }

 protected:
  std::vector<double> d_coefficients;
};

class LightGBMWrapper : public LearningInterface
{
 public:
  LightGBMWrapper(const char* modelFile);
  virtual double predict(const float* features) override;
  virtual ~LightGBMWrapper();

  virtual size_t numberOfFeatures() const override;

 protected:
  BoosterHandle d_handle;
  int d_numIterations;
};
}  // namespace CVC4

#endif
