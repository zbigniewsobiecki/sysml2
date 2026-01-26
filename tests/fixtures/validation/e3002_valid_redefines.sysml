// Test fixture for E3002: Valid redefines of inherited feature
package TestPackage {
    part def A {
        part x;
    }

    part def B :> A {
        part y :>> x;  // Valid: 'x' exists in A
    }
}
