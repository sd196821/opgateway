cmake_minimum_required(VERSION 2.8.3)
project(opgateway)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS roscpp sensor_msgs std_msgs message_generation pluginlib diagnostic_msgs diagnostic_updater)

## System dependencies are found with CMake's conventions
find_package(Boost REQUIRED COMPONENTS system thread timer date_time)

## Uncomment this if the package has a setup.py. This macro ensures
## modules and global scripts declared therein get installed
## See http://ros.org/doc/api/catkin/html/user_guide/setup_dot_py.html
# catkin_python_setup()

#######################################
## Declare ROS messages and services ##
#######################################

## Generate messages in the 'msg' folder
add_message_files(
   FILES
   FlightStatus.msg
)

## Generate services in the 'srv' folder
#add_service_files(
#   FILES
#   Service1.srv
#)

## Generate added messages and services with any dependencies listed here
generate_messages(
   DEPENDENCIES
   sensor_msgs
   std_msgs
)

###################################
## catkin specific configuration ##
###################################
## The catkin_package macro generates cmake config files for your package
## Declare things to be passed to dependent projects
## LIBRARIES: libraries you create in this project that dependent projects also need
## CATKIN_DEPENDS: catkin_packages dependent projects also need
## DEPENDS: system dependencies of this project that dependent projects also need
catkin_package(
   INCLUDE_DIRS include
   #LIBRARIES uavobjects
   CATKIN_DEPENDS roscpp sensor_msgs std_msgs message_runtime
   DEPENDS system_lib
)

###########
## Build ##
###########

include(${CMAKE_CURRENT_SOURCE_DIR}/genuavobj.cmake)

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories(
   include
   src/uavobjects
   src/uavtalk
   ${catkin_INCLUDE_DIRS}
   ${UAVOBJ_SYNTETICS_DIR}
)

## Declare a cpp library
add_library(uavobjects
   src/uavobjects/uavobject.cpp
   src/uavobjects/uavmetaobject.cpp
   src/uavobjects/uavdataobject.cpp
   src/uavobjects/uavobjectmanager.cpp
   ${UAVOBJ_SYNTETICS_SOURCES}
)

add_library(uavtalk
   src/uavtalk/uavtalk.cpp
   src/uavtalk/telemetry.cpp
   src/uavtalk/telemetrymonitor.cpp
   src/uavtalk/telemetrymanager.cpp
   src/uavtalk/uavtalkrelay.cpp
   src/uavtalk/iodrivers/uavtalkserialio.cpp
   src/uavtalk/iodrivers/uavtalkudpio.cpp
)
add_dependencies(uavtalk uavobjects)
target_link_libraries(uavtalk
  ${catkin_LIBRARIES}
  ${Boost_LIBRARIES}
)

add_library(opgateway_plugins
   src/plugins/status.cpp
)
target_link_libraries(opgateway_plugins
   uavobjects
   ${catkin_LIBRARIES}
)

## Declare a cpp executable
add_executable(opgateway_node src/opgateway_node.cpp)

## Add cmake target dependencies of the executable/library
## as an example, message headers may need to be generated before nodes
add_dependencies(opgateway_node opgateway_generate_messages_cpp)
add_dependencies(opgateway_node uavobjects)
add_dependencies(opgateway_node uavtalk)
add_dependencies(opgateway_node opgateway_plugins)

## Specify libraries to link a library or executable target against
target_link_libraries(opgateway_node
  uavobjects
  uavtalk
  ${catkin_LIBRARIES}
  ${Boost_LIBRARIES}
)

#############
## Install ##
#############

# all install targets should use catkin DESTINATION variables
# See http://ros.org/doc/api/catkin/html/adv_user_guide/variables.html

## Mark executable scripts (Python etc.) for installation
## in contrast to setup.py, you can choose the destination
# install(PROGRAMS
#   scripts/my_python_script
#   DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark executables and/or libraries for installation
# install(TARGETS opgateway opgateway_node
#   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
#   RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
# )

## Mark cpp header files for installation
# install(DIRECTORY include/${PROJECT_NAME}/
#   DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
#   FILES_MATCHING PATTERN "*.h"
#   PATTERN ".svn" EXCLUDE
# )

## Mark other files for installation (e.g. launch and bag files, etc.)
# install(FILES
#   # myfile1
#   # myfile2
#   DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
# )

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
catkin_add_gtest(uavobjects-test test/test_uavobjects.cpp)
if(TARGET uavobjects-test)
   target_link_libraries(uavobjects-test uavobjects)
endif()

catkin_add_gtest(uavtalk-test test/test_uavtalk.cpp)
if(TARGET uavtalk-test)
   target_link_libraries(uavtalk-test uavobjects uavtalk)
endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
