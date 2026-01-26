// Test fixture for abstract instantiation warning
package TestPackage {
    abstract part def Vehicle;

    part myVehicle : Vehicle;  // Warning: instantiating abstract type
}
