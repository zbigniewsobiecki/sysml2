// Test cross-file imports - File B (uses imports)
package B {
    import A::*;
    part car : Engine;
    attribute weight : Real;
}
