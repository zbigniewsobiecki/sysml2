// E3005: Indirect circular specialization
package ValidationTest {
    part def A :> B;
    part def B :> C;
    part def C :> A;
}
