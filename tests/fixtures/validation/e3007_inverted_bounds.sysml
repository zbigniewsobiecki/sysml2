// Test fixture for E3007: Multiplicity lower bound exceeds upper bound
package TestPackage {
    part def Vehicle {
        part wheels[5..2];  // Error: 5 > 2
    }
}
