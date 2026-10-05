#ifndef msr_AirLibUnitTests_OusterSettingsTest_hpp
#define msr_AirLibUnitTests_OusterSettingsTest_hpp

#include "TestBase.hpp"
#include "common/AirSimSettings.hpp"
#include "sensors/SensorFactory.hpp"
#include "sensors/lidar/LidarSimpleParams.hpp"

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace msr
{
namespace airlib
{

    // Ouster sensors (SensorType 12), for the hercules_sensors_ouster host
    // process: AirLib reads their settings, with the pose conventions of the
    // Lidar sensor, but has no sensor to create for them yet, so the sensor
    // factory skips them with a warning.
    class OusterSettingsTest : public TestBase
    {
    public:
        virtual void run() override
        {
            testSettings();
            testHostPortRange();
            testSensorFactory();
            testDefaultSensors();
        }

    private:
        // keeps what is logged while it is installed
        class CaptureLogger : public common_utils::Utils::Logger
        {
        public:
            CaptureLogger()
                : previous_(Utils::getSetLogger())
            {
                Utils::getSetLogger(this);
            }

            ~CaptureLogger()
            {
                Utils::getSetLogger(previous_);
            }

            virtual void log(int level, const std::string& message) override
            {
                messages.emplace_back(level, message);
            }

            std::vector<std::pair<int, std::string>> messages;

        private:
            common_utils::Utils::Logger* previous_;
        };

        static bool near(real_T a, real_T b)
        {
            return std::abs(a - b) < 1e-5f;
        }

        static bool samePose(const Pose& a, const Pose& b)
        {
            return (a.position - b.position).norm() < 1e-5f &&
                   std::abs(std::abs(a.orientation.dot(b.orientation)) - 1) < 1e-5f;
        }

        static void loadSensors(const std::string& sensors)
        {
            AirSimSettings::initializeSettings(R"({"SettingsVersion": 2.0, "SimMode": "Hero",
                "Vehicles": {"Husky1": {"VehicleType": "PhysXCar", "Sensors": {)" + sensors + "}}}}");
            AirSimSettings::singleton().load(nullptr);
        }

        static const AirSimSettings::SensorSetting& sensor(const std::string& name)
        {
            return *AirSimSettings::singleton().vehicles.at("Husky1")->sensors.at(name);
        }

        static AirSimSettings::OusterSetting ouster(const std::string& name)
        {
            return static_cast<const AirSimSettings::OusterSetting&>(sensor(name));
        }

        static Pose lidarPose(const std::string& name)
        {
            LidarSimpleParams params;
            params.initializeFromSettings(static_cast<const AirSimSettings::LidarSetting&>(sensor(name)));
            return params.relative_pose;
        }

        void testSettings()
        {
            // each Ouster has a lidar twin with the same pose keys
            loadSensors(R"(
                "os_default": {"SensorType": 12, "Enabled": true},
                "lidar_default": {"SensorType": 6, "Enabled": true},
                "os_mounted": {"SensorType": 12, "Enabled": true, "X": 0.2, "Y": -0.1, "Z": -1.5,
                               "Roll": 2, "Pitch": -5, "Yaw": 90,
                               "HostAddress": "192.168.1.20", "HostPort": 7502},
                "lidar_mounted": {"SensorType": 6, "Enabled": true, "X": 0.2, "Y": -0.1, "Z": -1.5,
                                  "Roll": 2, "Pitch": -5, "Yaw": 90},
                "os_partial": {"SensorType": 12, "Enabled": true, "Z": -0.5, "Yaw": 45},
                "lidar_partial": {"SensorType": 6, "Enabled": true, "Z": -0.5, "Yaw": 45}
            )");

            const auto plain = ouster("os_default");
            testAssert(plain.sensor_type == SensorBase::SensorType::Ouster, "Ouster sensor type");
            testAssert(plain.sensor_name == "os_default" && plain.enabled, "Ouster name and Enabled");
            testAssert(plain.host_address == "127.0.0.1", "default HostAddress");
            testAssert(plain.host_port == 7600, "default HostPort");
            testAssert(plain.position == Vector3r::Zero(), "default position is zero");
            testAssert(plain.rotation.roll == 0 && plain.rotation.pitch == 0 && plain.rotation.yaw == 0,
                       "default rotation is zero");
            testAssert(samePose(plain.relativePose(), Pose(Vector3r::Zero(), Quaternionr::Identity())),
                       "default pose is the vehicle's");
            testAssert(samePose(plain.relativePose(), lidarPose("lidar_default")), "default pose as a lidar's");

            const auto mounted = ouster("os_mounted");
            testAssert(near(mounted.position.x(), 0.2f) && near(mounted.position.y(), -0.1f) &&
                           near(mounted.position.z(), -1.5f),
                       "position in metres, NED");
            testAssert(mounted.rotation.roll, 2.0, "roll in degrees");
            testAssert(mounted.rotation.pitch, -5.0, "pitch in degrees");
            testAssert(mounted.rotation.yaw, 90.0, "yaw in degrees");
            testAssert(mounted.host_address == "192.168.1.20", "HostAddress");
            testAssert(mounted.host_port == 7502, "HostPort");
            testAssert(samePose(mounted.relativePose(), lidarPose("lidar_mounted")), "pose as a lidar's");

            const auto partial = ouster("os_partial");
            testAssert(partial.position == Vector3r(0, 0, -0.5f), "unset X and Y are zero");
            testAssert(partial.rotation.roll == 0 && partial.rotation.pitch == 0 && partial.rotation.yaw == 45,
                       "unset Roll and Pitch are zero");
            testAssert(samePose(partial.relativePose(), lidarPose("lidar_partial")), "partial pose as a lidar's");
        }

        void testHostPortRange()
        {
            for (const std::string port : { "0", "-1", "65536", "7600.5" }) {
                bool threw = false;
                try {
                    loadSensors(R"("os": {"SensorType": 12, "Enabled": true, "HostPort": )" + port + "}");
                }
                catch (const std::invalid_argument& e) {
                    threw = std::string(e.what()).find("HostPort") != std::string::npos;
                }
                testAssert(threw, "HostPort " + port + " was not rejected");
            }

            loadSensors(R"("low": {"SensorType": 12, "Enabled": true, "HostPort": 1},
                           "high": {"SensorType": 12, "Enabled": true, "HostPort": 65535})");
            testAssert(ouster("low").host_port == 1 && ouster("high").host_port == 65535, "HostPort 1 and 65535 accepted");
        }

        void testSensorFactory()
        {
            loadSensors(R"("os": {"SensorType": 12, "Enabled": true})");
            const auto& setting = AirSimSettings::singleton().vehicles.at("Husky1")->sensors.at("os");
            SensorFactory factory;

            CaptureLogger logger;
            testAssert(factory.createSensorFromSettings(setting.get()) == nullptr, "SensorFactory created an Ouster sensor");
            testAssert(logger.messages.size() == 1 && logger.messages[0].first == Utils::kLogLevelWarn,
                       "an ignored Ouster sensor logs one warning");
            testAssert(logger.messages[0].second.find("'os'") != std::string::npos, "the warning names the sensor");

            SensorCollection sensors;
            vector<shared_ptr<SensorBase>> storage;
            factory.createSensorsFromSettings({ { "os", setting } }, sensors, storage);
            testAssert(storage.empty() && sensors.size(SensorBase::SensorType::Ouster) == 0,
                       "createSensorsFromSettings skips the Ouster sensor");
        }

        void testDefaultSensors()
        {
            // a default Ouster goes only to the vehicles without one of their own
            AirSimSettings::initializeSettings(R"({"SettingsVersion": 2.0, "SimMode": "Hero",
                "DefaultSensors": {"os_default": {"SensorType": 12, "Enabled": true, "Z": -1}},
                "Vehicles": {
                    "Husky1": {"VehicleType": "PhysXCar", "Sensors": {"os_own": {"SensorType": 12, "Enabled": true}}},
                    "Husky2": {"VehicleType": "PhysXCar"}
                }
            })");
            AirSimSettings::singleton().load(nullptr);

            const auto& vehicles = AirSimSettings::singleton().vehicles;
            const auto& own = vehicles.at("Husky1")->sensors;
            testAssert(own.count("os_own") == 1 && own.count("os_default") == 0,
                       "a vehicle's own Ouster replaces the default one");
            const auto& defaults = vehicles.at("Husky2")->sensors;
            testAssert(defaults.count("os_default") == 1 &&
                           static_cast<const AirSimSettings::OusterSetting&>(*defaults.at("os_default")).position.z() == -1,
                       "a vehicle without an Ouster gets the default one");
        }
    };
}
}
#endif
