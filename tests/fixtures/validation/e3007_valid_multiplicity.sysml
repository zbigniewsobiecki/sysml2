// Test fixture for E3007: Valid multiplicity examples
package TestPackage {
    part def Vehicle {
        part wheels[4];       // Single bound (4..4)
        part doors[0..1];     // Optional
        part seats[1..*];     // One or more
        part mirrors[*];      // Any number
        part engines[1..2];   // Range
    }
}
