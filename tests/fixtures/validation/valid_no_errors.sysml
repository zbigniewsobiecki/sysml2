// Valid model - should pass without errors
package VehiclePkg {
    attribute def CylinderCount;

    part def Engine {
        attribute cylinders : CylinderCount;
    }

    part def Vehicle {
        part engine : Engine;
    }

    part myCar : Vehicle {
        part frontEngine : Engine;
    }
}
