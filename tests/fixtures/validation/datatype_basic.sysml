// Test basic datatype keyword support
package DataTypeTest {
    datatype Real;
    datatype Integer :> Real;
    attribute def Length :> Real;
    attribute x : Real;
}
