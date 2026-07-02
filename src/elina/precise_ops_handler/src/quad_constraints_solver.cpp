#include "quad_constraints_solver.hpp"

#include <chrono>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

std::unordered_map<std::string, void*> QuadConstraintsSolver::loaded_libraries;

// Static method to create the appropriate solver based on the name
std::unique_ptr<QuadConstraintsSolver> QuadConstraintsSolver::create_solver(const std::string& solver_name,
                                                                            const std::unordered_map<std::string, std::string>& parameters
                                                                           )
{
    std::string lib_name = solver_name + "_quad_constraints_solver.so";
    
    // Check if the library is already loaded
    if (loaded_libraries.find(lib_name) == loaded_libraries.end()) {
        // Load the library if not already loaded
        auto start = std::chrono::high_resolution_clock::now();
        void* handle = dlopen(lib_name.c_str(), RTLD_LAZY);
        auto end = std::chrono::high_resolution_clock::now();

        if (!handle) {
            throw std::runtime_error("Error loading library: " + std::string(dlerror()));
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Print the duration
        std::cout << "Time for loading lib " << lib_name << ": " << duration.count() << " ms" << std::endl;

        // Cache the loaded library handle
        loaded_libraries[lib_name] = handle;
    }

    // Retrieve the cached handle
    void* handle = loaded_libraries[lib_name];

    // Look for the factory function symbol
    typedef QuadConstraintsSolver* (*CreateFunc)();
    CreateFunc create_instance = (CreateFunc) dlsym(handle, "create_solver");
    const char* error = dlerror();
    if (error) {
        throw std::runtime_error("Error looking up factory function: " + std::string(error));
    }

    // Create an instance of the solver class using the factory function
    std::unique_ptr<QuadConstraintsSolver> solver(create_instance());

    // Set the parameters if any
    solver->set_params(parameters);

    return solver;
}