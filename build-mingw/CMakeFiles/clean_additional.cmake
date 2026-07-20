# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\FarmEyeAI_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\FarmEyeAI_autogen.dir\\ParseCache.txt"
  "FarmEyeAI_autogen"
  )
endif()
