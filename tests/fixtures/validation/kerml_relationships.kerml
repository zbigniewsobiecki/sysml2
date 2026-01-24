// KerML Relationship Elements Test
package KerMLRelationships {
    type A;
    type B;

    subtype B :> A;
    disjoint A from B;

    feature x;
    feature y;

    subset y :> x;
    redefinition y :>> x;

    multiplicity mult [1..*];
    multiplicity single [1];
}
