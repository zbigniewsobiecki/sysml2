// SysML v2 Vehicle System Model

package VehicleSystem {

    import Base::*;

    // Part definitions
    part def Vehicle {
        attribute mass : Real;
        attribute maxSpeed : Real;

        part engine : Engine;
        part transmission : Transmission;
        part wheels[4] : Wheel;

        port fuelPort : FuelPort;
    }

    part def Engine {
        attribute displacement : Real;
        attribute power : Real;
        attribute torque : Real;

        port fuelIn : ~FuelPort;
        port powerOut : PowerPort;
    }

    part def Transmission {
        attribute gearCount : Integer;

        port powerIn : ~PowerPort;
        port powerOut : PowerPort;
    }

    part def Wheel {
        attribute diameter : Real;
        attribute width : Real;

        port powerIn : ~PowerPort;
    }

    // Port definitions
    port def FuelPort {
        in item fuel : Fuel;
    }

    port def PowerPort {
        out attribute power : Real;
        out attribute torque : Real;
    }

    // Interface definition
    interface def PowerInterface {
        end source : PowerPort;
        end sink : ~PowerPort;
    }

    // Item definition
    item def Fuel {
        attribute octaneRating : Integer;
    }

    // Action definition
    action def Drive {
        in vehicle : Vehicle;
        in destination : Location;

        action accelerate;
        action steer;
        action brake;
    }

    // State definition
    state def VehicleStates {
        entry state parked;
        state running {
            entry state idle;
            state moving;
        }
    }

    // Requirement definition
    requirement def PerformanceReq {
        doc /* The vehicle shall meet performance requirements */

        attribute requiredPower : Real;
        attribute requiredSpeed : Real;

        require constraint {
            vehicle.engine.power >= requiredPower
        }
    }

    // Constraint definition
    constraint def PowerConstraint {
        in power : Real;
        in mass : Real;
        in acceleration : Real;

        power >= mass * acceleration
    }
}
