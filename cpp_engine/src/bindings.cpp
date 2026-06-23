#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "prakshep/types.hpp"
#include "prakshep/simulation.hpp"

namespace py = pybind11;

PYBIND11_MODULE(prakshep_core, m) {
    m.doc() = "Prakshep C++ 6-DOF Physics Engine";

    py::enum_<prakshep::RocketType>(m, "RocketType")
        .value("PSLV_XL", prakshep::RocketType::PSLV_XL)
        .value("GSLV_MK2", prakshep::RocketType::GSLV_MK2)
        .value("LVM3", prakshep::RocketType::LVM3)
        .export_values();

    py::enum_<prakshep::FailureType>(m, "FailureType")
        .value("NONE", prakshep::FailureType::NONE)
        .value("ENGINE_UNDERPERFORMANCE", prakshep::FailureType::ENGINE_UNDERPERFORMANCE)
        .value("CROSSWIND", prakshep::FailureType::CROSSWIND)
        .value("MASS_ASYMMETRY", prakshep::FailureType::MASS_ASYMMETRY)
        .value("TVC_FAILURE", prakshep::FailureType::TVC_FAILURE)
        .value("CRYO_VALVE_FREEZE", prakshep::FailureType::CRYO_VALVE_FREEZE)
        .value("LOX_BOILOFF", prakshep::FailureType::LOX_BOILOFF)
        .value("IMU_DRIFT", prakshep::FailureType::IMU_DRIFT)
        .export_values();

    py::class_<prakshep::Vec3>(m, "Vec3")
        .def(py::init<double, double, double>(), py::arg("x")=0, py::arg("y")=0, py::arg("z")=0)
        .def_readwrite("x", &prakshep::Vec3::x)
        .def_readwrite("y", &prakshep::Vec3::y)
        .def_readwrite("z", &prakshep::Vec3::z)
        .def("__repr__",
            [](const prakshep::Vec3 &v) {
                return "<Vec3(x=" + std::to_string(v.x) + 
                       ", y=" + std::to_string(v.y) + 
                       ", z=" + std::to_string(v.z) + ")>";
            });

    py::class_<prakshep::TelemetryFrame>(m, "TelemetryFrame")
        .def_readonly("time", &prakshep::TelemetryFrame::time)
        .def_readonly("altitude", &prakshep::TelemetryFrame::altitude)
        .def_readonly("velocity_magnitude", &prakshep::TelemetryFrame::velocity_magnitude)
        .def_readonly("acceleration_magnitude", &prakshep::TelemetryFrame::acceleration_magnitude)
        .def_readonly("downrange_distance", &prakshep::TelemetryFrame::downrange_distance)
        .def_readonly("mach_number", &prakshep::TelemetryFrame::mach_number)
        .def_readonly("dynamic_pressure", &prakshep::TelemetryFrame::dynamic_pressure)
        .def_readonly("max_q_reached", &prakshep::TelemetryFrame::max_q_reached)
        .def_readonly("max_q_value", &prakshep::TelemetryFrame::max_q_value)
        .def_readonly("latitude", &prakshep::TelemetryFrame::latitude)
        .def_readonly("longitude", &prakshep::TelemetryFrame::longitude)
        .def_readonly("geodetic_altitude", &prakshep::TelemetryFrame::geodetic_altitude)
        .def_readonly("position_eci", &prakshep::TelemetryFrame::position_eci)
        .def_readonly("velocity_eci", &prakshep::TelemetryFrame::velocity_eci)
        .def_readonly("pitch", &prakshep::TelemetryFrame::pitch)
        .def_readonly("yaw", &prakshep::TelemetryFrame::yaw)
        .def_readonly("roll", &prakshep::TelemetryFrame::roll)
        .def_readonly("current_stage", &prakshep::TelemetryFrame::current_stage)
        .def_readonly("thrust", &prakshep::TelemetryFrame::thrust)
        .def_readonly("mass", &prakshep::TelemetryFrame::mass)
        .def_readonly("propellant_remaining", &prakshep::TelemetryFrame::propellant_remaining)
        .def_readonly("structural_integrity", &prakshep::TelemetryFrame::structural_integrity)
        .def_readonly("crash_probability", &prakshep::TelemetryFrame::crash_probability)
        .def_readonly("event", &prakshep::TelemetryFrame::event);

    py::class_<prakshep::Simulation>(m, "Simulation")
        .def(py::init<prakshep::RocketType, double>(), py::arg("type"), py::arg("payload_mass") = 0)
        .def("tick", &prakshep::Simulation::tick, py::arg("dt") = 1.0/60.0)
        .def("reset", &prakshep::Simulation::reset)
        .def("inject_failure", &prakshep::Simulation::inject_failure, py::arg("type"), py::arg("severity") = 1.0)
        .def("clear_failures", &prakshep::Simulation::clear_failures)
        .def("set_tvc_override", &prakshep::Simulation::set_tvc_override, py::arg("gimbal_pitch"), py::arg("gimbal_yaw"), py::arg("throttle"))
        .def("clear_tvc_override", &prakshep::Simulation::clear_tvc_override)
        .def_property_readonly("is_complete", &prakshep::Simulation::is_complete);
}
