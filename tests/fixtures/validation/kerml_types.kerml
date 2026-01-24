// KerML Type Definitions Test
package KerMLTypes {
    // Datatype (KerML core)
    datatype Real;
    datatype Integer :> Real;
    datatype Boolean;
    datatype String;

    // Type hierarchy
    type Base;
    type Derived :> Base;

    classifier MyClassifier :> Base;
    class MyClass :> MyClassifier;
    struct MyStruct;

    assoc MyAssoc;
    assoc struct MyAssocStruct;

    metaclass MyMetaclass;

    // Abstract types
    abstract type AbstractType;
    abstract class AbstractClass;
    abstract datatype AbstractDatatype;
}
