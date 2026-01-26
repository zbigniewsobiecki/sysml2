// Test fixture for E3008: Valid redefinition with narrowing
package TestPackage {
    part def P;
    part def Q :> P;  // Q is a subtype of P

    part def A {
        part x : P[0..5];
    }

    part def B :> A {
        part y :>> x : Q[1..3];  // Valid: Q :> P and [1..3] ⊆ [0..5]
    }
}
