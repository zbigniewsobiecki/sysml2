// Simple KerML test file

package Vehicles {

    // Base vehicle class
    abstract class Vehicle {
        feature mass : Real;
        feature maxSpeed : Real;
    }

    // Car specializes Vehicle
    class Car :> Vehicle {
        feature wheels[4] : Wheel;
        feature engine : Engine;
        feature passengers[0..5] : Person;
    }

    // Truck with cargo capacity
    class Truck :> Vehicle {
        feature wheels[6..18] : Wheel;
        feature cargoCapacity : Real;
    }

    // Simple datatypes
    datatype Point {
        feature x : Real;
        feature y : Real;
    }

    datatype Color {
        feature r : Integer;
        feature g : Integer;
        feature b : Integer;
    }

    // Struct example
    struct Configuration {
        feature maxLoad : Real;
        feature enabled : Boolean;
    }

    // Association
    assoc Ownership {
        feature owner[1] : Person;
        feature owned[*] : Vehicle;
    }
}

// Library package with standard types
library package Base {
    abstract class Anything;

    datatype Boolean;
    datatype Integer;
    datatype Real;
    datatype String;
}
