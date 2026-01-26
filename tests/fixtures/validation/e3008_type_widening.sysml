// Test fixture for E3008: Redefinition with incompatible type
package TestPackage {
    part def P;
    part def Q;  // Q is not a subtype of P

    part def A {
        part x : P;
    }

    part def B :> A {
        part y :>> x : Q;  // Error: Q is not a subtype of P
    }
}
