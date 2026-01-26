// Test fixture for E3002: Redefines non-existent feature
package TestPackage {
    part def A {
        part x;
    }

    part def B :> A {
        part y :>> nonexistent;  // Error: 'nonexistent' not found in A
    }
}
