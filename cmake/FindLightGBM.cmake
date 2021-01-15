#####################
## FindLightGBM.cmake
## Top contributors (to current version):
##   Mathias Preiner
## This file is part of the CVC4 project.
## Copyright (c) 2009-2020 by the authors listed in the file AUTHORS
## in the top-level source directory and their institutional affiliations.
## All rights reserved.  See the file COPYING in the top-level source
## directory for licensing information.
##
# Find LightGBM
# LightGBM_FOUND - system has LightGBM lib
# LightGBM_INCLUDE_DIR - the LightGBM include directory
# LightGBM_LIBRARIES - Libraries needed to use LightGBM

find_path(LightGBM_INCLUDE_DIR NAMES lightgbm.h)
find_library(LightGBM_LIBRARIES NAMES lightgbm )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LightGBM
  DEFAULT_MSG
  LightGBM_INCLUDE_DIR LightGBM_LIBRARIES)

mark_as_advanced(LightGBM_INCLUDE_DIR LightGBM_LIBRARIES)
if(LightGBM_LIBRARIES)
  message(STATUS "Found LightGBM libs: ${LightGBM_LIBRARIES}")
endif()
