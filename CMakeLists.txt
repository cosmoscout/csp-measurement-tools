# ------------------------------------------------------------------------------------------------ #
#                                This file is part of CosmoScout VR                                #
#       and may be used under the terms of the MIT license. See the LICENSE file for details.      #
#                         Copyright: (c) 2019 German Aerospace Center (DLR)                        #
# ------------------------------------------------------------------------------------------------ #

# build plugin -------------------------------------------------------------------------------------

file(GLOB SOURCE_FILES src/*.cpp src/voronoi/*.cpp)

add_library(csp-measurement-tools SHARED
  ${SOURCE_FILES}
)

target_link_libraries(csp-measurement-tools
  PUBLIC
    cs-core
)

# install plugin -----------------------------------------------------------------------------------

install(TARGETS    csp-measurement-tools DESTINATION "share/plugins")
install(DIRECTORY "gui"                  DESTINATION "share/resources")
