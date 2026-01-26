// Test fixture for E3008: Redefinition widens multiplicity
package TestPackage {
    part def A {
        part x[0..5];
    }

    part def B :> A {
        part y :>> x[0..10];  // Error: 10 > 5 (widening)
    }
}
