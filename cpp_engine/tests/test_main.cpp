#include "prakshep/simulation.hpp"
#include "prakshep/atmosphere.hpp"
#include "prakshep/gravity.hpp"
#include "prakshep/constants.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace prakshep;

void test_atmosphere() {
    auto atm = atmosphere::compute(0.0);
    assert(std::abs(atm.temperature - constants::T0) < 1e-4);
    assert(std::abs(atm.pressure - constants::P0) < 1e-4);
    assert(std::abs(atm.density - constants::RHO0) < 1e-4);
    std::cout << "Atmosphere tests passed.\n";
}

void test_gravity() {
    Vec3 surface_pos(constants::R_EARTH, 0, 0);
    Vec3 g = gravity::compute(surface_pos);
    assert(std::abs(g.norm() - 9.798) < 0.1); // ~9.8 m/s^2 at equator
    std::cout << "Gravity tests passed.\n";
}

void test_simulation() {
    Simulation sim(RocketType::PSLV_XL);
    double initial_mass = sim.get_state().mass;
    
    std::cout << "Starting 5-second PSLV-XL simulation...\n";
    
    double dt = 1.0 / 60.0;
    for (int i = 0; i < 300; ++i) {
        auto telem = sim.tick(dt);
        if (i % 60 == 0) {
            std::cout << "T+" << telem.time << "s | Alt: " << telem.altitude 
                      << "m | Vel: " << telem.velocity_magnitude 
                      << "m/s | Mass: " << telem.mass << "kg\n";
        }
    }
    
    auto final_telem = sim.tick(dt);
    assert(final_telem.altitude >= 0.0);
    assert(final_telem.mass < initial_mass);
    std::cout << "Simulation tests passed.\n";
}

int main() {
    test_atmosphere();
    test_gravity();
    test_simulation();
    std::cout << "All tests passed successfully.\n";
    return 0;
}
