# Create a library from all .cpp files in src/
file(GLOB CONSTRAINTS_SRCS src/*.cpp)

add_library(constraints_templates_impl SHARED ${CONSTRAINTS_SRCS})

install(TARGETS constraints_templates_impl EXPORT ConstraintsTemplatesTargets)