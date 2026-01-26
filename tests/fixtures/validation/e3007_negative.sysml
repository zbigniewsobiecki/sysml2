// Test fixture for E3007: Negative multiplicity bound
package TestPackage {
    part def Vehicle {
        part wheels[-1];  // Error: negative bound
    }
}
