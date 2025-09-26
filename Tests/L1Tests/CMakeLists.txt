cmake_minimum_required(VERSION 3.8)
          set(PLUGIN_NAME SceneSetL1TestsIN)
          set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

          set(CMAKE_CXX_STANDARD 14)
          set(CMAKE_CXX_STANDARD_REQUIRED ON)

          find_package(${NAMESPACE}Plugins REQUIRED)

          set (TEST_SRC
              tests/test_SceneSet.cpp
          )

          set (TEST_LIB
              ${NAMESPACE}Plugins::${NAMESPACE}Plugins
              pthread
          )

          set (TEST_INC
              ../../SceneSet/src
          )

          add_library(${MODULE_NAME} SHARED ${TEST_SRC})

          if (RDK_SERVICES_L1_TEST)
              target_link_options(${MODULE_NAME} PRIVATE "-Wl,-wrap,opendir")
          endif (RDK_SERVICES_L1_TEST)

          include_directories(${TEST_INC})

          target_link_directories(${MODULE_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins)

          target_link_libraries(${MODULE_NAME} ${TEST_LIB})

          target_include_directories(${MODULE_NAME}
                  PUBLIC
                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                  $<INSTALL_INTERFACE:include>
                  ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks
                  ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/devicesettings
                  ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
                  ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
                  )

          install(TARGETS ${MODULE_NAME} DESTINATION lib)
          write_config(${PLUGIN_NAME})
