#####################
## CMakeLists.txt
## Top contributors (to current version):
##   Mathias Preiner, Gereon Kremer, Aina Niemetz
## This file is part of the CVC4 project.
## Copyright (c) 2009-2021 by the authors listed in the file AUTHORS
## in the top-level source directory and their institutional affiliations.
## All rights reserved.  See the file COPYING in the top-level source
## directory for licensing information.
##
configure_file(floatingpoint_literal_symfpu.h.in floatingpoint_literal_symfpu.h)
configure_file(rational.h.in rational.h)
configure_file(integer.h.in integer.h)
configure_file(real_algebraic_number.h.in real_algebraic_number.h)

libcvc4_add_sources(
  abstract_value.cpp
  abstract_value.h
  bin_heap.h
  bitvector.cpp
  bitvector.h
  bool.h
  cardinality.cpp
  cardinality.h
  dense_map.h
  divisible.cpp
  divisible.h
  floatingpoint.cpp
  floatingpoint.h
  floatingpoint_size.cpp
  floatingpoint_size.h
  gmp_util.h
  hash.h
  iand.h
  index.cpp
  index.h
  maybe.h
  ostream_util.cpp
  ostream_util.h
  poly_util.cpp
  poly_util.h
  random.cpp
  random.h
  resource_manager.cpp
  resource_manager.h
  result.cpp
  result.h
  regexp.cpp
  regexp.h
  roundingmode.h
  safe_print.cpp
  safe_print.h
  sampler.cpp
  sampler.h
  sexpr.cpp
  sexpr.h
  smt2_quote_string.cpp
  smt2_quote_string.h
  statistics.cpp
  statistics.h
  statistics_registry.cpp
  statistics_registry.h
  string.cpp
  string.h
  floatingpoint_literal_symfpu.cpp
  tuple.h
  unsafe_interrupt_exception.h
  utility.cpp
  utility.h
)

if(CVC4_USE_CLN_IMP)
  libcvc4_add_sources(rational_cln_imp.cpp integer_cln_imp.cpp)
endif()

if(CVC4_USE_GMP_IMP)
  libcvc4_add_sources(rational_gmp_imp.cpp integer_gmp_imp.cpp)
endif()

if(CVC4_USE_POLY_IMP)
  libcvc4_add_sources(real_algebraic_number_poly_imp.cpp real_algebraic_number_poly_imp.h)
endif()
